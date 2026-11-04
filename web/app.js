/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// KalliFunghi Sync page: status, time sync, file list and downloads, charts, live view.
'use strict';

const IDLE_DISCONNECT_MS = 10 * 60 * 1000;
const KEEPALIVE_MS = 60 * 1000;
const STATUS_POLL_MS = 5000;
const FIELDS = [
  { key: 'co2', title: 'CO2', unit: 'ppm', digits: 0 },
  { key: 'temp', title: 'Temperature', unit: '°C', digits: 1 },
  { key: 'rh', title: 'Humidity', unit: '%RH', digits: 0 },
  { key: 'lux', title: 'Light', unit: 'lux', digits: 1 },
];
const INTERVAL_S = { ultra: 300, eco: 30, fast: 5 };

const $ = (id) => document.getElementById(id);
const dev = new KfDevice();
const cache = new Map();          // path -> {path, bytes, crc, deviceSize, updated}
let deviceFiles = null;           // [{path, size}] from the last list, null = not connected
let status = null;
let dirHandle = null;             // File System Access folder, if chosen
let lastActivity = Date.now();
let lastKeepalive = 0;
let busy = false;
let charts = [];
let chartRun = '';
let rangeS = 86400;
const live = new Map();           // run dir -> [{t, co2, temp, rh, lux, mode}]

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
// The last few messages stay visible, newest first, so an error is not overwritten.
const logLines = [];
function log(msg, cls) {
  logLines.unshift({ msg: new Date().toLocaleTimeString() + '  ' + msg, cls: cls || '' });
  logLines.length = Math.min(logLines.length, 6);
  $('log').innerHTML = logLines.map((l, i) => `<div class="${l.cls}" style="opacity:${i ? 0.65 : 1}">${esc(l.msg)}</div>`).join('');
  if (cls === 'bad') console.warn(msg);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
function fmtSize(n) {
  if (n < 1024) return n + ' B';
  if (n < 1024 * 1024) return (n / 1024).toFixed(1) + ' KB';
  return (n / 1024 / 1024).toFixed(2) + ' MB';
}
function fmtDate(utcS) {
  if (!utcS) return 'never';
  return new Date(utcS * 1000).toLocaleString([], { dateStyle: 'medium', timeStyle: 'short' });
}
function fmtAgo(utcS, nowS) {
  if (!utcS) return 'never';
  const d = Math.max(0, nowS - utcS);
  if (d < 3600) return Math.round(d / 60) + ' min ago';
  if (d < 86400) return Math.round(d / 3600) + ' h ago';
  return Math.round(d / 86400) + ' days ago';
}
function esc(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}

// ---- Status ----

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
function renderStatus() {
  const el = $('status');
  if (!status) { el.innerHTML = ''; return; }
  const s = status;
  const nowS = Date.now() / 1000;
  const off = s.offset_ms / 1000;
  const offCls = !s.clock_valid || Math.abs(off) > 60 ? 'bad' : Math.abs(off) > 5 ? 'warn' : 'ok';
  const offTxt = (off >= 0 ? '+' : '') + off.toFixed(1) + ' s' + (s.clock_valid ? '' : ' (never synced)');
  const items = [
    ['Device time', new Date(s.utc_ms).toLocaleString()],
    ['Device vs. this device', `<span class="${offCls}">${esc(offTxt)}</span>`],
    ['Battery', s.usb ? 'USB' : `${s.bat_pct} % (${(s.bat_mv / 1000).toFixed(2)} V)`],
    ['Last sync', fmtAgo(s.last_sync, nowS)],
    ['Last CO2 calibration', s.last_cal ? fmtDate(s.last_cal) : 'never'],
    ['Measurement run', s.run_no ? `Run ${s.run_no} since ${fmtDate(s.run_start)}` : 'none yet'],
    ['SD card', `<span class="${s.sd === 'ok' ? 'ok' : 'bad'}">${esc(s.sd)}</span>` +
      (s.sd === 'ok' ? `, ${fmtSize(s.free_kb * 1024)} free` : '')],
    ['Sampling', `${esc(s.eff_mode)}${s.eff_mode !== s.mode ? ` (setting ${esc(s.mode)})` : ''}`],
    ['Firmware', esc(s.fw)],
  ];
  el.innerHTML = items.map(([k, v]) => `<div><span>${k}</span><b>${v}</b></div>`).join('');
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
async function refreshStatus() {
  if (!dev.connected) return;
  try {
    status = await dev.readStatus();
    renderStatus();
  } catch (e) {
    log('Status read failed: ' + e.message, 'bad');
  }
}

// ---- Connection ----

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
function setConnected(on) {
  $('connect').disabled = on || !KfDevice.supported();
  $('disconnect').disabled = !on;
  $('sync').disabled = !on;
  $('fast').disabled = !on;
  $('refresh').disabled = !on;
  $('all').disabled = !on;
  $('hint').hidden = on;
  if (!on) {
    deviceFiles = null;
    $('fast').checked = false;
  }
  renderFiles();
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
async function connect() {
  try {
    log('Choose KalliFunghi in the browser dialog ...');
    await dev.connect();
    log('Connected.', 'ok');
    setConnected(true);
    lastActivity = Date.now();
    await refreshStatus();
    await refreshList();
    if (status && status.run_dir) selectRun(status.run_dir);
  } catch (e) {
    if (e.name === 'NotFoundError') log('No device chosen. Tap Sync on the device first (on USB power it is always visible).');
    else log('Connect failed: ' + e.message, 'bad');
    setConnected(dev.connected);
  }
}

dev.addEventListener('disconnected', () => {
  log('Disconnected. To connect again, tap Sync on the device (on USB power it is always visible).');
  setConnected(false);
});

dev.addEventListener('sample', (e) => {
  const s = e.detail;
  if (!status || !status.run_dir || s.unsynced) return;
  if (!live.has(status.run_dir)) live.set(status.run_dir, []);
  live.get(status.run_dir).push(s);
  if (chartRun === status.run_dir) drawCharts(true);
});

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Page activity keeps the session; 10 min without it disconnects (the device has the
// same idle timeout).
function activity() { lastActivity = Date.now(); }
['pointerdown', 'keydown', 'wheel', 'touchstart'].forEach((t) => addEventListener(t, activity, { passive: true }));
setInterval(async () => {
  if (!dev.connected) return;
  const now = Date.now();
  if (now - lastActivity > IDLE_DISCONNECT_MS) {
    log('Disconnected after 10 min without activity.');
    await dev.disconnect(true);
    return;
  }
  if (now - lastKeepalive >= KEEPALIVE_MS && now - lastActivity < KEEPALIVE_MS) {
    lastKeepalive = now;
    dev.control(CTL.keepalive).catch(() => {});
  }
  if (!busy) refreshStatus();
}, STATUS_POLL_MS);

// ---- Files ----

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
function runOf(path) { const i = path.indexOf('/'); return i < 0 ? '' : path.slice(0, i); }
function nameOf(path) { return path.slice(path.indexOf('/') + 1); }
function runLabel(dir) {
  const m = /^m(\d+)_(.*)$/.exec(dir);
  if (!m) return dir;
  return `Run ${Number(m[1])}, ` + (m[2] === 'unsynced' ? 'started before the first clock sync' : 'started ' + m[2]);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
function renderFiles() {
  const rows = new Map();   // path -> {path, size (device), have}
  for (const [p, r] of cache) rows.set(p, { path: p, size: null, have: r.bytes.length });
  if (deviceFiles) {
    for (const f of deviceFiles) {
      const r = rows.get(f.path) || { path: f.path, have: 0 };
      r.size = f.size;
      rows.set(f.path, r);
    }
  }
  const groups = new Map();
  for (const r of rows.values()) {
    const g = runOf(r.path);
    if (!groups.has(g)) groups.set(g, []);
    groups.get(g).push(r);
  }
  const order = [...groups.keys()].sort((a, b) => (a === '' ? -1 : b === '' ? 1 : b.localeCompare(a)));
  let html = '';
  for (const g of order) {
    html += `<tr class="group"><td colspan="4">${esc(g === '' ? 'Device logs' : runLabel(g))}</td></tr>`;
    for (const r of groups.get(g).sort((a, b) => a.path.localeCompare(b.path))) {
      let state;
      if (r.size === null) state = `<span class="muted">cached ${fmtSize(r.have)}</span>`;
      else if (r.have === 0) state = 'new';
      else if (r.have < r.size) state = `<span class="warn">${Math.floor(r.have * 100 / r.size)} %, ${fmtSize(r.size - r.have)} new</span>`;
      else if (r.have > r.size) state = '<span class="warn">changed on the card</span>';
      else state = '<span class="ok">complete</span>';
      const canGet = r.have > 0 || (deviceFiles && r.size !== null);
      html += `<tr><td>${esc(nameOf(r.path))}</td><td class="num">${r.size === null ? '' : fmtSize(r.size)}</td>` +
        `<td>${state}</td><td><button class="small" data-path="${esc(r.path)}" ${canGet ? '' : 'disabled'}>Download</button></td></tr>`;
    }
  }
  if (!html) html = '<tr><td colspan="4" class="muted">No files yet. Connect to list the files on the device.</td></tr>';
  $('files').tBodies[0].innerHTML = html;
}

$('files').addEventListener('click', (e) => {
  const b = e.target.closest('button[data-path]');
  if (b) downloadOne(b.dataset.path);
});

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
async function refreshList() {
  if (!dev.connected) return;
  try {
    deviceFiles = await dev.list();
    renderFiles();
    renderRunSelect();
  } catch (e) {
    log('List failed: ' + e.message, 'bad');
  }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Brings the cached copy up to the device's size: only the missing bytes, resuming
// after a failed or interrupted transfer. A file smaller than the cache (new card) is
// fetched again from the start.
async function fetchFile(path, deviceSize) {
  let rec = cache.get(path) || { path, bytes: new Uint8Array(0) };
  if (deviceSize < rec.bytes.length) rec = { path, bytes: new Uint8Array(0) };
  const prog = $('progress');
  prog.hidden = false;
  const t0 = performance.now();
  let got = 0;
  for (let attempt = 0; attempt < 4 && rec.bytes.length < deviceSize; attempt++) {
    const base = rec.bytes.length;
    try {
      const r = await dev.read(path, base, (n) => {
        prog.max = deviceSize;
        prog.value = n;
      });
      if (r.size < base) { rec = { path, bytes: new Uint8Array(0) }; continue; }
      rec.bytes = concat([rec.bytes, r.bytes]);
      got += r.bytes.length;
      deviceSize = r.size;
    } catch (e) {
      console.warn('read', path, base, e);
      if (e.partial && e.partial.length) {
        rec.bytes = concat([rec.bytes, e.partial]);
        got += e.partial.length;
      }
      await save(rec, deviceSize);
      if (!dev.connected) throw e;
      log(`${path}: ${e.message}, resuming at ${rec.bytes.length}`, 'warn');
      await new Promise((r) => setTimeout(r, 400));
    }
  }
  prog.hidden = true;
  await save(rec, deviceSize);
  const s = (performance.now() - t0) / 1000;
  if (got > 0) log(`${nameOf(path)}: ${fmtSize(got)} in ${s.toFixed(1)} s (${fmtSize(got / s)}/s)`, 'ok');
  if (rec.bytes.length < deviceSize) throw new Error(`${path} incomplete`);
  return rec;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
async function save(rec, deviceSize) {
  rec.crc = crc32(rec.bytes);
  rec.deviceSize = deviceSize;
  rec.updated = Date.now();
  cache.set(rec.path, rec);
  await Store.putFile(rec);
  renderFiles();
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Saves the cached file as a normal file: into the chosen folder (run subfolders) or as
// a browser download named <run>_<file>.
async function saveToDisk(path) {
  const rec = cache.get(path);
  if (!rec) return;
  const blob = new Blob([rec.bytes], { type: path.endsWith('.csv') ? 'text/csv' : 'text/plain' });
  if (dirHandle) {
    let d = dirHandle;
    if (runOf(path)) d = await d.getDirectoryHandle(runOf(path), { create: true });
    const fh = await d.getFileHandle(nameOf(path), { create: true });
    const w = await fh.createWritable();
    await w.write(blob);
    await w.close();
    return;
  }
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = path.replace('/', '_');
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 10000);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
async function withBusy(fn) {
  if (busy) return;
  busy = true;
  $('all').disabled = true;
  try { await fn(); } catch (e) { log(e.message, 'bad'); }
  busy = false;
  $('all').disabled = !dev.connected;
  $('progress').hidden = true;
  renderRunSelect();
  drawCharts();
}

function downloadOne(path) {
  return withBusy(async () => {
    const f = deviceFiles && deviceFiles.find((x) => x.path === path);
    if (f && dev.connected) await fetchFile(path, f.size);
    await saveToDisk(path);
  });
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Every file that is new or has grown since the last download in this browser.
function downloadAllNew() {
  return withBusy(async () => {
    await refreshList();
    if (!deviceFiles) return;
    const todo = deviceFiles.filter((f) => {
      const r = cache.get(f.path);
      return !r || r.bytes.length !== f.size;
    });
    if (!todo.length) { log('Everything is downloaded already.', 'ok'); return; }
    const t0 = performance.now();
    let bytes = 0;
    for (let i = 0; i < todo.length; i++) {
      const f = todo[i];
      log(`Downloading ${i + 1} of ${todo.length}: ${f.path}`);
      const before = cache.get(f.path) ? cache.get(f.path).bytes.length : 0;
      const rec = await fetchFile(f.path, f.size);
      bytes += Math.max(0, rec.bytes.length - before);
      await saveToDisk(f.path);
    }
    const s = (performance.now() - t0) / 1000;
    log(`${todo.length} files, ${fmtSize(bytes)} in ${s.toFixed(1)} s (${fmtSize(bytes / s)}/s).`, 'ok');
  });
}

// ---- Charts ----

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
function runs() {
  const set = new Set();
  for (const p of cache.keys()) if (runOf(p)) set.add(runOf(p));
  if (deviceFiles) for (const f of deviceFiles) if (runOf(f.path)) set.add(runOf(f.path));
  if (status && status.run_dir) set.add(status.run_dir);
  return [...set].sort((a, b) => b.localeCompare(a));
}

function renderRunSelect() {
  const sel = $('run');
  const list = runs();
  if (!chartRun && list.length) chartRun = list[0];
  sel.innerHTML = list.map((r) => `<option value="${esc(r)}" ${r === chartRun ? 'selected' : ''}>${esc(runLabel(r))}</option>`).join('')
    || '<option value="">no runs yet</option>';
}

function selectRun(dir) {
  chartRun = dir;
  renderRunSelect();
  drawCharts();
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Rows of a run from the cached CSVs, sorted by time. unsynced.csv rows are shifted by
// the clock_synced= correction in run.txt, or left out when there is none.
function runRows(dir) {
  const dec = new TextDecoder();
  let corr = null;
  const info = cache.get(dir + '/run.txt');
  if (info) {
    const m = /^clock_synced=[^,\n]*,([+-]?\d+)/m.exec(dec.decode(info.bytes));
    if (m) corr = Number(m[1]);
  }
  const rows = [];
  let skipped = 0;
  for (const [p, rec] of cache) {
    if (runOf(p) !== dir || !p.endsWith('.csv')) continue;
    for (const line of dec.decode(rec.bytes).split('\n')) {
      const c = line.split(',');
      if (c.length < 7 || c[0] === 'utc_iso') continue;
      let t = Date.parse(c[0]) / 1000;
      if (!isFinite(t)) continue;
      if (c[6].trim() === 'unsynced') {
        if (corr === null) { skipped++; continue; }
        t += corr;
      }
      const num = (s) => (s === '' ? null : Number(s));
      rows.push({ t, co2: num(c[1]), temp: num(c[2]), rh: num(c[3]), lux: num(c[4]), mode: c[5] });
    }
  }
  const lv = live.get(dir) || [];
  const lastT = rows.reduce((a, r) => Math.max(a, r.t), 0);
  for (const s of lv) if (s.t > lastT) rows.push(s);
  rows.sort((a, b) => a.t - b.t);
  return { rows, skipped };
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// uPlot data with a null after every gap longer than 3 sampling intervals, so a pause
// is not drawn as a straight line.
function toData(rows) {
  const ts = [];
  const cols = FIELDS.map(() => []);
  for (let i = 0; i < rows.length; i++) {
    const r = rows[i];
    if (i > 0) {
      const gap = 3 * (INTERVAL_S[r.mode] || 30);
      if (r.t - rows[i - 1].t > gap) {
        ts.push(rows[i - 1].t + 1);
        cols.forEach((c) => c.push(null));
      }
    }
    ts.push(r.t);
    FIELDS.forEach((f, k) => cols[k].push(r[f.key]));
  }
  return [ts, ...cols];
}

function cssVar(n) { return getComputedStyle(document.documentElement).getPropertyValue(n).trim(); }

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
function makeCharts(data) {
  const host = $('charts');
  host.innerHTML = '';
  charts.forEach((c) => c.destroy());
  charts = [];
  const axis = { stroke: cssVar('--text-secondary'), grid: { stroke: cssVar('--grid'), width: 1 },
    ticks: { stroke: cssVar('--grid'), width: 1 } };
  let syncing = false;
  FIELDS.forEach((f, k) => {
    const box = document.createElement('div');
    box.className = 'chart';
    box.innerHTML = `<h3>${f.title} <span class="muted">${f.unit}</span></h3>`;
    host.appendChild(box);
    const u = new uPlot({
      width: Math.max(280, box.clientWidth),
      height: 220,
      cursor: { sync: { key: 'kf' }, drag: { x: true, y: false }, points: { size: 8 } },
      scales: { x: { time: true } },
      legend: { live: true },
      series: [
        { value: (u, v) => (v == null ? '–' : new Date(v * 1000).toLocaleString([], { dateStyle: 'short', timeStyle: 'medium' })) },
        { label: f.title, stroke: cssVar('--series-1'), width: 2, spanGaps: false, points: { show: false },
          value: (u, v) => (v == null ? '–' : v.toFixed(f.digits) + ' ' + f.unit) },
      ],
      axes: [axis, Object.assign({ size: 54 }, axis)],
      hooks: {
        setScale: [(u, key) => {
          if (key !== 'x' || syncing) return;
          syncing = true;
          const { min, max } = u.scales.x;
          charts.forEach((o) => { if (o !== u) o.setScale('x', { min, max }); });
          syncing = false;
        }],
      },
    }, [data[0], data[k + 1]], box);
    charts.push(u);
  });
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// keep=true (a live sample): new data, the current zoom stays unless it ends at "now".
function drawCharts(keep) {
  const note = $('chart-note');
  if (!chartRun) { note.textContent = 'No data in this browser yet.'; $('charts').innerHTML = ''; return; }
  const { rows, skipped } = runRows(chartRun);
  note.textContent = (rows.length ? `${rows.length} samples.` : 'No downloaded data for this run yet.') +
    (skipped ? ` ${skipped} rows from before the first clock sync are not shown.` : '');
  if (!rows.length) { $('charts').innerHTML = ''; charts = []; return; }
  const data = toData(rows);
  if (keep && charts.length === FIELDS.length) {
    const sc = charts[0].scales.x;
    const atEnd = sc.max >= data[0][data[0].length - 2];
    charts.forEach((u, k) => u.setData([data[0], data[k + 1]], false));
    if (atEnd) applyRange(); else charts.forEach((u) => u.setScale('x', { min: sc.min, max: sc.max }));
    return;
  }
  makeCharts(data);
  applyRange();
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
function applyRange() {
  if (!charts.length) return;
  const ts = charts[0].data[0];
  const t1 = ts[ts.length - 1];
  const t0 = rangeS ? Math.max(ts[0], t1 - rangeS) : ts[0];
  charts.forEach((u) => u.setScale('x', { min: t0, max: t1 }));
}

$('range').addEventListener('click', (e) => {
  const b = e.target.closest('button');
  if (!b) return;
  rangeS = Number(b.dataset.s);
  $('range').querySelectorAll('button').forEach((x) => x.classList.toggle('on', x === b));
  applyRange();
});

new ResizeObserver(() => {
  charts.forEach((u) => {
    const w = u.root.parentElement.clientWidth;
    if (w && Math.abs(w - u.width) > 4) u.setSize({ width: w, height: 220 });
  });
}).observe($('charts'));

matchMedia('(prefers-color-scheme: dark)').addEventListener('change', () => drawCharts());

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// All CSV rows of the run in one file, header once.
function exportRun() {
  if (!chartRun) return;
  const dec = new TextDecoder();
  const paths = [...cache.keys()].filter((p) => runOf(p) === chartRun && p.endsWith('.csv')).sort();
  let header = '';
  const body = [];
  for (const p of paths) {
    const lines = dec.decode(cache.get(p).bytes).split('\n').filter(Boolean);
    if (lines.length && lines[0].startsWith('utc_iso')) header = header || lines[0];
    if (lines.length && lines[0].startsWith('utc_iso')) lines.shift();
    body.push(...lines);
  }
  if (!body.length) { log('Nothing downloaded for this run yet.'); return; }
  const blob = new Blob([(header || 'utc_iso,co2_ppm,temp_c,rh_pct,lux,mode,clock') + '\n' + body.join('\n') + '\n'], { type: 'text/csv' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = chartRun + '_all.csv';
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 10000);
}

// ---- Wiring ----

$('connect').addEventListener('click', connect);
$('disconnect').addEventListener('click', () => dev.disconnect(true));
$('sync').addEventListener('click', async () => {
  try {
    const before = status ? status.offset_ms / 1000 : null;
    await dev.syncTime();
    log('Time sent, the device sets its clock at the next full second ...');
    await new Promise((r) => setTimeout(r, 1600));
    await refreshStatus();
    log('Clock set' + (before !== null ? `, it was off by ${before >= 0 ? '+' : ''}${before.toFixed(1)} s.` : '.'), 'ok');
  } catch (e) {
    log('Sync failed: ' + e.message, 'bad');
  }
});
$('fast').addEventListener('change', (e) => dev.control(CTL.fast, e.target.checked).catch((err) => log(err.message, 'bad')));
$('refresh').addEventListener('click', () => withBusy(refreshList));
$('all').addEventListener('click', downloadAllNew);
$('run').addEventListener('change', (e) => selectRun(e.target.value));
$('export').addEventListener('click', exportRun);
$('clear').addEventListener('click', async () => {
  if (!confirm('Delete all logs downloaded into this browser? Files on the device and saved files stay.')) return;
  await Store.clear();
  cache.clear();
  live.clear();
  renderFiles();
  renderRunSelect();
  drawCharts();
  log('Cache cleared.');
});
if (window.showDirectoryPicker) {
  $('folder').hidden = false;
  $('folder').addEventListener('click', async () => {
    try {
      dirHandle = await showDirectoryPicker({ mode: 'readwrite', id: 'kallifunghi' });
      await Store.putSetting('dir', dirHandle);
      log(`Downloads go to the folder "${dirHandle.name}", one subfolder per run.`, 'ok');
    } catch (_) { /* cancelled */ }
  });
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Why Web Bluetooth is missing, with what to do about it.
function bluetoothProblem() {
  const ua = navigator.userAgent;
  const host = location.hostname;
  if (!isSecureContext) {
    return location.protocol === 'http:'
      ? `The page was opened over plain http from "${host}", which is not a secure context. Open it from http://localhost, from HTTPS (the GitHub Pages address) or as a local file.`
      : 'This is not a secure context. Open the page from HTTPS, http://localhost or as a local file.';
  }
  if (/Firefox\//.test(ua)) return 'Firefox has no Web Bluetooth. Use Chrome or Edge.';
  if (/\b(iPhone|iPad|iPod)\b/.test(ua)) return 'iPhone browsers have no Web Bluetooth. Use a Web Bluetooth browser such as Bluefy.';
  if (/Safari\//.test(ua) && !/Chrome\/|Chromium\/|Edg\//.test(ua)) return 'Safari has no Web Bluetooth. Use Chrome or Edge.';
  if (/; wv\)|\bwv\b|FBAN|FBAV|Instagram|GSA\//.test(ua)) {
    return 'This looks like an in-app viewer (WebView), which has no Web Bluetooth. Open the page in Chrome itself (menu: "Open in Chrome").';
  }
  if (/Linux/.test(ua) && !/Android/.test(ua)) return 'On Linux, Chrome needs the flag chrome://flags/#enable-experimental-web-platform-features.';
  return 'This browser does not offer Web Bluetooth. Use Chrome or Edge on Android, Windows, macOS or ChromeOS, and check that Bluetooth is not blocked by a policy.';
}

/* Edited by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
async function init() {
  if (!KfDevice.supported()) {
    $('banner').classList.add('show');
    $('banner-hint').textContent = bluetoothProblem();
    $('banner-detail').textContent = `protocol ${location.protocol}, secure context ${isSecureContext}, ` +
      `navigator.bluetooth ${'bluetooth' in navigator ? 'present' : 'missing'}, ${navigator.userAgent}`;
  }
  setConnected(false);
  try {
    for (const r of await Store.allFiles()) cache.set(r.path, r);
    const h = await Store.getSetting('dir');
    // Permission must be granted again after a reload; only reuse it if still granted.
    if (h && (await h.queryPermission({ mode: 'readwrite' })) === 'granted') dirHandle = h;
  } catch (e) {
    log('Browser storage not available: ' + e.message, 'warn');
  }
  renderFiles();
  renderRunSelect();
  drawCharts();
  if ('serviceWorker' in navigator && (location.protocol === 'https:' || location.hostname === 'localhost' || location.hostname === '127.0.0.1')) {
    navigator.serviceWorker.register('sw.js').catch(() => {});
  }
}
init();
