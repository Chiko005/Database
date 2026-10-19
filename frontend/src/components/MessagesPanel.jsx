import { CheckCircle2, XCircle, Info, Terminal } from 'lucide-react';

function MessageItem({ msg }) {
  const icons = {
    success: <CheckCircle2 size={14} color="var(--text-success)" />,
    error:   <XCircle     size={14} color="var(--text-error)"   />,
    info:    <Info        size={14} color="var(--blue-300)"     />,
  };

  const time = new Date(msg.timestamp).toLocaleTimeString('en', { hour12: false });

  return (
    <div className={`message-item ${msg.type} fade-in`}>
      <span className="message-icon">{icons[msg.type] ?? icons.info}</span>
      <div className="message-body">
        <div className="message-text">{msg.text}</div>
        <div className="message-time">{time}</div>
      </div>
    </div>
  );
}

export default function MessagesPanel({ messages }) {
  if (messages.length === 0) {
    return (
      <div className="empty-state" style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', gap: 8, padding: 24 }}>
        <Terminal size={32} style={{ opacity: 0.2 }} />
        <p>Messages will appear here after executing queries</p>
      </div>
    );
  }

  return (
    <div className="messages-panel">
      {messages.map((m, i) => <MessageItem key={i} msg={m} />)}
    </div>
  );
}
