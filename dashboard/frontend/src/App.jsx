import { useState, useEffect } from 'react';
import './index.css';

function formatBytes(bytes) {
  if (bytes === 0) return '0 Bytes';
  const k = 1024;
  const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function App() {
  const [nodes, setNodes] = useState([]);
  const [files, setFiles] = useState([]);
  const [health, setHealth] = useState({
    total_capacity: 0,
    total_used: 0,
    active_nodes: 0,
    dead_nodes: 0,
    replication_health: 0
  });

  const fetchData = async () => {
    try {
      const [nodesRes, filesRes, healthRes] = await Promise.all([
        fetch('http://localhost:3001/api/nodes'),
        fetch('http://localhost:3001/api/files'),
        fetch('http://localhost:3001/api/health')
      ]);
      
      setNodes(await nodesRes.json());
      setFiles(await filesRes.json());
      setHealth(await healthRes.json());
    } catch (error) {
      console.error("Failed to fetch data", error);
    }
  };

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 2000); // Poll every 2 seconds
    return () => clearInterval(interval);
  }, []);

  return (
    <div className="dashboard-container">
      <header>
        <h1>DFS Monitor</h1>
        <p>Real-time cluster visualization and health analytics</p>
      </header>

      {/* Global Overview */}
      <h2 className="section-title">Global Overview</h2>
      <div className="overview-grid">
        <div className="glass-card">
          <div className="stat-title">Replication Health</div>
          <div className="stat-value" style={{color: health.replication_health === 100 ? 'var(--success-color)' : 'var(--danger-color)'}}>
            {health.replication_health}%
          </div>
        </div>
        <div className="glass-card">
          <div className="stat-title">Active Nodes</div>
          <div className="stat-value" style={{color: 'var(--success-color)'}}>{health.active_nodes}</div>
        </div>
        <div className="glass-card">
          <div className="stat-title">Dead Nodes</div>
          <div className="stat-value" style={{color: health.dead_nodes > 0 ? 'var(--danger-color)' : 'var(--text-color)'}}>
            {health.dead_nodes}
          </div>
        </div>
        <div className="glass-card">
          <div className="stat-title">Storage Used</div>
          <div className="stat-value">
            {formatBytes(health.total_used)} <span style={{fontSize: '1rem', color: '#94a3b8'}}>/ {formatBytes(health.total_capacity)}</span>
          </div>
        </div>
      </div>

      {/* Nodes Grid */}
      <h2 className="section-title">Storage Nodes</h2>
      <div className="nodes-grid">
        {nodes.map(node => {
          const usagePercent = node.capacity > 0 ? (node.used_space / node.capacity) * 100 : 0;
          return (
            <div className="glass-card" key={node.node_id}>
              <div className="node-header">
                <span className="node-id">{node.node_id}</span>
                <span className={`status-badge ${node.status.toLowerCase()}`}>
                  {node.status}
                </span>
              </div>
              <div className="node-details">
                {node.ip}:{node.port}
              </div>
              <div>
                <div className="progress-bg">
                  <div className="progress-fill" style={{width: `${Math.min(usagePercent, 100)}%`}}></div>
                </div>
                <div className="progress-text">
                  {formatBytes(node.used_space)} / {formatBytes(node.capacity)}
                </div>
              </div>
            </div>
          );
        })}
        {nodes.length === 0 && <p style={{color: '#94a3b8'}}>No nodes registered yet.</p>}
      </div>

      {/* File Explorer */}
      <h2 className="section-title">Uploaded Files</h2>
      <div className="glass-card" style={{padding: 0, overflow: 'hidden'}}>
        <table>
          <thead>
            <tr>
              <th>Filename</th>
              <th>Size</th>
              <th>Replicated Across</th>
            </tr>
          </thead>
          <tbody>
            {files.map(file => (
              <tr key={file.filename}>
                <td><strong>{file.filename}</strong></td>
                <td>{formatBytes(file.size)}</td>
                <td>
                  {file.nodes_hosting.map(nodeId => (
                    <span key={nodeId} className="node-chip">{nodeId}</span>
                  ))}
                  {file.nodes_hosting.length === 0 && <span style={{color: '#94a3b8', fontSize: '0.8rem'}}>Calculating...</span>}
                </td>
              </tr>
            ))}
            {files.length === 0 && (
              <tr>
                <td colSpan="3" style={{textAlign: 'center', color: '#94a3b8'}}>No files uploaded yet.</td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}

export default App;
