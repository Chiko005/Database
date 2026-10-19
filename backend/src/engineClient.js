/**
 * engineClient.js – TCP client that talks to the C++ B+ Tree engine server
 *
 * Protocol: 4-byte little-endian uint32 length prefix + JSON body
 *
 * The C++ server is single-threaded (one client at a time).
 * We maintain a persistent TCP connection and serialize requests through
 * a queue so concurrent HTTP requests never overlap on the socket.
 */

'use strict';

const net = require('net');

const ENGINE_HOST = '127.0.0.1';
const ENGINE_PORT = 7070;
const CONNECT_TIMEOUT_MS = 5000;
const REQUEST_TIMEOUT_MS = 30000;

let socket = null;
let connected = false;
let connecting = false;

// Incoming data buffer
let recvBuf = Buffer.alloc(0);

// Pending request queue: each entry is { resolve, reject, timer }
const pending = [];

// ─────────────────────────────────────────────────────────────────────────────
// Low-level TCP helpers
// ─────────────────────────────────────────────────────────────────────────────

function buildMessage(obj) {
  const body = Buffer.from(JSON.stringify(obj), 'utf8');
  const header = Buffer.alloc(4);
  header.writeUInt32LE(body.length, 0);
  return Buffer.concat([header, body]);
}

function tryResolveMessages() {
  while (true) {
    if (recvBuf.length < 4) return;
    const len = recvBuf.readUInt32LE(0);
    if (recvBuf.length < 4 + len) return;

    const body = recvBuf.slice(4, 4 + len).toString('utf8');
    recvBuf = recvBuf.slice(4 + len);

    if (pending.length === 0) {
      console.warn('[engine] Received unsolicited message – ignored');
      return;
    }

    const { resolve, reject, timer } = pending.shift();
    clearTimeout(timer);

    let parsed;
    try {
      parsed = JSON.parse(body);
    } catch (e) {
      reject(new Error('Engine returned invalid JSON: ' + body.slice(0, 200)));
      return;
    }
    resolve(parsed);
  }
}

function rejectAll(reason) {
  while (pending.length > 0) {
    const { reject, timer } = pending.shift();
    clearTimeout(timer);
    reject(new Error(reason));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Connection management
// ─────────────────────────────────────────────────────────────────────────────

function createSocket() {
  const s = new net.Socket();

  s.on('data', (chunk) => {
    recvBuf = Buffer.concat([recvBuf, chunk]);
    tryResolveMessages();
  });

  s.on('error', (err) => {
    console.error('[engine] Socket error:', err.message);
    connected = false;
    socket = null;
    rejectAll('Engine socket error: ' + err.message);
  });

  s.on('close', () => {
    console.warn('[engine] Connection closed');
    connected = false;
    socket = null;
    rejectAll('Engine connection closed');
  });

  return s;
}

/**
 * Ensure a connection to the engine exists.
 * Returns a promise that resolves when connected (or rejects on failure).
 */
function ensureConnected() {
  if (connected && socket && !socket.destroyed) return Promise.resolve();

  if (connecting) {
    // Wait for the existing attempt
    return new Promise((resolve, reject) => {
      const check = setInterval(() => {
        if (connected) { clearInterval(check); resolve(); }
        else if (!connecting) { clearInterval(check); reject(new Error('Connection failed')); }
      }, 50);
    });
  }

  connecting = true;
  socket = createSocket();
  recvBuf = Buffer.alloc(0);

  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      connecting = false;
      socket.destroy();
      reject(new Error(`Cannot connect to engine on port ${ENGINE_PORT} (timeout)`));
    }, CONNECT_TIMEOUT_MS);

    socket.connect(ENGINE_PORT, ENGINE_HOST, () => {
      clearTimeout(timer);
      connected = true;
      connecting = false;
      console.log('[engine] Connected to C++ engine');
      resolve();
    });

    socket.once('error', (err) => {
      clearTimeout(timer);
      connecting = false;
      connected = false;
      reject(new Error(`Cannot connect to engine: ${err.message}`));
    });
  });
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Send a JSON request to the engine and return the parsed JSON response.
 * Requests are serialized — no two overlap on the socket.
 */
async function send(request) {
  await ensureConnected();

  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      // Remove from queue (it's still there if we timed out before the server responded)
      const idx = pending.findIndex(p => p.timer === timer);
      if (idx !== -1) pending.splice(idx, 1);
      reject(new Error('Engine request timed out'));
    }, REQUEST_TIMEOUT_MS);

    pending.push({ resolve, reject, timer });
    socket.write(buildMessage(request));
  });
}

async function executeSQL(sql) {
  return send({ sql });
}

async function listTables() {
  return send({ cmd: 'tables' });
}

async function getSchema() {
  return send({ cmd: 'schema' });
}

async function getStats() {
  return send({ cmd: 'stats' });
}

async function ping() {
  try {
    const r = await send({ cmd: 'ping' });
    return r.success === true;
  } catch {
    return false;
  }
}

module.exports = { executeSQL, listTables, getSchema, getStats, ping, ensureConnected };
