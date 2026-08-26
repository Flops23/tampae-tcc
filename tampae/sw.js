const CACHE_NAME = "tampae-app-v7";

const APP_SHELL = [
  "./app/pages/login/index.html",
  "./app/pages/login/index.css",
  "./app/pages/login/index.js",
  "./app/img/icon-192.png",
  "./app/img/icon-512.png",
  "./app/img/icon-maskable-512.png"
];

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then((cache) => cache.addAll(APP_SHELL))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(
        keys
          .filter((key) => key !== CACHE_NAME)
          .map((key) => caches.delete(key))
      ))
      .then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", (event) => {
  const request = event.request;
  const url = new URL(request.url);

  if (url.origin !== self.location.origin || request.method !== "GET") return;

  // O Service Worker só controla URLs dentro de /tampae/app/.
  if (!url.pathname.includes("/tampae/app/")) return;

  // Supabase e APIs externas não devem passar pelo cache do PWA.
  if (url.hostname !== self.location.hostname) return;

  event.respondWith(
    fetch(request)
      .then((response) => {
        if (response.ok && response.type === "basic") {
          const copy = response.clone();
          caches.open(CACHE_NAME).then((cache) => cache.put(request, copy));
        }
        return response;
      })
      .catch(() =>
        caches.match(request).then((cached) => {
          if (cached) return cached;
          return caches.match("./app/pages/login/index.html");
        })
      )
  );
});
