import { useState, useEffect, useCallback, useRef } from 'react';
import { api } from './services/api';
import Navbar from './components/Navbar';
import DatabaseExplorer from './components/DatabaseExplorer';
import QueryEditor from './components/QueryEditor';
import ResultsGrid from './components/ResultsGrid';
import MessagesPanel from './components/MessagesPanel';
import './index.css';

const SAMPLE_SQL = `-- Welcome to B+ Studio
-- Your B+ Tree SQL Engine is connected
-- Press Ctrl+Enter or click Run Query to execute

SELECT * FROM users;
`;

let tabCounter = 2;
const initialTabs = [{ id: 1, title: 'Query 1', sql: SAMPLE_SQL }];

function addMessage(setMessages, type, text) {
  setMessages(prev => [...prev, { type, text, timestamp: Date.now() }]);
}

export default function App() {
  // ── Engine connection state ──────────────────────────────────────────────
  const [engineStatus, setEngineStatus] = useState('unknown'); // unknown | connected | disconnected
  const [tables, setTables]   = useState([]);
  const [schema, setSchema]   = useState(null);
  const [selectedTable, setSelectedTable] = useState(null);
  const [explorerLoading, setExplorerLoading] = useState(false);

  // ── Editor tabs ──────────────────────────────────────────────────────────
  const [tabs, setTabs]       = useState(initialTabs);
  const [activeTab, setActiveTab] = useState(1);

  // ── Results ──────────────────────────────────────────────────────────────
  const [result, setResult]       = useState(null);
  const [messages, setMessages]   = useState([]);
  const [running, setRunning]     = useState(false);
  const [resultsTab, setResultsTab] = useState('data'); // 'data' | 'messages'

  // ── Load health + tables on mount ────────────────────────────────────────
  const checkHealth = useCallback(async () => {
    try {
      const h = await api.health();
      if (h.status === 'ok') {
        setEngineStatus('connected');
        return true;
      } else {
        setEngineStatus('disconnected');
        addMessage(setMessages, 'error', h.message || 'Cannot reach C++ engine');
        return false;
      }
    } catch (e) {
      setEngineStatus('disconnected');
      addMessage(setMessages, 'error', 'Backend unreachable — is the Express server running on port 3001?');
      return false;
    }
  }, []);

  const loadTables = useCallback(async () => {
    setExplorerLoading(true);
    try {
      const r = await api.tables();
      if (r.success) setTables(r.tables ?? []);
    } catch {}
    setExplorerLoading(false);
  }, []);

  useEffect(() => {
    (async () => {
      const ok = await checkHealth();
      if (ok) loadTables();
    })();
  }, []);

  // ── Select a table ────────────────────────────────────────────────────────
  async function handleSelectTable(table) {
    setSelectedTable(table);
    try {
      const s = await api.schema(table);
      if (s.success) setSchema(s);
    } catch {}
  }

  // ── Run query ─────────────────────────────────────────────────────────────
  async function handleRun(sql) {
    const trimmed = (sql ?? '').trim();
    if (!trimmed) {
      addMessage(setMessages, 'info', 'Nothing to execute — editor is empty');
      setResultsTab('messages');
      return;
    }
    if (running) return;

    setRunning(true);
    setResult(null);
    setResultsTab('data');

    try {
      const res = await api.query(trimmed);
      setResult(res);

      if (res.success) {
        if (res.isSelect || (res.columns && res.columns.length > 0)) {
          const rowCount = res.rows?.length ?? 0;
          addMessage(setMessages, 'success',
            `Query OK — ${rowCount} row(s) returned in ${(res.executionTimeMs ?? 0).toFixed(2)} ms`);
        } else {
          addMessage(setMessages, 'success',
            res.message || `Query OK — ${res.rowsAffected ?? 0} row(s) affected`);
          // Refresh tables in case new data was inserted
          loadTables();
        }
      } else {
        addMessage(setMessages, 'error', res.message || 'Query failed');
        setResultsTab('messages');
      }
    } catch (e) {
      addMessage(setMessages, 'error', `Network error: ${e.message}`);
      setResultsTab('messages');
    }

    setRunning(false);
  }

  // ── Tab management ────────────────────────────────────────────────────────
  function handleTabAdd() {
    const id = tabCounter++;
    setTabs(prev => [...prev, { id, title: `Query ${id}`, sql: '' }]);
    setActiveTab(id);
  }
  function handleTabClose(id) {
    setTabs(prev => {
      const next = prev.filter(t => t.id !== id);
      if (next.length === 0) return prev;
      if (activeTab === id) setActiveTab(next[next.length - 1].id);
      return next;
    });
  }
  function handleTabSqlChange(id, sql) {
    setTabs(prev => prev.map(t => t.id === id ? { ...t, sql } : t));
  }

  function handleClear() {
    setResult(null);
  }

  return (
    <div className="app-shell">
      <Navbar
        engineStatus={engineStatus}
        onRefresh={() => { checkHealth().then(ok => ok && loadTables()); }}
      />

      <DatabaseExplorer
        tables={tables}
        schema={schema}
        selectedTable={selectedTable}
        onSelectTable={handleSelectTable}
        onRefresh={() => { checkHealth().then(ok => ok && loadTables()); }}
        loading={explorerLoading}
      />

      <main className="app-main workspace">
        {/* Query Editor */}
        <QueryEditor
          tabs={tabs}
          activeTab={activeTab}
          onTabChange={setActiveTab}
          onTabAdd={handleTabAdd}
          onTabClose={handleTabClose}
          onTabSqlChange={handleTabSqlChange}
          onRun={handleRun}
          onClear={handleClear}
          running={running}
        />

        {/* Results Area */}
        <div className="results-area">
          <div className="results-tabs">
            <div
              className={`results-tab ${resultsTab === 'data' ? 'active' : ''}`}
              onClick={() => setResultsTab('data')}
            >
              Data
              {result?.rows && (
                <span style={{
                  marginLeft: 6, background: 'var(--bg-overlay)',
                  borderRadius: 10, padding: '0 6px', fontSize: 10,
                  color: 'var(--text-secondary)'
                }}>
                  {result.rows.length}
                </span>
              )}
            </div>
            <div
              className={`results-tab ${resultsTab === 'messages' ? 'active' : ''}`}
              onClick={() => setResultsTab('messages')}
            >
              Messages
              {messages.filter(m => m.type === 'error').length > 0 && (
                <span style={{
                  marginLeft: 6, background: 'var(--error-dim)',
                  borderRadius: 10, padding: '0 6px', fontSize: 10,
                  color: 'var(--text-error)'
                }}>
                  {messages.filter(m => m.type === 'error').length}
                </span>
              )}
            </div>
          </div>

          <div className="results-body">
            {resultsTab === 'data' ? (
              <ResultsGrid result={result} loading={running} />
            ) : (
              <MessagesPanel messages={messages} />
            )}
          </div>
        </div>
      </main>
    </div>
  );
}
