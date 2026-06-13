const express = require('express');
const cors = require('cors');
const sqlite3 = require('sqlite3').verbose();
const path = require('path');

const app = express();
app.use(cors());

const dbPath = path.resolve(__dirname, '../../build/metadata.db');
const db = new sqlite3.Database(dbPath, (err) => {
    if (err) {
        console.error('Error opening database', err.message);
    } else {
        console.log('Connected to the SQLite database.');
    }
});

const HEARTBEAT_TIMEOUT_SEC = 15;

app.get('/api/nodes', (req, res) => {
    const now = Math.floor(Date.now() / 1000);
    db.all('SELECT * FROM Nodes', [], (err, rows) => {
        if (err) return res.status(500).json({ error: err.message });
        
        const nodes = rows.map(node => ({
            ...node,
            status: (now - node.last_heartbeat) <= HEARTBEAT_TIMEOUT_SEC ? 'Alive' : 'Dead'
        }));
        res.json(nodes);
    });
});

app.get('/api/files', (req, res) => {
    db.all('SELECT * FROM Files', [], (err, files) => {
        if (err) return res.status(500).json({ error: err.message });
        
        // Also fetch chunk distribution
        db.all('SELECT c.filename, cl.node_id FROM Chunks c JOIN ChunkLocations cl ON c.chunk_id = cl.chunk_id', [], (err, chunks) => {
            if (err) return res.status(500).json({ error: err.message });
            
            const fileChunksMap = {};
            chunks.forEach(c => {
                if (!fileChunksMap[c.filename]) fileChunksMap[c.filename] = new Set();
                fileChunksMap[c.filename].add(c.node_id);
            });

            const enrichedFiles = files.map(f => ({
                ...f,
                nodes_hosting: fileChunksMap[f.filename] ? Array.from(fileChunksMap[f.filename]) : []
            }));
            
            res.json(enrichedFiles);
        });
    });
});

app.get('/api/health', (req, res) => {
    const now = Math.floor(Date.now() / 1000);
    db.all('SELECT * FROM Nodes', [], (err, nodes) => {
        if (err) return res.status(500).json({ error: err.message });
        
        let totalCapacity = 0;
        let totalUsed = 0;
        let activeNodes = 0;
        let deadNodes = 0;

        nodes.forEach(node => {
            totalCapacity += node.capacity;
            totalUsed += node.used_space;
            if ((now - node.last_heartbeat) <= HEARTBEAT_TIMEOUT_SEC) {
                activeNodes++;
            } else {
                deadNodes++;
            }
        });

        // Basic replication health: activeNodes / 3 (since RF=3)
        // If activeNodes >= 3, health is 100%. If 2, ~66%. If 1, ~33%. If 0, 0%.
        let repHealth = 100;
        if (activeNodes < 3) {
            repHealth = Math.round((activeNodes / 3) * 100);
        }

        res.json({
            total_capacity: totalCapacity,
            total_used: totalUsed,
            active_nodes: activeNodes,
            dead_nodes: deadNodes,
            replication_health: repHealth
        });
    });
});

const PORT = 3001;
app.listen(PORT, () => {
    console.log(`Backend API running on http://localhost:${PORT}`);
});
