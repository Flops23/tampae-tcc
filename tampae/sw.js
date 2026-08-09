/*=====================================================
   SERVICE WORKER — TampAê
   Necessário pra o navegador considerar o app um PWA
   instalável de verdade (junto com o manifest.json).
   Guarda o "esqueleto" do app em cache só como reserva
   pra funcionar offline — a estratégia é REDE PRIMEIRO,
   então toda vez que tiver internet ele busca a versão
   mais nova de verdade, em vez de travar numa versão
   antiga guardada no celular.
=====================================================*/

const CACHE_NAME = "tampae-v2";

const ARQUIVOS_ESSENCIAIS = [
    "/index.html",
    "/app/index.html",
    "/app/home.html",
    "/app/mapa.html",
    "/app/rank.html",
    "/app/qr.html",
    "/app/perfil.html",
    "/app/configuracoes.html",
    "/manifest.json",
    "/icons/icon-192.png",
    "/icons/icon-512.png"
];

self.addEventListener("install", (evento) => {

    evento.waitUntil(
        caches.open(CACHE_NAME)
            .then((cache) => cache.addAll(ARQUIVOS_ESSENCIAIS))
    );

    // assume o controle imediatamente, sem esperar todas as
    // abas antigas fecharem
    self.skipWaiting();

});

self.addEventListener("activate", (evento) => {

    evento.waitUntil(
        caches.keys().then((chaves) =>
            Promise.all(
                // apaga qualquer cache de uma versão anterior
                // (ex: "tampae-v1") assim que essa versão nova ativa
                chaves
                    .filter((chave) => chave !== CACHE_NAME)
                    .map((chave) => caches.delete(chave))
            )
        )
    );

    self.clients.claim();

});

self.addEventListener("fetch", (evento) => {

    evento.respondWith(

        fetch(evento.request)
            .then((resposta) => {

                // busca da rede deu certo -> atualiza o cache
                // com a versão mais nova, pra servir de reserva
                const copia = resposta.clone();

                caches.open(CACHE_NAME).then((cache) => {
                    cache.put(evento.request, copia);
                });

                return resposta;

            })
            .catch(() => {

                // sem internet -> usa o que tiver salvo em cache
                return caches.match(evento.request);

            })

    );

});
