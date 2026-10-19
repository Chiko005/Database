// server.cpp  –  TCP JSON gateway for the B+ Tree SQL engine
// Listens on 127.0.0.1:7070
// Protocol: length-prefixed JSON messages
//   Request:  4-byte little-endian uint32 length  +  JSON body
//   Response: 4-byte little-endian uint32 length  +  JSON body
//
// JSON request  { "sql": "SELECT * FROM users;" }
// JSON request  { "cmd": "tables" }          → list tables
// JSON request  { "cmd": "schema" }          → schema of 'users' table
// JSON request  { "cmd": "stats"  }          → engine stats
//
// JSON response (success)
// {
//   "success": true,
//   "columns": [...],     // only for SELECT
//   "rows": [[...],...],  // only for SELECT
//   "rowsAffected": N,
//   "executionTimeMs": F,
//   "message": "..."
// }
// JSON response (error)
// { "success": false, "message": "..." }

#include "table.h"
#include "sql/Lexer.h"
#include "sql/Parser.h"
#include "sql/Executor.h"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <iomanip>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   typedef SOCKET sock_t;
#  define SOCK_INVALID INVALID_SOCKET
#  define SOCK_ERR     SOCKET_ERROR
#  define sock_close   closesocket
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
   typedef int sock_t;
#  define SOCK_INVALID (-1)
#  define SOCK_ERR     (-1)
#  define sock_close   close
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Minimal JSON helpers (no external libs)
// ─────────────────────────────────────────────────────────────────────────────

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else {
            out += c;
        }
    }
    return out;
}

// Very small JSON parser – only what we need (extract "sql" / "cmd" strings)
static bool jsonGetString(const std::string& json, const std::string& key, std::string& out) {
    // Look for "key":
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    // skip whitespace + colon
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == ':')) ++pos;
    if (pos >= json.size() || json[pos] != '"') return false;
    ++pos; // skip opening quote
    std::string val;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            if      (json[pos] == '"')  { val += '"';  ++pos; }
            else if (json[pos] == '\\') { val += '\\'; ++pos; }
            else if (json[pos] == 'n')  { val += '\n'; ++pos; }
            else if (json[pos] == 'r')  { val += '\r'; ++pos; }
            else if (json[pos] == 't')  { val += '\t'; ++pos; }
            else                        { val += json[pos++]; }
        } else {
            val += json[pos++];
        }
    }
    out = val;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Response builders
// ─────────────────────────────────────────────────────────────────────────────

static std::string errorJson(const std::string& msg) {
    return "{\"success\":false,\"message\":\"" + jsonEscape(msg) + "\"}";
}

static std::string buildQueryResponse(const QueryResult& res) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(4);
    o << "{";
    o << "\"success\":" << (res.success ? "true" : "false") << ",";
    if (res.isSelect) {
        // columns
        o << "\"columns\":[\"id\",\"name\",\"age\"],";
        // rows
        o << "\"rows\":[";
        for (size_t i = 0; i < res.rows.size(); ++i) {
            if (i) o << ",";
            const auto& r = res.rows[i];
            o << "[" << r.id
              << ",\"" << jsonEscape(r.name) << "\""
              << "," << r.age << "]";
        }
        o << "],";
    }
    o << "\"rowsAffected\":" << res.affectedRows << ",";
    o << "\"executionTimeMs\":" << res.timeMs << ",";
    o << "\"message\":\"" << jsonEscape(res.message) << "\"";
    o << "}";
    return o.str();
}

static std::string buildTablesJson() {
    return "{\"success\":true,\"tables\":[\"users\"],\"message\":\"OK\"}";
}

static std::string buildSchemaJson() {
    return R"({"success":true,"table":"users","columns":[)"
           R"({"name":"id","type":"INT","key":"PRI"},)"
           R"({"name":"name","type":"TEXT","key":""},)"
           R"({"name":"age","type":"INT","key":""})"
           R"(],"indexes":["users_id_idx"],"message":"OK"})";
}

static std::string buildStatsJson(Table& table) {
    // Capture printTableStats output
    // Since it writes to cout, redirect or just return static info
    std::ostringstream o;
    o << "{\"success\":true,";
    o << "\"engine\":\"B+Tree\",";
    o << "\"pageSize\":4096,";
    o << "\"totalPages\":" << table.getDiskManager().getTotalPages() << ",";
    o << "\"cachedPages\":" << table.getDiskManager().cachedPages() << ",";
    o << "\"walLSN\":" << table.getDiskManager().walLSN() << ",";
    o << "\"message\":\"OK\"}";
    return o.str();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Network helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool recvAll(sock_t fd, char* buf, uint32_t len) {
    uint32_t got = 0;
    while (got < len) {
        int n = recv(fd, buf + got, (int)(len - got), 0);
        if (n <= 0) return false;
        got += (uint32_t)n;
    }
    return true;
}

static bool sendAll(sock_t fd, const char* buf, uint32_t len) {
    uint32_t sent = 0;
    while (sent < len) {
        int n = send(fd, buf + sent, (int)(len - sent), 0);
        if (n <= 0) return false;
        sent += (uint32_t)n;
    }
    return true;
}

static bool readMsg(sock_t fd, std::string& out) {
    uint32_t len = 0;
    if (!recvAll(fd, (char*)&len, 4)) return false;
    // little-endian on x86; fine
    if (len > 4 * 1024 * 1024) return false; // 4 MB sanity
    out.resize(len);
    return recvAll(fd, &out[0], len);
}

static bool writeMsg(sock_t fd, const std::string& msg) {
    uint32_t len = (uint32_t)msg.size();
    if (!sendAll(fd, (char*)&len, 4)) return false;
    return sendAll(fd, msg.data(), len);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Handle one connected client (single-threaded – one client at a time)
// ─────────────────────────────────────────────────────────────────────────────

static void handleClient(sock_t client, Table& table) {
    while (true) {
        std::string req;
        if (!readMsg(client, req)) break;

        std::string resp;

        // Check for cmd field
        std::string cmd;
        if (jsonGetString(req, "cmd", cmd)) {
            if (cmd == "tables") {
                resp = buildTablesJson();
            } else if (cmd == "schema") {
                resp = buildSchemaJson();
            } else if (cmd == "stats") {
                resp = buildStatsJson(table);
            } else if (cmd == "ping") {
                resp = "{\"success\":true,\"message\":\"pong\"}";
            } else {
                resp = errorJson("Unknown command: " + cmd);
            }
        } else {
            // SQL query
            std::string sql;
            if (!jsonGetString(req, "sql", sql)) {
                resp = errorJson("Request must contain 'sql' or 'cmd' field");
            } else {
                try {
                    Lexer       lexer(sql);
                    auto        tokens = lexer.tokenize();
                    Parser      parser(tokens);
                    Statement   stmt   = parser.parse();
                    Executor    exec(table);
                    QueryResult res    = exec.execute(stmt);

                    if (res.success && !res.isSelect && res.message.empty()) {
                        res.message = "Query OK, " + std::to_string(res.affectedRows) + " row(s) affected";
                    }
                    resp = buildQueryResponse(res);
                } catch (const ParseError& e) {
                    resp = errorJson(std::string("Parse error: ") + e.what());
                } catch (const std::exception& e) {
                    resp = errorJson(std::string("Engine error: ") + e.what());
                } catch (...) {
                    resp = errorJson("Unknown engine error");
                }
            }
        }

        if (!writeMsg(client, resp)) break;
    }
    sock_close(client);
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────

int main() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    const char*    DB_FILE = "database.db";
    const uint16_t PORT    = 7070;

    Table table(DB_FILE);
    std::cout << "[server] B+ Tree engine loaded from " << DB_FILE << "\n";
    std::cout << "[server] Listening on 127.0.0.1:" << PORT << "\n";
    std::cout.flush();

    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == SOCK_INVALID) { std::cerr << "socket() failed\n"; return 1; }

    int opt = 1;
#ifdef _WIN32
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERR) {
        std::cerr << "[server] bind() failed – is port " << PORT << " already in use?\n";
        sock_close(srv);
        return 1;
    }
    if (listen(srv, 4) == SOCK_ERR) {
        std::cerr << "listen() failed\n";
        sock_close(srv);
        return 1;
    }

    while (true) {
        sockaddr_in cli{};
#ifdef _WIN32
        int clen = sizeof(cli);
#else
        socklen_t clen = sizeof(cli);
#endif
        sock_t client = accept(srv, (sockaddr*)&cli, &clen);
        if (client == SOCK_INVALID) {
            std::cerr << "[server] accept() error\n";
            continue;
        }
        std::cout << "[server] Client connected\n"; std::cout.flush();
        handleClient(client, table);
        std::cout << "[server] Client disconnected\n"; std::cout.flush();
    }

    sock_close(srv);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
