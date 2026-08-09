/* =====================================================
   SERVICE WORKER — TampAê
   Versão atual da estrutura do PWA.

   O aplicativo atual está organizado em:
   /app/pages/login
   /app/pages/inicio
   /app/pages/mapa
   /app/pages/coleta
   /app/pages/perfil
   /app/pages/ranking
   /app/pages/configura

   Estratégia: REDE PRIMEIRO.
   Com internet, o navegador sempre tenta buscar a versão
   atual. O cache funciona como fallback quando a rede
   estiver indisponível.
   ===================================================== */

const CACHE_NAME = "tampae-v3";

const ARQUIVOS_ESSENCIAIS = [
    "/",
    "/index.html",
    "/manifest.json",

    // Entrada do aplicativo
    "/app/pages/login/index.html",

    // Páginas principais
    "/app/pages/inicio/home.html",
    "/app/pages/mapa/mapa.html",
    "/app/pages/coleta/qr.html",
    "/app/pages/perfil/perfil.html",
    "/app/pages/ranking/rank.html",
    "/app/pages/configura/configuracoes.html",

    // Configuração compartilhada
    "/app/js/auth.js",
    "/app/js/config.js",
    "/app/js/supabase.js",

    // Ícones do PWA
    "/app/img/icon-192.png",
    "/app/img/icon-512.png",
    "/app/img/icon-maskable-512.png"
];

self.addEventListener("install", (evento) => {
    evento.waitUntil(
        caches.open(CACHE_NAME).then((cache) => {
            return cache.addAll(ARQUIVOS_ESSENCIAIS);
        })
    );

    // Ativa a nova versão sem esperar as abas antigas fecharem.
    self.skipWaiting();
});

self.addEventListener("activate", (evento) => {
    evento.waitUntil(
        caches.keys().then((chaves) => {
            return Promise.all(
                chaves
                    .filter((chave) => chave !== CACHE_NAME)
                    .map((chave) => caches.delete(chave))
            );
        })
    );

    // Assume imediatamente o controle das páginas abertas.
    self.clients.claim();
});

self.addEventListener("fetch", (evento) => {
    // Só trata requisições GET.
    if (evento.request.method !== "GET") {
        return;
    }

    evento.respondWith(
        fetch(evento.request)
            .then((resposta) => {
                // Não armazena respostas inválidas.
                if (!resposta || resposta.status !== 200) {
                    return resposta;
                }

                const copia = resposta.clone();

                caches.open(CACHE_NAME).then((cache) => {
                    cache.put(evento.request, copia);
                });

                return resposta;
            })
            .catch(() => {
                // Sem internet: tenta entregar a versão em cache.
                return caches.match(evento.request);
            })
    );
});
