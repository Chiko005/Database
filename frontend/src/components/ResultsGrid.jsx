import { Table2 } from 'lucide-react';

function formatCell(val) {
  if (val === null || val === undefined) return { text: 'NULL', cls: 'null-val' };
  if (typeof val === 'number') return { text: String(val), cls: 'num-val' };
  return { text: String(val), cls: 'str-val' };
}

export default function ResultsGrid({ result, loading }) {
  if (loading) {
    return (
      <div className="empty-state">
        <div className="spinner" style={{ width: 28, height: 28 }} />
        <p>Executing query…</p>
      </div>
    );
  }

  if (!result) {
    return (
      <div className="empty-state">
        <Table2 size={36} />
        <p>Run a query to see results here</p>
      </div>
    );
  }

  if (!result.success) {
    return (
      <div className="empty-state">
        <p style={{ color: 'var(--text-error)' }}>Query failed — see Messages tab</p>
      </div>
    );
  }

  // Non-SELECT (INSERT / UPDATE / DELETE)
  if (!result.columns || result.columns.length === 0) {
    return (
      <div className="empty-state">
        <p style={{ color: 'var(--text-success)' }}>
          Query OK — {result.rowsAffected ?? 0} row(s) affected
        </p>
      </div>
    );
  }

  const rows = result.rows ?? [];
  const cols = result.columns;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div className="grid-container">
        <table className="grid-table">
          <thead>
            <tr>
              <th className="row-num-col">#</th>
              {cols.map(c => <th key={c}>{c}</th>)}
            </tr>
          </thead>
          <tbody>
            {rows.length === 0 ? (
              <tr>
                <td colSpan={cols.length + 1} style={{ textAlign: 'center', color: 'var(--text-muted)', padding: '24px', fontFamily: 'var(--font-sans)' }}>
                  Empty set — no rows returned
                </td>
              </tr>
            ) : (
              rows.map((row, ri) => (
                <tr key={ri} className="fade-in">
                  <td className="row-num">{ri + 1}</td>
                  {row.map((cell, ci) => {
                    const { text, cls } = formatCell(cell);
                    return <td key={ci} className={cls}>{text}</td>;
                  })}
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {/* Footer */}
      <div className="grid-footer">
        <span className="stat">
          <span>Rows:</span>
          <span className="stat-value success">{rows.length}</span>
        </span>
        {result.rowsAffected !== undefined && result.rowsAffected > 0 && (
          <span className="stat">
            <span>Affected:</span>
            <span className="stat-value">{result.rowsAffected}</span>
          </span>
        )}
        {result.executionTimeMs !== undefined && (
          <span className="stat">
            <span>Time:</span>
            <span className="stat-value">{result.executionTimeMs.toFixed(2)} ms</span>
          </span>
        )}
      </div>
    </div>
  );
}
