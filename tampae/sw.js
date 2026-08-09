const CACHE_NAME = "tampae-app-v5";

const APP_SHELL = [
  "./app/",
  "./app/index.html",
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

  // O Service Worker do PWA controla somente a aplicação.
  // A apresentação em /tampae/index.html fica fora do escopo do app.
  if (url.origin !== self.location.origin || request.method !== "GET") {
    return;
  }

  // Não interceptar chamadas ao Supabase/API ou recursos externos.
  if (url.pathname.includes("/supabase/") || url.hostname !== self.location.hostname) {
    return;
  }

  // O app usa rede primeiro para receber atualizações imediatamente.
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
        caches.match(request).then((cached) => cached || caches.match("./app/index.html"))
      )
  );
});
