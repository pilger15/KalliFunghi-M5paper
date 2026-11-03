/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Low | Reviewed by: None */
// Service worker: the installed app works fully offline after one online visit (next to
// the grow box, no internet), and picks up a new version whenever it is online.
// Network first with a short timeout, then the cache; every good response refreshes the
// cache. Bump CACHE when the file list changes.
'use strict';

const CACHE = 'kallifunghi-v2';
const FILES = ['./', 'index.html', 'app.js', 'ble.js', 'store.js', 'manifest.webmanifest',
  'icon.svg', 'icon-192.png', 'icon-512.png', 'icon-maskable-512.png',
  'vendor/uPlot.iife.min.js', 'vendor/uPlot.min.css', 'KalliFunghi.html'];
const NETWORK_TIMEOUT_MS = 3000;

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE)
    .then((c) => c.addAll(FILES.map((f) => new Request(f, { cache: 'reload' }))))
    .then(() => self.skipWaiting()));
});

self.addEventListener('activate', (e) => {
  e.waitUntil(caches.keys()
    .then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
    .then(() => self.clients.claim()));
});

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
function fromNetwork(req) {
  return new Promise((resolve, reject) => {
    const t = setTimeout(() => reject(new Error('timeout')), NETWORK_TIMEOUT_MS);
    fetch(req).then((r) => {
      clearTimeout(t);
      if (!r.ok) { reject(new Error('http ' + r.status)); return; }
      const copy = r.clone();
      caches.open(CACHE).then((c) => c.put(req, copy));
      resolve(r);
    }, (err) => { clearTimeout(t); reject(err); });
  });
}

self.addEventListener('fetch', (e) => {
  if (e.request.method !== 'GET' || new URL(e.request.url).origin !== location.origin) return;
  e.respondWith(fromNetwork(e.request).catch(() =>
    caches.match(e.request, { ignoreSearch: true })
      .then((r) => r || (e.request.mode === 'navigate' ? caches.match('index.html') : Response.error()))));
});
