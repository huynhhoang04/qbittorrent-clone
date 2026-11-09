#include "web_ui.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

#include "logger.hpp"
#include "peer_node.hpp"

using namespace std;
using namespace std::chrono;
using json = nlohmann::json;

namespace {



const char* INDEX_HTML = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>qBittorrent Clone - Web Dashboard</title>
<style>
:root {
  --bg-primary: #111827;
  --bg-secondary: #1f2937;
  --bg-card: #374151;
  --bg-hover: #4b5563;
  --text-main: #f9fafb;
  --text-muted: #9ca3af;
  --accent-blue: #38bdf8;
  --accent-green: #10b981;
  --accent-yellow: #f59e0b;
  --accent-red: #ef4444;
  --border-color: #4b5563;
}
* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
body { background: var(--bg-primary); color: var(--text-main); height: 100vh; display: flex; flex-direction: column; overflow: hidden; }

/* Header */
header { background: var(--bg-secondary); border-bottom: 1px solid var(--border-color); padding: 10px 20px; display: flex; align-items: center; justify-content: space-between; }
.logo-area { display: flex; align-items: center; gap: 10px; }
.logo-icon { width: 32px; height: 32px; background: linear-gradient(135deg, #0284c7, #38bdf8); border-radius: 8px; display: flex; align-items: center; justify-content: center; font-weight: bold; font-size: 18px; color: white; }
.logo-title { font-size: 18px; font-weight: 700; color: #f3f4f6; letter-spacing: -0.5px; }
.node-meta { display: flex; align-items: center; gap: 14px; font-size: 13px; color: var(--text-muted); }
.badge { display: inline-flex; align-items: center; gap: 6px; padding: 3px 8px; border-radius: 9999px; font-size: 11px; font-weight: 600; text-transform: uppercase; }
.badge-online { background: rgba(16, 185, 129, 0.2); color: #34d399; }
.badge-offline { background: rgba(239, 68, 68, 0.2); color: #f87171; }
.badge-dot { width: 6px; height: 6px; border-radius: 50%; background: currentColor; }

/* Toolbar */
.toolbar { background: var(--bg-secondary); border-bottom: 1px solid var(--border-color); padding: 8px 20px; display: flex; gap: 10px; align-items: center; flex-wrap: wrap; }
button { background: var(--bg-card); color: var(--text-main); border: 1px solid var(--border-color); padding: 6px 14px; border-radius: 6px; cursor: pointer; font-size: 13px; font-weight: 500; transition: all 0.15s; display: inline-flex; align-items: center; gap: 6px; }
button:hover { background: var(--bg-hover); border-color: #6b7280; }
button.btn-primary { background: #0284c7; border-color: #0369a1; }
button.btn-primary:hover { background: #0369a1; }
button.btn-danger { background: #991b1b; border-color: #7f1d1d; }
button.btn-danger:hover { background: #7f1d1d; }

/* Stats Bar */
.stats-bar { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 12px; padding: 12px 20px; background: rgba(31, 41, 55, 0.5); border-bottom: 1px solid var(--border-color); }
.stat-card { background: var(--bg-secondary); border: 1px solid var(--border-color); padding: 10px 14px; border-radius: 8px; }
.stat-label { font-size: 11px; text-transform: uppercase; color: var(--text-muted); letter-spacing: 0.5px; }
.stat-value { font-size: 18px; font-weight: 700; margin-top: 4px; color: #fff; }
.stat-green { color: var(--accent-green); }
.stat-blue { color: var(--accent-blue); }

/* Main layout: split */
.main-content { flex: 1; display: flex; flex-direction: column; min-height: 0; }
.torrents-pane { flex: 1; min-height: 180px; overflow-y: auto; }
.torrents-table { width: 100%; border-collapse: collapse; font-size: 13px; }
.torrents-table th { background: var(--bg-secondary); color: var(--text-muted); text-align: left; padding: 10px 14px; position: sticky; top: 0; border-bottom: 1px solid var(--border-color); font-weight: 600; }
.torrents-table td { padding: 10px 14px; border-bottom: 1px solid rgba(75, 85, 99, 0.4); vertical-align: middle; }
.torrents-table tr.selected { background: rgba(56, 189, 248, 0.15); }
.torrents-table tr:hover:not(.selected) { background: rgba(255, 255, 255, 0.03); cursor: pointer; }

/* Progress bar in table */
.progress-container { width: 160px; background: #111827; border-radius: 4px; height: 16px; position: relative; overflow: hidden; border: 1px solid #4b5563; }
.progress-fill { height: 100%; background: linear-gradient(90deg, #0284c7, #38bdf8); transition: width 0.3s; }
.progress-text { position: absolute; inset: 0; display: flex; align-items: center; justify-content: center; font-size: 10px; font-weight: 600; color: #fff; text-shadow: 0 1px 2px rgba(0,0,0,0.8); }

/* Detail Tabs Pane */
.detail-pane { height: 260px; background: var(--bg-secondary); border-top: 2px solid var(--border-color); display: flex; flex-direction: column; }
.tab-headers { display: flex; background: #111827; border-bottom: 1px solid var(--border-color); }
.tab-btn { background: transparent; border: none; border-radius: 0; padding: 8px 18px; color: var(--text-muted); font-size: 13px; font-weight: 600; border-bottom: 2px solid transparent; }
.tab-btn:hover { background: rgba(255,255,255,0.05); color: #fff; }
.tab-btn.active { color: var(--accent-blue); border-bottom-color: var(--accent-blue); background: var(--bg-secondary); }
.tab-content { flex: 1; padding: 12px; overflow-y: auto; }

/* Piece Map */
.piece-map { display: flex; flex-wrap: wrap; gap: 3px; max-height: 190px; overflow-y: auto; padding: 4px; }
.piece-block { width: 14px; height: 14px; border-radius: 2px; background: #4b5563; transition: transform 0.1s; }
.piece-block.done { background: var(--accent-green); box-shadow: 0 0 2px var(--accent-green); }
.piece-block:hover { transform: scale(1.3); z-index: 2; }

/* Modal */
.modal-overlay { position: fixed; inset: 0; background: rgba(0,0,0,0.7); display: none; align-items: center; justify-content: center; z-index: 100; backdrop-filter: blur(2px); }
.modal-overlay.open { display: flex; }
.modal { background: var(--bg-secondary); border: 1px solid var(--border-color); border-radius: 10px; width: 480px; max-width: 90vw; padding: 20px; box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.5); }
.modal-title { font-size: 16px; font-weight: 700; margin-bottom: 14px; display: flex; justify-content: space-between; align-items: center; }
.close-btn { background: none; border: none; font-size: 18px; color: var(--text-muted); cursor: pointer; }
.form-group { margin-bottom: 14px; }
.form-group label { display: block; font-size: 12px; font-weight: 600; color: var(--text-muted); margin-bottom: 6px; }
.form-control { width: 100%; padding: 8px 12px; background: var(--bg-primary); border: 1px solid var(--border-color); border-radius: 6px; color: #fff; font-size: 13px; }
.form-control:focus { outline: none; border-color: var(--accent-blue); }
.modal-actions { display: flex; justify-content: flex-end; gap: 10px; margin-top: 20px; }

/* Logs Console */
.log-console { font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; font-size: 12px; line-height: 1.5; color: #d1d5db; white-space: pre-wrap; word-break: break-all; }
.log-line { padding: 1px 0; }
.log-line.warn { color: var(--accent-yellow); }
.log-line.err { color: var(--accent-red); }
.log-line.ok { color: var(--accent-green); }
</style>
</head>
<body>

<header>
  <div class="logo-area">
    <div class="logo-icon">&#8681;</div>
    <div class="logo-title">qBittorrent Clone</div>
    <span id="trackerBadge" class="badge badge-offline"><span class="badge-dot"></span>Tracker OFFLINE</span>
  </div>
  <div class="node-meta">
    <div>Peer: <strong id="peerId">-</strong></div>
    <div>Port: <strong id="peerPort">-</strong></div>
    <div>Announce Host: <strong id="trackerHost">-</strong></div>
  </div>
</header>

<div class="toolbar">
  <button class="btn-primary" onclick="openModal('seedModal')">&#10133; Seed File</button>
  <button class="btn-primary" onclick="openModal('downloadModal')">&#10133; Add Torrent</button>
  <button onclick="controlSwarm('pause')">&#10074;&#10074; Pause</button>
  <button onclick="controlSwarm('resume')">&#9654; Resume</button>
  <button onclick="refreshData()">&#8635; Refresh</button>
  <span style="font-size: 12px; color: var(--text-muted); margin-left: auto;">Auto-refresh: 1s</span>
</div>

<div class="stats-bar">
  <div class="stat-card">
    <div class="stat-label">Download Speed</div>
    <div class="stat-value stat-green" id="downSpeed">0 B/s</div>
  </div>
  <div class="stat-card">
    <div class="stat-label">Upload Speed</div>
    <div class="stat-value stat-blue" id="upSpeed">0 B/s</div>
  </div>
  <div class="stat-card">
    <div class="stat-label">Total Downloaded</div>
    <div class="stat-value" id="totalDown">0 B</div>
  </div>
  <div class="stat-card">
    <div class="stat-label">Total Uploaded</div>
    <div class="stat-value" id="totalUp">0 B</div>
  </div>
  <div class="stat-card">
    <div class="stat-label">Upload Slots (Active / Max)</div>
    <div class="stat-value" id="uploadSlots">0 / 0</div>
  </div>
</div>

<div class="main-content">
  <div class="torrents-pane">
    <table class="torrents-table">
      <thead>
        <tr>
          <th>Name</th>
          <th>Size</th>
          <th>Progress</th>
          <th>Status</th>
          <th>Chunks</th>
          <th>File ID (SHA-256)</th>
          <th>Actions</th>
        </tr>
      </thead>
      <tbody id="torrentsBody">
        <tr><td colspan="7" style="text-align: center; color: var(--text-muted); padding: 30px;">No torrents registered yet. Click "+ Seed File" or "+ Add Torrent" to start.</td></tr>
      </tbody>
    </table>
  </div>

  <div class="detail-pane">
    <div class="tab-headers">
      <button class="tab-btn active" onclick="switchTab('piecemap')">Piece Map</button>
      <button class="tab-btn" onclick="switchTab('peers')">Peers (<span id="peerCount">0</span>)</button>
      <button class="tab-btn" onclick="switchTab('logs')">Execution Logs</button>
    </div>
    <div class="tab-content" id="tabContentPieceMap">
      <div id="pieceMapInfo" style="font-size: 12px; color: var(--text-muted); margin-bottom: 8px;">Select a torrent above to inspect its verified chunks.</div>
      <div class="piece-map" id="pieceMapGrid"></div>
    </div>
    <div class="tab-content" id="tabContentPeers" style="display: none;">
      <table class="torrents-table" style="font-size: 12px;">
        <thead>
          <tr>
            <th>Direction</th>
            <th>Peer ID</th>
            <th>Endpoint</th>
            <th>State</th>
            <th>Chunks</th>
            <th>Data Transferred</th>
            <th>Last Status</th>
          </tr>
        </thead>
        <tbody id="peersTableBody"></tbody>
      </table>
    </div>
    <div class="tab-content" id="tabContentLogs" style="display: none;">
      <div class="log-console" id="logConsole"></div>
    </div>
  </div>
</div>

<!-- Modal Seed -->
<div class="modal-overlay" id="seedModal">
  <div class="modal">
    <div class="modal-title">
      <span>Seed a File</span>
      <button class="close-btn" onclick="closeModal('seedModal')">&times;</button>
    </div>
    <div class="form-group">
      <label>Option A: Upload File from Browser</label>
      <input type="file" id="seedFileInput" class="form-control">
    </div>
    <div style="text-align: center; color: var(--text-muted); font-size: 12px; margin: 8px 0;">-- OR --</div>
    <div class="form-group">
      <label>Option B: Server File Path</label>
      <input type="text" id="seedPathInput" class="form-control" placeholder="/path/to/my_data.bin">
    </div>
    <div class="form-group">
      <label>Chunk Size (Bytes)</label>
      <input type="number" id="seedChunkSize" class="form-control" value="262144">
    </div>
    <div class="modal-actions">
      <button onclick="closeModal('seedModal')">Cancel</button>
      <button class="btn-primary" onclick="submitSeed()">Start Seeding</button>
    </div>
  </div>
</div>

<!-- Modal Download -->
<div class="modal-overlay" id="downloadModal">
  <div class="modal">
    <div class="modal-title">
      <span>Add Torrent for Download</span>
      <button class="close-btn" onclick="closeModal('downloadModal')">&times;</button>
    </div>
    <div class="form-group">
      <label>Option A: Upload .torrent.json File</label>
      <input type="file" id="torrentFileInput" class="form-control" accept=".json,.torrent">
    </div>
    <div style="text-align: center; color: var(--text-muted); font-size: 12px; margin: 8px 0;">-- OR --</div>
    <div class="form-group">
      <label>Option B: Torrent File Path on Server</label>
      <input type="text" id="torrentPathInput" class="form-control" placeholder="/path/to/file.bin.torrent.json">
    </div>
    <div class="form-group">
      <label>Output Directory (Optional)</label>
      <input type="text" id="outputDirInput" class="form-control" placeholder="./data/downloads">
    </div>
    <div class="modal-actions">
      <button onclick="closeModal('downloadModal')">Cancel</button>
      <button class="btn-primary" onclick="submitDownload()">Start Download</button>
    </div>
  </div>
</div>

<script>
let selectedFileId = null;
let currentData = null;

function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024, sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

function openModal(id) { document.getElementById(id).classList.add('open'); }
function closeModal(id) { document.getElementById(id).classList.remove('open'); }

function switchTab(name) {
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.getElementById('tabContentPieceMap').style.display = 'none';
  document.getElementById('tabContentPeers').style.display = 'none';
  document.getElementById('tabContentLogs').style.display = 'none';
  if (name === 'piecemap') {
    document.querySelectorAll('.tab-btn')[0].classList.add('active');
    document.getElementById('tabContentPieceMap').style.display = 'block';
  } else if (name === 'peers') {
    document.querySelectorAll('.tab-btn')[1].classList.add('active');
    document.getElementById('tabContentPeers').style.display = 'block';
  } else if (name === 'logs') {
    document.querySelectorAll('.tab-btn')[2].classList.add('active');
    document.getElementById('tabContentLogs').style.display = 'block';
  }
}

async function refreshData() {
  try {
    const res = await fetch('/api/status');
    if (!res.ok) return;
    const data = await res.json();
    currentData = data;

    // Header info
    document.getElementById('peerId').textContent = data.peer_id || '-';
    document.getElementById('peerPort').textContent = data.port || '-';
    document.getElementById('trackerHost').textContent = data.tracker_url || '-';
    const badge = document.getElementById('trackerBadge');
    if (data.tracker_online) {
      badge.className = 'badge badge-online';
      badge.innerHTML = '<span class="badge-dot"></span>Tracker ONLINE';
    } else {
      badge.className = 'badge badge-offline';
      badge.innerHTML = '<span class="badge-dot"></span>Tracker OFFLINE';
    }

    // Stats
    document.getElementById('downSpeed').textContent = formatBytes(data.download_speed || 0) + '/s';
    document.getElementById('upSpeed').textContent = formatBytes(data.upload_speed || 0) + '/s';
    document.getElementById('totalDown').textContent = formatBytes(data.downloaded_bytes || 0);
    document.getElementById('totalUp').textContent = formatBytes(data.uploaded_bytes || 0);
    document.getElementById('uploadSlots').textContent = `${data.active_upload_slots || 0} / ${data.max_upload_slots || 5}`;

    // Torrents
    const tbody = document.getElementById('torrentsBody');
    if (!data.torrents || data.torrents.length === 0) {
      tbody.innerHTML = '<tr><td colspan="7" style="text-align: center; color: var(--text-muted); padding: 30px;">No torrents registered yet. Click "+ Seed File" or "+ Add Torrent" to start.</td></tr>';
      selectedFileId = null;
    } else {
      let html = '';
      if (!selectedFileId || !data.torrents.some(t => t.file_id === selectedFileId)) {
        selectedFileId = data.torrents[0].file_id;
      }
      data.torrents.forEach(t => {
        const isSel = (t.file_id === selectedFileId);
        const percent = (t.progress || 0).toFixed(1);
        html += `<tr class="${isSel ? 'selected' : ''}" onclick="selectTorrent('${t.file_id}')">
          <td><strong>${escapeHtml(t.file_name)}</strong></td>
          <td>${formatBytes(t.file_size)}</td>
          <td>
            <div class="progress-container">
              <div class="progress-fill" style="width: ${percent}%;"></div>
              <div class="progress-text">${percent}%</div>
            </div>
          </td>
          <td><span style="font-weight: 600; text-transform: capitalize; color: ${t.mode === 'seeding' || t.mode === 'completed' ? 'var(--accent-green)' : 'var(--accent-blue)'}">${t.mode}</span></td>
          <td>${t.completed_chunks || 0} / ${t.num_chunks || 0}</td>
          <td><code style="font-size: 11px; color: var(--text-muted);">${(t.file_id || '').substring(0, 16)}...</code></td>
          <td>
            <a href="/api/torrent/file?file_id=${encodeURIComponent(t.file_id)}" download="${encodeURIComponent(t.file_name)}.torrent.json" style="color: var(--accent-blue); text-decoration: none; font-size: 12px; margin-right: 8px;">&#128190; .torrent</a>
          </td>
        </tr>`;
      });
      tbody.innerHTML = html;
    }

    renderPieceMap();
    renderPeers();
    renderLogs();
  } catch (err) {
    console.error("Refresh error:", err);
  }
}

function selectTorrent(fileId) {
  selectedFileId = fileId;
  document.querySelectorAll('#torrentsBody tr').forEach(tr => tr.classList.remove('selected'));
  if (currentData && currentData.torrents) {
    const rows = document.querySelectorAll('#torrentsBody tr');
    currentData.torrents.forEach((t, idx) => {
      if (t.file_id === fileId && rows[idx]) rows[idx].classList.add('selected');
    });
  }
  renderPieceMap();
}

function renderPieceMap() {
  const container = document.getElementById('pieceMapGrid');
  const info = document.getElementById('pieceMapInfo');
  if (!currentData || !currentData.torrents || !selectedFileId) {
    container.innerHTML = '';
    info.textContent = 'Select a torrent above to inspect its chunk piece map.';
    return;
  }
  const torrent = currentData.torrents.find(t => t.file_id === selectedFileId);
  if (!torrent || !torrent.bitfield) {
    container.innerHTML = '';
    info.textContent = 'No chunk bitfield available.';
    return;
  }

  info.textContent = `File: ${torrent.file_name} | Total Chunks: ${torrent.num_chunks} | Chunk Size: ${formatBytes(torrent.chunk_size)} | Verified: ${torrent.completed_chunks}/${torrent.num_chunks}`;
  let html = '';
  torrent.bitfield.forEach((hasChunk, idx) => {
    html += `<div class="piece-block ${hasChunk ? 'done' : ''}" title="Chunk #${idx}: ${hasChunk ? 'Verified (SHA-256 OK)' : 'Missing/Pending'}"></div>`;
  });
  container.innerHTML = html;
}

function renderPeers() {
  const tbody = document.getElementById('peersTableBody');
  if (!currentData) return;
  const downPeers = currentData.download_peers || [];
  const upPeers = currentData.upload_peers || [];
  document.getElementById('peerCount').textContent = downPeers.length + upPeers.length;

  let html = '';
  downPeers.forEach(p => {
    html += `<tr>
      <td><span style="color: var(--accent-green);">&#8595; Inbound</span></td>
      <td><code>${escapeHtml(p.peer_id)}</code></td>
      <td>${p.ip}:${p.port}</td>
      <td>${p.connected ? (p.choked ? 'Choked' : 'Unchoked') : 'Disconnected'}</td>
      <td>${p.chunks_downloaded || 0}</td>
      <td>${formatBytes(p.bytes_downloaded || 0)}</td>
      <td style="color: ${p.last_error ? 'var(--accent-red)' : 'var(--text-muted)'}">${escapeHtml(p.last_error || 'OK')}</td>
    </tr>`;
  });
  upPeers.forEach(p => {
    html += `<tr>
      <td><span style="color: var(--accent-blue);">&#8593; Outbound</span></td>
      <td><code>${escapeHtml(p.peer_id)}</code></td>
      <td>${p.ip}:${p.port}</td>
      <td>${p.choked ? 'Choked' : 'Unchoked'}</td>
      <td>${p.chunks_uploaded || 0}</td>
      <td>${formatBytes(p.bytes_uploaded || 0)}</td>
      <td>Serving upload</td>
    </tr>`;
  });
  if (downPeers.length === 0 && upPeers.length === 0) {
    html = '<tr><td colspan="7" style="text-align: center; color: var(--text-muted); padding: 15px;">No active peer connections in this swarm.</td></tr>';
  }
  tbody.innerHTML = html;
}

function renderLogs() {
  const consoleEl = document.getElementById('logConsole');
  if (!currentData || !currentData.logs) return;
  let html = '';
  currentData.logs.forEach(line => {
    let cls = '';
    if (line.includes('[ERROR]')) cls = 'err';
    else if (line.includes('[WARN]')) cls = 'warn';
    else if (line.includes('[SUCCESS]')) cls = 'ok';
    html += `<div class="log-line ${cls}">${escapeHtml(line)}</div>`;
  });
  consoleEl.innerHTML = html;
  consoleEl.scrollTop = consoleEl.scrollHeight;
}

async function controlSwarm(action) {
  try {
    await fetch('/api/control', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: action })
    });
    refreshData();
  } catch (err) { alert('Control request failed: ' + err); }
}

async function submitSeed() {
  const fileInput = document.getElementById('seedFileInput');
  const pathInput = document.getElementById('seedPathInput');
  const chunkSize = parseInt(document.getElementById('seedChunkSize').value) || 262144;

  if (fileInput.files.length > 0) {
    const file = fileInput.files[0];
    const buffer = await file.arrayBuffer();
    const res = await fetch('/api/upload_seed?filename=' + encodeURIComponent(file.name) + '&chunk_size=' + chunkSize, {
      method: 'POST',
      body: buffer
    });
    const result = await res.json();
    if (result.status === 'ok') {
      closeModal('seedModal');
      refreshData();
    } else {
      alert('Seeding error: ' + (result.error || 'unknown'));
    }
  } else if (pathInput.value.trim().length > 0) {
    const res = await fetch('/api/seed', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ file_path: pathInput.value.trim(), chunk_size: chunkSize })
    });
    const result = await res.json();
    if (result.status === 'ok') {
      closeModal('seedModal');
      refreshData();
    } else {
      alert('Seeding error: ' + (result.error || 'unknown'));
    }
  } else {
    alert('Please choose a file to upload or enter a server path.');
  }
}

async function submitDownload() {
  const fileInput = document.getElementById('torrentFileInput');
  const pathInput = document.getElementById('torrentPathInput');
  const outputDir = document.getElementById('outputDirInput').value.trim();

  if (fileInput.files.length > 0) {
    const file = fileInput.files[0];
    const text = await file.text();
    const res = await fetch('/api/upload_torrent?output_dir=' + encodeURIComponent(outputDir), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: text
    });
    const result = await res.json();
    if (result.status === 'ok') {
      closeModal('downloadModal');
      refreshData();
    } else {
      alert('Download error: ' + (result.error || 'unknown'));
    }
  } else if (pathInput.value.trim().length > 0) {
    const res = await fetch('/api/download', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ torrent_path: pathInput.value.trim(), output_dir: outputDir })
    });
    const result = await res.json();
    if (result.status === 'ok') {
      closeModal('downloadModal');
      refreshData();
    } else {
      alert('Download error: ' + (result.error || 'unknown'));
    }
  } else {
    alert('Please choose a .torrent.json file or enter a path.');
  }
}

function escapeHtml(str) {
  if (!str) return '';
  return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

setInterval(refreshData, 1000);
refreshData();
</script>
</body>
</html>
)rawhtml";

} // namespace

WebUiServer::WebUiServer(PeerNode& node, int port)
    : node(node), port(port), last_sample(steady_clock::now()) {
    setup_routes();
}

WebUiServer::~WebUiServer() {
    stop();
}

void WebUiServer::add_log(const string& line) {
    lock_guard<mutex> lock(log_mtx);
    log_history.push_back(line);
    while (log_history.size() > 200) {
        log_history.pop_front();
    }
}

void WebUiServer::update_speed_samples() {
    const auto status = node.get_status();
    const auto now = steady_clock::now();
    const double seconds = duration_cast<duration<double>>(now - last_sample).count();
    if (seconds >= 0.25) {
        const double down_delta = static_cast<double>(status.download.bytes_downloaded - last_download_bytes);
        const double up_delta = static_cast<double>(status.uploaded_bytes - last_upload_bytes);
        const double smoothing = 0.6;
        current_download_speed = smoothing * current_download_speed + (1.0 - smoothing) * (down_delta / seconds);
        current_upload_speed = smoothing * current_upload_speed + (1.0 - smoothing) * (up_delta / seconds);
        last_download_bytes = status.download.bytes_downloaded;
        last_upload_bytes = status.uploaded_bytes;
        last_sample = now;
    }
}

void WebUiServer::setup_routes() {
    // 1. Dashboard UI
    CROW_ROUTE(app, "/")([this](const crow::request&) {
        crow::response res(200, "text/html; charset=utf-8", INDEX_HTML);
        return res;
    });

    // 2. Status API
    CROW_ROUTE(app, "/api/status").methods(crow::HTTPMethod::Get)([this](const crow::request&) {
        update_speed_samples();
        const peer_node_status status = node.get_status();

        json j;
        j["peer_id"] = status.peer_id;
        j["port"] = status.port;
        j["tracker_url"] = node.get_tracker_url();
        j["tracker_online"] = status.tracker_online;
        j["download_active"] = status.download_active;
        j["download_speed"] = current_download_speed;
        j["upload_speed"] = current_upload_speed;
        j["uploaded_bytes"] = status.uploaded_bytes;
        j["downloaded_bytes"] = status.download.bytes_downloaded;
        j["active_upload_slots"] = status.active_upload_slots;
        j["max_upload_slots"] = node.get_options().max_active_uploads;

        // Torrents
        json torrents_arr = json::array();
        for (const auto& t : status.torrents) {
            json tj;
            tj["file_id"] = t.file_id;
            tj["file_name"] = t.file_name;
            tj["output_path"] = t.output_path;
            tj["mode"] = t.mode;
            tj["file_size"] = t.file_size;
            tj["chunk_size"] = t.chunk_size;
            tj["num_chunks"] = t.num_chunks;
            tj["completed_chunks"] = t.completed_chunks;
            tj["progress"] = t.progress;
            tj["bitfield"] = t.bitfield;
            torrents_arr.push_back(tj);
        }
        j["torrents"] = torrents_arr;

        // Download peers
        json down_peers_arr = json::array();
        for (const auto& p : status.download_peers) {
            json pj;
            pj["peer_id"] = p.peer_id;
            pj["ip"] = p.ip;
            pj["port"] = p.port;
            pj["connected"] = p.connected;
            pj["choked"] = p.choked;
            pj["chunks_downloaded"] = p.chunks_downloaded;
            pj["bytes_downloaded"] = p.bytes_downloaded;
            pj["last_error"] = p.last_error;
            down_peers_arr.push_back(pj);
        }
        j["download_peers"] = down_peers_arr;

        // Upload peers
        json up_peers_arr = json::array();
        for (const auto& p : status.upload_peers) {
            json pj;
            pj["peer_id"] = p.peer_id;
            pj["ip"] = p.ip;
            pj["port"] = p.port;
            pj["choked"] = p.choked;
            pj["chunks_uploaded"] = p.chunks_uploaded;
            pj["bytes_uploaded"] = p.bytes_uploaded;
            up_peers_arr.push_back(pj);
        }
        j["upload_peers"] = up_peers_arr;

        // Logs
        json logs_arr = json::array();
        {
            lock_guard<mutex> lock(log_mtx);
            for (const auto& log_line : log_history) {
                logs_arr.push_back(log_line);
            }
        }
        j["logs"] = logs_arr;

        return crow::response(200, "application/json", j.dump());
    });

    // 3. Seed file from path
    CROW_ROUTE(app, "/api/seed").methods(crow::HTTPMethod::Post)([this](const crow::request& req) {
        Logger log;
        try {
            json body = json::parse(req.body);
            if (!body.contains("file_path") || !body["file_path"].is_string()) {
                return crow::response(400, "application/json", "{\"error\":\"Missing file_path field\"}");
            }
            string path = body["file_path"].get<string>();
            size_t chunk_size = 256 * 1024;
            if (body.contains("chunk_size") && body["chunk_size"].is_number()) {
                chunk_size = body["chunk_size"].get<size_t>();
            }

            if (!node.seed_file(path, chunk_size)) {
                return crow::response(500, "application/json", "{\"error\":\"Failed to seed file\"}");
            }
            log.success("WebUI: started seeding file " + path);
            return crow::response(200, "application/json", "{\"status\":\"ok\",\"message\":\"Seeding started\"}");
        } catch (const exception& e) {
            return crow::response(400, "application/json", "{\"error\":\"" + string(e.what()) + "\"}");
        }
    });

    // 4. Upload raw binary file from browser and seed immediately
    CROW_ROUTE(app, "/api/upload_seed").methods(crow::HTTPMethod::Post)([this](const crow::request& req) {
        Logger log;
        try {
            const char* fname_param = req.url_params.get("filename");
            string filename = (fname_param && strlen(fname_param) > 0) ? fname_param : "uploaded_file.bin";
            const char* chunk_param = req.url_params.get("chunk_size");
            size_t chunk_size = (chunk_param && strlen(chunk_param) > 0) ? stoull(chunk_param) : (256 * 1024);

            string shared_dir = node.get_data_dir() + "/shared";
            filesystem::create_directories(shared_dir);
            string target_path = shared_dir + "/" + filename;

            ofstream out(target_path, ios::binary);
            if (!out.is_open()) {
                return crow::response(500, "application/json", "{\"error\":\"Cannot write file on server\"}");
            }
            out.write(req.body.data(), req.body.size());
            out.close();

            if (!node.seed_file(target_path, chunk_size)) {
                return crow::response(500, "application/json", "{\"error\":\"Failed to seed uploaded file\"}");
            }

            log.success("WebUI: successfully uploaded and seeded " + filename);
            return crow::response(200, "application/json", "{\"status\":\"ok\",\"message\":\"File uploaded and seeded\",\"path\":\"" + target_path + "\"}");
        } catch (const exception& e) {
            return crow::response(400, "application/json", "{\"error\":\"" + string(e.what()) + "\"}");
        }
    });

    // 5. Download from torrent file path
    CROW_ROUTE(app, "/api/download").methods(crow::HTTPMethod::Post)([this](const crow::request& req) {
        Logger log;
        try {
            json body = json::parse(req.body);
            if (!body.contains("torrent_path") || !body["torrent_path"].is_string()) {
                return crow::response(400, "application/json", "{\"error\":\"Missing torrent_path field\"}");
            }
            string torrent_path = body["torrent_path"].get<string>();
            string output_dir;
            if (body.contains("output_dir") && body["output_dir"].is_string()) {
                output_dir = body["output_dir"].get<string>();
            }

            if (!node.start_download_async(torrent_path, output_dir)) {
                return crow::response(500, "application/json", "{\"error\":\"Failed to start async download\"}");
            }
            log.success("WebUI: started downloading from " + torrent_path);
            return crow::response(200, "application/json", "{\"status\":\"ok\",\"message\":\"Download initiated\"}");
        } catch (const exception& e) {
            return crow::response(400, "application/json", "{\"error\":\"" + string(e.what()) + "\"}");
        }
    });

    // 6. Upload .torrent.json text/content from browser and download immediately
    CROW_ROUTE(app, "/api/upload_torrent").methods(crow::HTTPMethod::Post)([this](const crow::request& req) {
        Logger log;
        try {
            json torrent_json = json::parse(req.body);
            string file_name = "torrent";
            if (torrent_json.contains("file_name") && torrent_json["file_name"].is_string()) {
                file_name = torrent_json["file_name"].get<string>();
            }

            string torrents_dir = node.get_data_dir() + "/torrents";
            filesystem::create_directories(torrents_dir);
            string torrent_path = torrents_dir + "/" + file_name + ".torrent.json";

            ofstream out(torrent_path);
            if (!out.is_open()) {
                return crow::response(500, "application/json", "{\"error\":\"Cannot save torrent file\"}");
            }
            out << req.body;
            out.close();

            const char* out_param = req.url_params.get("output_dir");
            string output_dir = (out_param && strlen(out_param) > 0) ? out_param : "";

            if (!node.start_download_async(torrent_path, output_dir)) {
                return crow::response(500, "application/json", "{\"error\":\"Failed to start download\"}");
            }

            log.success("WebUI: uploaded torrent and started downloading " + file_name);
            return crow::response(200, "application/json", "{\"status\":\"ok\",\"message\":\"Torrent uploaded and download started\"}");
        } catch (const exception& e) {
            return crow::response(400, "application/json", "{\"error\":\"Invalid torrent json: " + string(e.what()) + "\"}");
        }
    });

    // 7. Export .torrent.json file for sharing
    CROW_ROUTE(app, "/api/torrent/file").methods(crow::HTTPMethod::Get)([this](const crow::request& req) {
        const char* fid = req.url_params.get("file_id");
        if (!fid || strlen(fid) == 0) {
            return crow::response(400, "text/plain", "Missing file_id parameter");
        }
        string file_id = fid;
        const auto status = node.get_status();
        for (const auto& t : status.torrents) {
            if (t.file_id == file_id) {
                // Find torrent json file: either in output_path + .torrent.json, or torrents/
                string potential_path = t.output_path + ".torrent.json";
                if (!filesystem::exists(potential_path)) {
                    potential_path = node.get_data_dir() + "/torrents/" + t.file_name + ".torrent.json";
                }
                if (filesystem::exists(potential_path)) {
                    ifstream in(potential_path);
                    stringstream buf;
                    buf << in.rdbuf();
                    crow::response res(200, "application/json", buf.str());
                    res.set_header("Content-Disposition", "attachment; filename=\"" + t.file_name + ".torrent.json\"");
                    return res;
                }
            }
        }
        return crow::response(404, "text/plain", "Torrent metadata file not found");
    });

    // 8. Control actions: pause / resume / stop
    CROW_ROUTE(app, "/api/control").methods(crow::HTTPMethod::Post)([this](const crow::request& req) {
        try {
            json body = json::parse(req.body);
            string action = body.value("action", "");
            if (action == "pause") {
                node.pause_download();
                add_log("[INFO] WebUI: Paused download");
            } else if (action == "resume") {
                node.resume_download(true);
                add_log("[INFO] WebUI: Resumed download");
            } else if (action == "stop") {
                node.request_stop_download();
                add_log("[WARN] WebUI: Requested stop");
            } else {
                return crow::response(400, "application/json", "{\"error\":\"Unknown action\"}");
            }
            return crow::response(200, "application/json", "{\"status\":\"ok\"}");
        } catch (const exception& e) {
            return crow::response(400, "application/json", "{\"error\":\"" + string(e.what()) + "\"}");
        }
    });
}

bool WebUiServer::start() {
    if (is_running.exchange(true)) {
        return true;
    }

    server_thread = thread([this]() {
        app.port(port).multithreaded().run();
    });

    // Give server a brief moment to initialize
    this_thread::sleep_for(milliseconds(100));
    return true;
}

void WebUiServer::stop() {
    if (!is_running.exchange(false)) {
        return;
    }
    app.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}
