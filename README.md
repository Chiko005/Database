# B+ Studio

A modern database management GUI for your custom **B+ Tree SQL Engine** written in C++.

```
┌──────────────────────────────────────────┐
│              B+ Studio                   │
│  ┌──────────┐ ┌──────────────────────┐   │
│  │ Explorer │ │   Monaco SQL Editor  │   │
│  │          │ ├──────────────────────┤   │
│  │ Tables   │ │   Results / Messages │   │
│  └──────────┘ └──────────────────────┘   │
└──────────────────────────────────────────┘
```

## Architecture

```
React (Vite)  ←HTTP→  Express (Node.js)  ←TCP/JSON→  server.exe (C++ B+Tree)
  :5173                  :3001                            :7070
```

## Prerequisites

- **g++** (GCC ≥ 10 with C++17 support, available in PATH)  
- **Node.js** v18+ and **npm**  
- Windows (tested; Linux/macOS work with minor path adjustments)

---

## 1. Build the C++ Engine Server

```powershell
cd "C:\Users\asus\Desktop\B+tree"

g++ -std=c++17 -O2 -o server.exe `
    server.cpp buffer_pool.cpp diskmanager.cpp persistent.cpp table.cpp wal.cpp `
    sql/Lexer.cpp sql/Parser.cpp sql/Executor.cpp `
    -lws2_32
```

## 2. Start Everything (3 terminals)

### Terminal 1 — C++ Engine Server
```powershell
cd "C:\Users\asus\Desktop\B+tree"
.\server.exe
# Prints: [server] Listening on 127.0.0.1:7070
```

### Terminal 2 — Express Backend
```powershell
cd "C:\Users\asus\Desktop\B+tree\backend"
npm start
# Prints: [backend] Express server listening on http://localhost:3001
```

### Terminal 3 — React Frontend
```powershell
cd "C:\Users\asus\Desktop\B+tree\frontend"
npm run dev
# Open: http://localhost:5173
```

---

## Supported SQL Syntax

The C++ engine supports a fixed schema (`users` table: `id INT PK, name TEXT, age INT`).

| Statement | Example |
|-----------|---------|
| SELECT all | `SELECT * FROM users;` |
| SELECT with WHERE | `SELECT * FROM users WHERE id = 5;` |
| SELECT with range | `SELECT * FROM users WHERE id BETWEEN 10 AND 50;` |
| SELECT by name | `SELECT * FROM users WHERE name = 'Alice';` |
| SELECT by age | `SELECT * FROM users WHERE age >= 21;` |
| INSERT | `INSERT INTO users VALUES (1, 'Alice', 21);` |
| UPDATE | `UPDATE users SET name='Bob', age=25 WHERE id=1;` |
| DELETE | `DELETE FROM users WHERE id=1;` |

> **Note:** `CREATE TABLE` is not yet supported by the SQL parser. The `users` table is built-in with a fixed schema.

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/health` | Engine connectivity check |
| `GET` | `/api/tables` | List tables |
| `GET` | `/api/tables/:table/schema` | Table schema + indexes |
| `GET` | `/api/tables/:table/rows` | Browse rows |
| `GET` | `/api/stats` | Engine storage stats |
| `POST` | `/api/query` | Execute SQL: `{ "sql": "..." }` |

## Engine Architecture (C++ side)

| Component | File | Description |
|-----------|------|-------------|
| B+ Tree Index | `persistent.cpp` | Leaf/internal nodes with disk serialization |
| Disk Manager | `diskmanager.cpp` | Page I/O, allocation, WAL integration |
| Buffer Pool | `buffer_pool.cpp` | LRU page cache (64 pages default) |
| WAL | `wal.cpp` | Redo-only write-ahead log for crash recovery |
| Table | `table.cpp` | CRUD operations using B+ Tree + DiskManager |
| Lexer | `sql/Lexer.cpp` | SQL tokenizer |
| Parser | `sql/Parser.cpp` | Recursive descent parser for DML |
| Executor | `sql/Executor.cpp` | Query execution against the Table API |
| TCP Server | `server.cpp` | JSON-over-TCP gateway (port 7070) |

## Known Limitations

- Only `users` table with fixed schema `(id INT, name TEXT, age INT)`.
- No `CREATE TABLE` / `DROP TABLE` support yet.
- No `JOIN`, subqueries, or aggregates.
- Single-threaded TCP server — one query at a time.
- No transaction isolation (single writer; WAL provides crash recovery only).

## Project Structure

```
B+tree/
├── *.cpp / *.h            ← C++ engine source
├── server.cpp             ← TCP JSON gateway
├── server.exe             ← compiled gateway
├── database.db            ← persistent data file
├── Makefile
├── backend/
│   ├── package.json
│   └── src/
│       ├── server.js      ← Express REST API
│       └── engineClient.js ← TCP client for C++ server
├── frontend/
│   ├── package.json
│   └── src/
│       ├── App.jsx
│       ├── index.css      ← Design system
│       ├── components/
│       │   ├── Navbar.jsx
│       │   ├── DatabaseExplorer.jsx
│       │   ├── QueryEditor.jsx  (Monaco)
│       │   ├── ResultsGrid.jsx
│       │   └── MessagesPanel.jsx
│       └── services/
│           └── api.js
└── README.md
```
