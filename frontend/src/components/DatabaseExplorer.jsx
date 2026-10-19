import { useState } from 'react';
import {
  Database, Table2, ChevronRight, ChevronDown,
  RefreshCw, Key, Hash, Type, Columns3
} from 'lucide-react';

export default function DatabaseExplorer({ tables, schema, selectedTable, onSelectTable, onRefresh, loading }) {
  const [tablesOpen, setTablesOpen] = useState(true);
  const [indexesOpen, setIndexesOpen] = useState(true);

  return (
    <aside className="sidebar app-sidebar">
      <div className="sidebar-header">
        <span>Explorer</span>
        <button className="icon-btn" onClick={onRefresh} style={{ width: 22, height: 22 }}>
          <RefreshCw size={11} style={{ animation: loading ? 'spin 0.7s linear infinite' : 'none' }} />
        </button>
      </div>

      <div className="sidebar-scroll">
        {/* Tables section */}
        <div className="tree-section">
          <div className="tree-section-header" onClick={() => setTablesOpen(o => !o)}>
            {tablesOpen ? <ChevronDown size={11} /> : <ChevronRight size={11} />}
            <Database size={11} />
            Tables
          </div>
          {tablesOpen && (
            <>
              {tables.length === 0 ? (
                <div style={{ padding: '4px 24px', color: 'var(--text-muted)', fontSize: 11 }}>
                  No tables found
                </div>
              ) : (
                tables.map(t => (
                  <div
                    key={t}
                    className={`tree-item ${selectedTable === t ? 'active' : ''}`}
                    onClick={() => onSelectTable(t)}
                  >
                    <Table2 size={12} />
                    {t}
                  </div>
                ))
              )}
            </>
          )}
        </div>

        {/* Indexes section */}
        <div className="tree-section">
          <div className="tree-section-header" onClick={() => setIndexesOpen(o => !o)}>
            {indexesOpen ? <ChevronDown size={11} /> : <ChevronRight size={11} />}
            <Key size={11} />
            Indexes
          </div>
          {indexesOpen && schema?.indexes?.map(idx => (
            <div key={idx} className="tree-item">
              <Hash size={12} />
              {idx}
            </div>
          ))}
        </div>

        {/* Engine info */}
        <div className="tree-section">
          <div className="tree-section-header">
            <ChevronRight size={11} />
            Engine
          </div>
          <div style={{ padding: '4px 24px' }}>
            <div style={{ fontSize: 11, color: 'var(--text-muted)', lineHeight: 1.8 }}>
              <div>Type: <span style={{ color: 'var(--blue-300)' }}>B+ Tree</span></div>
              <div>Page: <span style={{ color: 'var(--blue-300)' }}>4096 B</span></div>
              <div>WAL: <span style={{ color: 'var(--text-success)' }}>enabled</span></div>
            </div>
          </div>
        </div>
      </div>

      {/* Schema panel for selected table */}
      {selectedTable && schema && (
        <div className="schema-panel">
          <h4><Columns3 size={10} style={{ display: 'inline', marginRight: 4 }} />{selectedTable} schema</h4>
          {schema.columns?.map(col => (
            <div key={col.name} className="schema-col">
              <span className="schema-col-name">{col.name}</span>
              <span style={{ display: 'flex', gap: 5, alignItems: 'center' }}>
                <span className="schema-col-type">{col.type}</span>
                {col.key === 'PRI' && <span className="schema-col-key">PK</span>}
              </span>
            </div>
          ))}
        </div>
      )}
    </aside>
  );
}
