# TampAê

Sistema de incentivo à coleta correta de tampinhas plásticas por gamificação.
Composto por um aplicativo PWA e uma máquina de coleta com ESP32.

## Estrutura de pastas

```
tampae/
├── index.html                → Página de divulgação (vitrine) — abre na raiz do domínio
├── manifest.json               → Configuração do PWA (nome, ícones, start_url)
├── sw.js                        → Service worker (escopo "/", cobre também /app)
├── icons/                        → Ícones do app (192px, 512px, maskable)
├── README.md
│
└── app/                       → O aplicativo PWA de verdade
    ├── index.html             → Login / Cadastro (start_url do manifest)
    ├── home.html                → Painel principal (pontos, conquistas, evento ativo)
    ├── mapa.html                  → Mapa com pontos de coleta + rota até o mais próximo
    ├── qr.html                     → Leitor de QR Code + sessão com a máquina
    ├── rank.html                    → Ranking (pódio + lista) por evento
    ├── perfil.html                   → Perfil do usuário, estatísticas e histórico
    └── configuracoes.html            → Preferências, alterar senha, sair da conta
```

## Por que separado assim

- **`index.html` (raiz)** é a vitrine — a página que a maioria das hospedagens
  estáticas (Netlify, GitHub Pages, Vercel...) abre automaticamente quando
  alguém visita o domínio. É **ela** que carrega o `manifest.json` e
  registra o `sw.js`, então é aqui — e só aqui — que o navegador oferece o
  prompt real de instalação do PWA. Os botões "Baixar app" chamam esse
  prompt diretamente; não existe nenhum link da vitrine pra dentro do site,
  só a instalação.
- **`app/`** é o aplicativo em si, com seu próprio `index.html` (login).
  Como o `manifest.json` tem `scope: "/"`, o mesmo service worker registrado
  na raiz já cobre tudo dentro de `/app` — as páginas do app não precisam
  (nem têm) nenhum código de instalação, só o serviço em si. O `start_url`
  do manifest aponta pra `app/index.html`, então é essa tela que abre
  quando alguém toca no ícone instalado.

## Como hospedar

Qualquer hospedagem de arquivos estáticos com **HTTPS** funciona (o
navegador exige HTTPS — ou `localhost` — pra habilitar o service worker
e o prompt de instalação). Alguns exemplos gratuitos: GitHub Pages,
Netlify, Vercel, Cloudflare Pages.

Basta subir a pasta `tampae/` inteira mantendo a estrutura acima —
**arraste a pasta `tampae` em si** (não o que está dentro dela) pro
Netlify, por exemplo, assim o `index.html` da raiz fica realmente na
raiz do site publicado.

- a vitrine (com o botão de instalar) fica em `seudominio.com/`
- o app instalado abre em `seudominio.com/app/`

## Sobre os dados

O projeto ainda não está conectado ao Supabase — todas as telas usam
dados mocados dentro de cada `<script>` (arrays como `usuario`,
`pontosDeColeta`, `participantes`, etc.), já estruturados pra bater com
as tabelas planejadas (`users`, `machines`, `collections`, `events`,
`achievements`, `rankings`). Quando o backend entrar, é só trocar esses
mocks pelas queries reais.
