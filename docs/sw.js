// Chispa EFI · service worker
// Primero pide a la red (así siempre toma la versión nueva) y, si no hay señal,
// responde con lo guardado. Así la app abre en el taller aunque no haya internet.
const CACHE = 'chispa-v1';
const BASE = ['./', 'index.html', 'banco/', 'manifest.webmanifest', 'pwa.js', 'icons/icon-192.png', 'icons/icon-512.png', 'icons/icon.svg'];

self.addEventListener('install', e => {
  e.waitUntil(caches.open(CACHE).then(c => c.addAll(BASE)).then(() => self.skipWaiting()));
});

self.addEventListener('activate', e => {
  e.waitUntil(
    caches.keys()
      .then(keys => Promise.all(keys.filter(k => k.startsWith('chispa-') && k !== CACHE).map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', e => {
  const req = e.request;
  if (req.method !== 'GET') return;
  const url = new URL(req.url);
  const propio = url.origin === self.location.origin;
  const fuente = url.hostname === 'fonts.googleapis.com' || url.hostname === 'fonts.gstatic.com';
  if (!propio && !fuente) return;
  e.respondWith(caches.open(CACHE).then(async cache => {
    try {
      const res = await fetch(req);
      if (res && (res.ok || res.type === 'opaque')) cache.put(req, res.clone());
      return res;
    } catch (err) {
      const guardado = await cache.match(req, { ignoreSearch: true });
      if (guardado) return guardado;
      throw err;
    }
  }));
});
