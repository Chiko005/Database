import { useRef, useEffect, useCallback } from 'react';
import Editor from '@monaco-editor/react';
import { Play, Trash2, Plus, X } from 'lucide-react';

const SAMPLE_QUERY = `-- Welcome to B+ Studio
-- Your B+ Tree SQL Engine is connected
-- Press Ctrl+Enter or click Run Query to execute

SELECT * FROM users;
`;

const MONACO_OPTIONS = {
  fontSize: 13,
  fontFamily: "'JetBrains Mono', 'Fira Code', monospace",
  fontLigatures: true,
  lineNumbers: 'on',
  minimap: { enabled: false },
  scrollBeyondLastLine: false,
  wordWrap: 'on',
  tabSize: 2,
  renderLineHighlight: 'line',
  cursorBlinking: 'smooth',
  smoothScrolling: true,
  padding: { top: 12, bottom: 12 },
  scrollbar: {
    verticalScrollbarSize: 6,
    horizontalScrollbarSize: 6,
  },
  suggestOnTriggerCharacters: true,
  quickSuggestions: true,
  automaticLayout: true,
};

export default function QueryEditor({ tabs, activeTab, onTabChange, onTabAdd, onTabClose, onTabSqlChange, onRun, onClear, running }) {
  const editorRef = useRef(null);
  const monacoRef = useRef(null);

  const currentTab = tabs.find(t => t.id === activeTab);

  function handleMount(editor, monaco) {
    editorRef.current = editor;
    monacoRef.current = monaco;

    // Ctrl+Enter to run
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter, () => {
      onRun(editor.getValue());
    });

    // Register SQL language completions
    monaco.languages.registerCompletionItemProvider('sql', {
      provideCompletionItems: () => ({
        suggestions: [
          'SELECT', 'FROM', 'WHERE', 'INSERT', 'INTO', 'VALUES',
          'UPDATE', 'SET', 'DELETE', 'BETWEEN', 'AND', 'OR',
          'users', 'id', 'name', 'age',
        ].map(word => ({
          label: word,
          kind: monaco.languages.CompletionItemKind.Keyword,
          insertText: word,
        })),
      }),
    });
  }

  function handleRun() {
    const sql = editorRef.current?.getValue() ?? currentTab?.sql ?? '';
    onRun(sql);
  }

  function handleClear() {
    if (editorRef.current) editorRef.current.setValue('');
    onClear();
  }

  // When active tab changes, update the editor content
  useEffect(() => {
    if (editorRef.current && currentTab) {
      const current = editorRef.current.getValue();
      if (current !== currentTab.sql) {
        editorRef.current.setValue(currentTab.sql);
      }
    }
  }, [activeTab]);

  return (
    <section className="editor-section" style={{ height: '320px' }}>
      {/* Toolbar */}
      <div className="editor-toolbar">
        <div className="editor-tabs">
          {tabs.map(tab => (
            <div
              key={tab.id}
              className={`editor-tab ${tab.id === activeTab ? 'active' : ''}`}
              onClick={() => onTabChange(tab.id)}
            >
              {tab.title}
              {tabs.length > 1 && (
                <button
                  onClick={e => { e.stopPropagation(); onTabClose(tab.id); }}
                  style={{ background: 'none', border: 'none', color: 'inherit', cursor: 'pointer', padding: 0, display: 'flex', alignItems: 'center' }}
                >
                  <X size={10} />
                </button>
              )}
            </div>
          ))}
          <button
            className="icon-btn"
            onClick={onTabAdd}
            style={{ width: 24, height: 24, marginLeft: 4 }}
            data-tooltip="New tab"
          >
            <Plus size={12} />
          </button>
        </div>

        <button className="btn btn-ghost" onClick={handleClear} disabled={running}>
          <Trash2 size={13} />
          Clear
        </button>

        <button className="btn btn-primary" onClick={handleRun} disabled={running} id="run-query-btn">
          {running ? (
            <span className="spinner" style={{ width: 13, height: 13 }} />
          ) : (
            <Play size={13} />
          )}
          Run Query
        </button>

        <span className="shortcut-hint">Ctrl+Enter</span>
      </div>

      {/* Monaco Editor */}
      <div style={{ flex: 1, overflow: 'hidden' }}>
        <Editor
          height="100%"
          defaultLanguage="sql"
          defaultValue={currentTab?.sql ?? SAMPLE_QUERY}
          theme="vs-dark"
          options={MONACO_OPTIONS}
          onMount={handleMount}
          onChange={val => onTabSqlChange(activeTab, val ?? '')}
        />
      </div>
    </section>
  );
}
