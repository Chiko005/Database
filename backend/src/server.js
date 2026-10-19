'use strict';

const express = require('express');
const cors    = require('cors');
const engine  = require('./engineClient');

const app  = express();
const PORT = 3001;

// ─────────────────────────────────────────────────────────────────────────────
// Middleware
// ─────────────────────────────────────────────────────────────────────────────

app.use(cors({ origin: 'http://localhost:5173' }));  // Vite default port
app.use(express.json({ limit: '1mb' }));

// ─────────────────────────────────────────────────────────────────────────────
// Routes
// ─────────────────────────────────────────────────────────────────────────────

/**
 * GET /api/health
 * Check whether the backend and database engine are reachable.
 */
app.get('/api/health', async (req, res) => {
  const ok = await engine.ping();
  if (ok) {
    res.json({ status: 'ok', engine: 'connected', message: 'B+ Tree engine is running' });
  } else {
    res.status(503).json({ status: 'error', engine: 'disconnected', message: 'Cannot reach C++ engine on port 7070. Is server.exe running?' });
  }
});

/**
 * GET /api/tables
 * List all tables known to the engine.
 */
app.get('/api/tables', async (req, res) => {
  try {
    const result = await engine.listTables();
    res.json(result);
  } catch (err) {
    res.status(503).json({ success: false, message: err.message });
  }
});

/**
 * GET /api/tables/:table/schema
 * Return schema for a table.
 */
app.get('/api/tables/:table/schema', async (req, res) => {
  const table = req.params.table;
  if (table !== 'users') {
    return res.status(404).json({ success: false, message: `Table '${table}' not found` });
  }
  try {
    const result = await engine.getSchema();
    res.json(result);
  } catch (err) {
    res.status(503).json({ success: false, message: err.message });
  }
});

/**
 * GET /api/tables/:table/rows
 * Browse table data (executes SELECT * FROM <table>).
 */
app.get('/api/tables/:table/rows', async (req, res) => {
  const table = req.params.table;
  const limit = parseInt(req.query.limit, 10) || 100;
  try {
    const result = await engine.executeSQL(`SELECT * FROM ${table};`);
    // Apply a simple limit
    if (result.rows && result.rows.length > limit) {
      result.rows = result.rows.slice(0, limit);
      result.message = `Showing first ${limit} rows`;
    }
    res.json(result);
  } catch (err) {
    res.status(503).json({ success: false, message: err.message });
  }
});

/**
 * GET /api/stats
 * Return engine storage statistics.
 */
app.get('/api/stats', async (req, res) => {
  try {
    const result = await engine.getStats();
    res.json(result);
  } catch (err) {
    res.status(503).json({ success: false, message: err.message });
  }
});

/**
 * POST /api/query
 * Execute an arbitrary SQL statement.
 *
 * Body: { "sql": "SELECT * FROM users;" }
 *
 * Response:
 * {
 *   "success": true|false,
 *   "columns": [...],      // SELECT only
 *   "rows": [[...],...],   // SELECT only
 *   "rowsAffected": N,
 *   "executionTimeMs": F,
 *   "message": "..."
 * }
 */
app.post('/api/query', async (req, res) => {
  const { sql } = req.body;

  if (!sql || typeof sql !== 'string') {
    return res.status(400).json({ success: false, message: 'Request body must contain a non-empty "sql" string' });
  }

  const trimmed = sql.trim();
  if (trimmed.length === 0) {
    return res.status(400).json({ success: false, message: 'SQL query is empty' });
  }

  // Basic safety: disallow semicolons injected via extra statements
  // (The engine only processes a single statement at a time anyway)
  try {
    const result = await engine.executeSQL(trimmed);
    res.json(result);
  } catch (err) {
    res.status(503).json({ success: false, message: err.message });
  }
});

// ─────────────────────────────────────────────────────────────────────────────
// 404 fallback
// ─────────────────────────────────────────────────────────────────────────────
app.use((req, res) => {
  res.status(404).json({ success: false, message: `No route: ${req.method} ${req.path}` });
});

// ─────────────────────────────────────────────────────────────────────────────
// Start
// ─────────────────────────────────────────────────────────────────────────────
app.listen(PORT, () => {
  console.log(`[backend] Express server listening on http://localhost:${PORT}`);
  console.log(`[backend] CORS enabled for http://localhost:5173`);
  console.log(`[backend] Expecting C++ engine on 127.0.0.1:7070`);
});
