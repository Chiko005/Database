/**
 * api.js – HTTP client for the Express backend
 */

const BASE = 'http://localhost:3001/api';

async function request(method, path, body) {
  const opts = {
    method,
    headers: { 'Content-Type': 'application/json' },
  };
  if (body !== undefined) opts.body = JSON.stringify(body);

  const res = await fetch(`${BASE}${path}`, opts);
  const data = await res.json();
  return data;
}

export const api = {
  health:       ()      => request('GET',  '/health'),
  tables:       ()      => request('GET',  '/tables'),
  schema:       (table) => request('GET',  `/tables/${encodeURIComponent(table)}/schema`),
  tableRows:    (table) => request('GET',  `/tables/${encodeURIComponent(table)}/rows`),
  stats:        ()      => request('GET',  '/stats'),
  query:        (sql)   => request('POST', '/query', { sql }),
};
