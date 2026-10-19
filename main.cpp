#include "table.h"
#include "sql/Lexer.h"
#include "sql/Parser.h"
#include "sql/Executor.h"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iomanip>
#include <climits>

#ifdef _WIN32
#  include <windows.h>
#endif

using namespace std;

// ============================================================
//  ANSI colour palette
// ============================================================
namespace C {
    const string RST  = "\033[0m";
    const string BOLD = "\033[1m";
    const string DIM  = "\033[2m";
    const string RED  = "\033[31m";
    const string GRN  = "\033[32m";
    const string YLW  = "\033[33m";
    const string BLU  = "\033[34m";
    const string MAG  = "\033[35m";
    const string CYN  = "\033[36m";
    const string WHT  = "\033[97m";
    const string BCYN = "\033[96m";
    const string BGRN = "\033[92m";
    const string BYLW = "\033[93m";
}

void enableAnsiColors() {
#ifdef _WIN32
#  ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

// ============================================================
//  Helpers
// ============================================================

static string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static string stripTrailingSemi(string s) {
    s = trim(s);
    if (!s.empty() && s.back() == ';') s.pop_back();
    return trim(s);
}

static string formatTime(double ms) {
    ostringstream oss;
    oss << fixed << setprecision(2) << ms << " ms";
    return oss.str();
}

// ============================================================
//  Pretty-print result table  (MySQL-style)
// ============================================================

void printResultTable(const vector<Record>& rows) {
    // Column widths (min = header label width)
    int wId   = 2;   // "id"
    int wName = 4;   // "name"
    int wAge  = 3;   // "age"

    for (const auto& r : rows) {
        wId   = max(wId,   (int)to_string(r.id).size());
        wName = max(wName, (int)r.name.size());
        wAge  = max(wAge,  (int)to_string(r.age).size());
    }

    // Build separator line
    auto hline = [&]() -> string {
        return "+" + string(wId + 2, '-') +
               "+" + string(wName + 2, '-') +
               "+" + string(wAge + 2, '-') + "+";
    };

    string sep = hline();

    // Header
    cout << C::CYN << sep << C::RST << "\n";
    cout << C::CYN << "| " << C::RST
         << C::BOLD << C::BCYN << left  << setw(wId)   << "id"   << C::RST
         << C::CYN << " | " << C::RST
         << C::BOLD << C::BCYN << left  << setw(wName)  << "name" << C::RST
         << C::CYN << " | " << C::RST
         << C::BOLD << C::BCYN << right << setw(wAge)   << "age"  << C::RST
         << C::CYN << " |" << C::RST << "\n";
    cout << C::CYN << sep << C::RST << "\n";

    // Rows (alternating subtle tint using DIM for even rows)
    for (size_t i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        string tint = (i % 2 == 0) ? "" : C::DIM;
        cout << C::CYN << "| " << C::RST
             << tint << right << setw(wId)   << r.id   << C::RST
             << C::CYN << " | " << C::RST
             << tint << left  << setw(wName)  << r.name << C::RST
             << C::CYN << " | " << C::RST
             << tint << right << setw(wAge)   << r.age  << C::RST
             << C::CYN << " |" << C::RST << "\n";
    }

    cout << C::CYN << sep << C::RST << "\n";
}

// ============================================================
//  Help
// ============================================================

void printHelp() {
    cout << "\n"
         << C::BOLD << C::BCYN << "  SQL Commands  " << C::RST
         << C::DIM  << "(all statements end with ;)" << C::RST << "\n\n"
         << "  " << C::BYLW << "SELECT" << C::RST << "  * FROM <table>;\n"
         << "  " << C::BYLW << "SELECT" << C::RST << "  * FROM <table> WHERE id = 5;\n"
         << "  " << C::BYLW << "SELECT" << C::RST << "  * FROM <table> WHERE id BETWEEN 10 AND 50;\n"
         << "  " << C::BYLW << "SELECT" << C::RST << "  * FROM <table> WHERE name = 'Alice';\n"
         << "  " << C::BYLW << "SELECT" << C::RST << "  * FROM <table> WHERE age >= 21;\n"
         << "\n"
         << "  " << C::BYLW << "INSERT" << C::RST << "  INTO <table> VALUES (1, 'Alice', 21);\n"
         << "  " << C::BYLW << "UPDATE" << C::RST << "  <table> SET name='Bob', age=25 WHERE id=1;\n"
         << "  " << C::BYLW << "DELETE" << C::RST << "  FROM <table> WHERE id=1;\n"
         << "\n"
         << C::BOLD << C::BCYN << "  Special Commands  " << C::RST << C::DIM << "(no semicolon needed)" << C::RST << "\n\n"
         << "  " << C::BGRN << "show tables" << C::RST << "       – List tables\n"
         << "  " << C::BGRN << "describe users" << C::RST << "    – Show schema\n"
         << "  " << C::BGRN << "stats" << C::RST << "             – Storage & fragmentation stats\n"
         << "  " << C::BGRN << "tree"  << C::RST << "              – Visualise B+ tree index\n"
         << "  " << C::BGRN << "clear" << C::RST << "             – Clear screen\n"
         << "  " << C::BGRN << "help"  << C::RST << "              – Show this help\n"
         << "  " << C::BGRN << "exit"  << C::RST << " / " << C::BGRN << "quit" << C::RST << "       – Exit\n\n";
}

// ============================================================
//  Welcome banner
// ============================================================

void printWelcome(const string& dbFile) {
    cout << "\n"
         << C::BOLD << C::CYN
         << "  ╔════════════════════════════════════════════════╗\n"
         << "  ║                                                ║\n"
         << "  ║    B+Tree  •  Persistent SQL Storage Engine   ║\n"
         << "  ║    Version 1.0  •  Page size: 4096 bytes      ║\n"
         << "  ║                                                ║\n"
         << "  ╚════════════════════════════════════════════════╝\n"
         << C::RST << "\n"
         << "  Database : " << C::BGRN << dbFile << C::RST << "\n"
         << "  Table    : " << C::BCYN << "users" << C::RST
         << C::DIM << "  (id INT PK, name TEXT, age INT)" << C::RST << "\n"
         << "\n"
         << "  Type " << C::BGRN << "help" << C::RST
         << " for available commands, "
         << C::BGRN << "exit" << C::RST << " to quit.\n\n";
}

// ============================================================
//  Special-command handler (returns true if handled)
// ============================================================

bool handleSpecial(const string& raw, Table& table) {
    string cmd = toLower(stripTrailingSemi(raw));

    if (cmd == "exit" || cmd == "quit" || cmd == "\\q") {
        cout << "\n" << C::BYLW << "  Bye!\n\n" << C::RST;
        exit(0);
    }
    if (cmd == "help" || cmd == "\\h" || cmd == "?") {
        printHelp(); return true;
    }
    if (cmd == "clear" || cmd == "\\c") {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
        return true;
    }
    if (cmd == "show tables") {
        cout << C::CYN
             << "+---------+\n"
             << "| Tables  |\n"
             << "+---------+\n"
             << "| users   |\n"
             << "+---------+\n"
             << C::RST
             << C::BGRN << "1 row in set" << C::RST << "\n\n";
        return true;
    }
    if (cmd == "describe users" || cmd == "desc users") {
        cout << C::CYN
             << "+-------+------+-----+\n"
             << "| Field | Type | Key |\n"
             << "+-------+------+-----+\n"
             << "| id    | INT  | PRI |\n"
             << "| name  | TEXT |     |\n"
             << "| age   | INT  |     |\n"
             << "+-------+------+-----+\n"
             << C::RST
             << C::BGRN << "3 rows in set" << C::RST << "\n\n";
        return true;
    }
    if (cmd == "stats") {
        table.printTableStats(); cout << "\n"; return true;
    }
    if (cmd == "tree") {
        table.printIndexTree(); cout << "\n"; return true;
    }
    return false;
}

// ============================================================
//  SQL REPL (default mode)
// ============================================================

void runSQLRepl(const string& dbFile) {
    Table table(dbFile);
    printWelcome(dbFile);

    string accumulated;
    bool   continuation = false;

    while (true) {
        // Print prompt
        if (!continuation)
            cout << C::BOLD << C::BCYN << "bptree" << C::RST
                 << C::CYN  << "> "    << C::RST;
        else
            cout << C::DIM << "    -> " << C::RST;
        cout.flush();

        string line;
        if (!getline(cin, line)) break; // EOF / Ctrl-Z

        string tline = trim(line);

        // --- Handle one-word special commands immediately (no semicolon) ---
        if (!continuation) {
            string lc = toLower(tline);
            // Strip trailing semicolon for keyword check
            string lcs = stripTrailingSemi(lc);
            if (lcs == "exit"   || lcs == "quit"  || lcs == "\\q" ||
                lcs == "help"   || lcs == "\\h"   || lcs == "?"   ||
                lcs == "clear"  || lcs == "\\c"   ||
                lcs == "show tables"               ||
                lcs == "describe users"            ||
                lcs == "desc users"                ||
                lcs == "stats"                     ||
                lcs == "tree") {
                handleSpecial(tline, table);
                continue;
            }
        }

        // Accumulate input
        if (!tline.empty()) {
            if (!accumulated.empty()) accumulated += ' ';
            accumulated += tline;
        }

        if (accumulated.empty()) continue;

        // Check whether we have a complete statement (ends with ';')
        string accTrim = trim(accumulated);
        if (accTrim.empty() || accTrim.back() != ';') {
            continuation = true;
            continue;
        }

        // ---- Complete statement – process it ----
        continuation = false;
        string query = accTrim;
        accumulated.clear();

        // Check multi-word specials with semicolon
        if (handleSpecial(query, table)) continue;

        // Lex → Parse → Execute
        try {
            Lexer     lexer(query);
            auto      tokens = lexer.tokenize();
            Parser    parser(tokens);
            Statement stmt   = parser.parse();
            Executor  exec(table);
            QueryResult res  = exec.execute(stmt);

            if (!res.success) {
                cout << C::RED << "ERROR: " << res.message << C::RST << "\n\n";
            } else if (res.isSelect) {
                if (res.rows.empty()) {
                    cout << C::BYLW << "Empty set"
                         << C::DIM  << " (" << formatTime(res.timeMs) << ")" << C::RST << "\n\n";
                } else {
                    printResultTable(res.rows);
                    cout << "\n"
                         << C::BGRN << res.rows.size() << " row(s) in set"
                         << C::DIM  << " (" << formatTime(res.timeMs) << ")" << C::RST << "\n\n";
                }
            } else {
                cout << C::BGRN << "Query OK, " << res.affectedRows << " row(s) affected"
                     << C::DIM  << " (" << formatTime(res.timeMs) << ")" << C::RST << "\n\n";
            }
        } catch (const ParseError& e) {
            cout << C::RED << "Parse Error: " << e.what() << C::RST << "\n\n";
        } catch (const exception& e) {
            cout << C::RED << "Error: " << e.what() << C::RST << "\n\n";
        }
    }

    cout << "\n" << C::BYLW << "  Bye!\n\n" << C::RST;
}

// ============================================================
//  Original automated tests (kept intact)
// ============================================================

void runAutomatedTests() {
    cout << "\n======================================================\n";
    cout << "        RUNNING COMPREHENSIVE AUTOMATED TESTS         \n";
    cout << "======================================================\n";

    const string testDb = "test_verification.db";
    remove(testDb.c_str());

    {
        cout << "\n[Test 1] Testing Record Insertions & B+ Tree Splits...\n";
        Table table(testDb);

        vector<Record> sampleRecords = {
            {10, "Alice", 21}, {20, "Bob", 22},   {30, "Charlie", 23},
            {40, "David", 24}, {50, "Eve", 25},   {60, "Frank", 26},
            {70, "Grace", 27}, {80, "Heidi", 28}, {90, "Ivan", 29},
            {100,"Judy",  30}
        };

        for (const auto& rec : sampleRecords) {
            bool ok = table.insert(rec);
            assert(ok);
            cout << "  Inserted: ID=" << rec.id
                 << ", Name=" << rec.name << ", Age=" << rec.age << "\n";
        }

        cout << "\n--> Verifying Point Queries (GET)...";
        for (const auto& rec : sampleRecords) {
            Record out;
            bool found = table.get(rec.id, out);
            assert(found); assert(out == rec);
        }
        cout << " PASSED!\n";

        cout << "\n--> Visualizing Tree after Insertions & Splits:";
        table.printIndexTree();
        table.printTableStats();

        cout << "\n[Test 2] Testing Range Queries...\n";
        auto rangeRes = table.rangeQuery(25, 75);
        cout << "  Range [25, 75] returned " << rangeRes.size() << " records:\n";
        for (const auto& r : rangeRes)
            cout << "    ID=" << r.id << ", Name=" << r.name << ", Age=" << r.age << "\n";
        assert(rangeRes.size() == 5);
        assert(rangeRes[0].id == 30);
        assert(rangeRes[4].id == 70);
        cout << "  Range Query PASSED!\n";

        cout << "\n[Test 3] Testing In-Place & Expanded Updates...\n";
        Record updatedBob{20, "Robert", 23};
        bool updated = table.update(20, updatedBob);
        assert(updated);
        Record checkBob;
        bool foundBob = table.get(20, checkBob);
        assert(foundBob);
        assert(checkBob.name == "Robert" && checkBob.age == 23);
        cout << "  Update PASSED! New Record: ID=" << checkBob.id
             << ", Name=" << checkBob.name << "\n";

        cout << "\n[Test 4] Testing Slotted Page Fragmentation & Compaction...\n";
        assert(table.remove(30)); assert(table.remove(50)); assert(table.remove(70));
        cout << "  Deleted records 30, 50, 70 to generate fragmented slots.\n";
        Record dummy;
        assert(!table.get(30, dummy)); assert(!table.get(50, dummy)); assert(!table.get(70, dummy));
        table.printTableStats();

        Record newR1{35, "CompactionTestRecordWithLongName", 33};
        assert(table.insert(newR1));
        Record checkR1;
        assert(table.get(35, checkR1));
        assert(checkR1.name == "CompactionTestRecordWithLongName");
        cout << "  Reused/compacted slot successfully for ID=35!\n";

        cout << "\n[Test 5] Testing B+ Tree Deletions, Underflows, & Merges...\n";
        assert(table.remove(10)); assert(table.remove(20)); assert(table.remove(40));
        cout << "  Deleted keys 10, 20, 40 to trigger borrowing and leaf/internal merges.\n";
        table.printIndexTree();
        table.printTableStats();
    }

    cout << "\n[Test 6] Testing Complete Disk Persistence Across Restart...\n";
    {
        Table table(testDb);
        cout << "  Database reopened successfully from disk file: " << testDb << "\n";
        Record r;
        assert(table.get(60, r)); assert(r.name == "Frank");
        assert(table.get(80, r)); assert(r.name == "Heidi");
        assert(table.get(35, r)); assert(r.name == "CompactionTestRecordWithLongName");
        cout << "  Verified surviving records after reopen:\n";
        cout << "    ID=60: " << r.name << "\n";
        auto res = table.rangeQuery(0, 200);
        cout << "  Total surviving records across all pages: " << res.size() << "\n";
        for (const auto& rec : res)
            cout << "    ID=" << rec.id << ", Name=" << rec.name << ", Age=" << rec.age << "\n";
        cout << "  Persistence Test PASSED!\n";
    }

    cout << "\n======================================================\n";
    cout << "             ALL AUTOMATED TESTS PASSED!              \n";
    cout << "======================================================\n\n";
}

// ============================================================
//  Legacy text menu (--menu flag)
// ============================================================

void runMenu(const string& dbFile) {
    Table table(dbFile);
    cout << "Connected to database: " << dbFile << "\n";

    while (true) {
        cout << "\n=================== B+ Tree Storage Engine ===================\n"
             << " 1. Insert Record\n 2. Update Record\n 3. Delete Record\n"
             << " 4. Point Query\n 5. Range Query\n 6. Storage Stats\n"
             << " 7. Visualize B+ Tree\n 8. Run Automated Tests\n 9. Exit\n"
             << "==============================================================\n"
             << "Enter your choice (1-9): ";

        int choice;
        if (!(cin >> choice)) break;

        if (choice == 1) {
            int id, age; string name;
            cout << "Enter ID: "; cin >> id;
            cout << "Enter Name: "; cin >> name;
            cout << "Enter Age: "; cin >> age;
            cout << (table.insert({id, name, age}) ? ">> Inserted.\n" : ">> Failed.\n");
        } else if (choice == 2) {
            int id, age; string name;
            cout << "Enter ID to update: "; cin >> id;
            cout << "New Name: "; cin >> name;
            cout << "New Age: "; cin >> age;
            cout << (table.update(id, {id, name, age}) ? ">> Updated.\n" : ">> Not found.\n");
        } else if (choice == 3) {
            int id; cout << "Enter ID: "; cin >> id;
            cout << (table.remove(id) ? ">> Deleted.\n" : ">> Not found.\n");
        } else if (choice == 4) {
            int id; cout << "Enter ID: "; cin >> id;
            Record r;
            if (table.get(id, r)) cout << ">> ID=" << r.id << " Name=" << r.name << " Age=" << r.age << "\n";
            else cout << ">> Not found.\n";
        } else if (choice == 5) {
            int lo, hi;
            cout << "Low: "; cin >> lo; cout << "High: "; cin >> hi;
            auto res = table.rangeQuery(lo, hi);
            cout << ">> " << res.size() << " records\n";
            for (auto& r : res) cout << "  [" << r.id << "] " << r.name << ", " << r.age << "\n";
        } else if (choice == 6) { table.printTableStats();
        } else if (choice == 7) { table.printIndexTree();
        } else if (choice == 8) { runAutomatedTests();
        } else if (choice == 9) { cout << "Exiting.\n"; break;
        } else { cout << "Invalid choice.\n"; }
    }
}

// ============================================================
//  main
// ============================================================

int main(int argc, char* argv[]) {
    enableAnsiColors();

    const string dbFile = "database.db";

    for (int i = 1; i < argc; ++i) {
        string arg(argv[i]);
        if (arg == "--test") { runAutomatedTests(); return 0; }
        if (arg == "--menu") { runMenu(dbFile);     return 0; }
    }

    // Default: SQL REPL
    runSQLRepl(dbFile);
    return 0;
}