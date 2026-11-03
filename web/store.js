/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Browser cache: the downloaded file bytes in IndexedDB (charts work offline, "Download
// all new" fetches only what is missing), plus a small index in localStorage.
'use strict';

const DB_NAME = 'kallifunghi';
const LS_KEY = 'kf.downloaded';   // {path: bytes cached}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
const Store = {
  db: null,

  open() {
    if (this.db) return Promise.resolve(this.db);
    return new Promise((resolve, reject) => {
      const r = indexedDB.open(DB_NAME, 1);
      r.onupgradeneeded = () => {
        r.result.createObjectStore('files', { keyPath: 'path' });
        r.result.createObjectStore('settings');
      };
      r.onsuccess = () => { this.db = r.result; resolve(this.db); };
      r.onerror = () => reject(r.error);
    });
  },

  async tx(store, mode, fn) {
    const db = await this.open();
    return new Promise((resolve, reject) => {
      const t = db.transaction(store, mode);
      const req = fn(t.objectStore(store));
      t.oncomplete = () => resolve(req ? req.result : undefined);
      t.onerror = () => reject(t.error);
    });
  },

  // {path, bytes: Uint8Array, crc, deviceSize, updated}
  getFile(path) { return this.tx('files', 'readonly', (s) => s.get(path)); },
  allFiles() { return this.tx('files', 'readonly', (s) => s.getAll()); },
  async putFile(rec) {
    await this.tx('files', 'readwrite', (s) => s.put(rec));
    const idx = this.index();
    idx[rec.path] = rec.bytes.length;
    try { localStorage.setItem(LS_KEY, JSON.stringify(idx)); } catch (_) { /* private mode */ }
  },
  getSetting(key) { return this.tx('settings', 'readonly', (s) => s.get(key)); },
  putSetting(key, value) { return this.tx('settings', 'readwrite', (s) => s.put(value, key)); },

  index() {
    try { return JSON.parse(localStorage.getItem(LS_KEY) || '{}'); } catch (_) { return {}; }
  },

  async clear() {
    if (this.db) { this.db.close(); this.db = null; }
    try { localStorage.removeItem(LS_KEY); } catch (_) { /* ignore */ }
    await new Promise((resolve, reject) => {
      const r = indexedDB.deleteDatabase(DB_NAME);
      r.onsuccess = resolve;
      r.onerror = () => reject(r.error);
      r.onblocked = resolve;
    });
  },
};
