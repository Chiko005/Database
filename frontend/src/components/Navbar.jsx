import { Database, Wifi, WifiOff, RefreshCw, Settings } from 'lucide-react';

export default function Navbar({ engineStatus, onRefresh }) {
  const connected = engineStatus === 'connected';

  return (
    <header className="navbar app-navbar">
      {/* Logo */}
      <div className="navbar-logo">
        <div className="navbar-logo-icon">
          <svg width="14" height="14" viewBox="0 0 14 14" fill="none">
            <rect x="1" y="1" width="5" height="5" rx="1" fill="white" opacity="0.9"/>
            <rect x="8" y="1" width="5" height="5" rx="1" fill="white" opacity="0.6"/>
            <rect x="1" y="8" width="5" height="5" rx="1" fill="white" opacity="0.6"/>
            <rect x="8" y="8" width="5" height="5" rx="1" fill="white" opacity="0.9"/>
          </svg>
        </div>
        B+ Studio
      </div>

      <div className="navbar-sep" />

      {/* DB Badge */}
      <div className="navbar-db-badge">
        <span className={`status-dot ${connected ? 'connected' : 'disconnected'}`} />
        <Database size={12} />
        <span className="db-name">database.db</span>
        <span style={{ color: 'var(--text-muted)', fontSize: 11 }}>
          {connected ? '· B+ Tree Engine' : '· Disconnected'}
        </span>
      </div>

      <div className="navbar-right">
        <button
          className="icon-btn"
          data-tooltip="Refresh connection"
          onClick={onRefresh}
        >
          <RefreshCw size={14} />
        </button>
        <button className="icon-btn" data-tooltip="Settings">
          <Settings size={14} />
        </button>
      </div>
    </header>
  );
}
