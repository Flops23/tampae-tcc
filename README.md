# ♻️ TAMPAÊ

### Gamificação como ferramenta de incentivo aos hábitos sustentáveis

O **TAMPAÊ** é um Trabalho de Conclusão de Curso do Técnico em Desenvolvimento de Sistemas da **Etec Prof. Carmine Biagio Tundisi**.

O projeto utiliza **gamificação, PWA, Supabase e ESP32** para incentivar o descarte correto de tampinhas plásticas e transformar a reciclagem em uma experiência de participação e acompanhamento.

## 💡 Como funciona

```text
Usuário
   ↓
PWA TAMPAÊ
   ↓
Supabase
   ↓
ESP32 / Máquina coletora
   ↓
Coleta
   ↓
Pontos • Conquistas • Ranking
```

O aplicativo cria e acompanha as sessões de coleta. A máquina utiliza o ESP32 para se comunicar com o Supabase, realizar a coleta e registrar os dados.

## ✨ Principais funcionalidades

- Cadastro e autenticação de usuários;
- Perfil e foto de usuário;
- Registro de coletas;
- Pontuação;
- Conquistas;
- Ranking;
- Eventos;
- Mapa de máquinas/pontos de coleta;
- Integração com máquina coletora ESP32;
- PWA instalável.

## 🏗️ Arquitetura

O projeto possui três partes principais:

**PWA** — interface do usuário, desenvolvida com HTML, CSS e JavaScript.

**Supabase** — backend e banco PostgreSQL, responsável por autenticação, dados, funções, sessões, coletas e armazenamento.

**ESP32** — controla a máquina coletora, OLED, QR Code e sensores, além da comunicação com o backend.

## 📁 Estrutura

```text
TAMPAÊ
│
├── tampae/
│   └── app/
│       ├── js/
│       └── pages/
│           ├── login/
│           ├── inicio/
│           ├── coleta/
│           ├── mapa/
│           ├── perfil/
│           └── configura/
│
├── hardware/
│   ├── esp32_oled_qr_sessao.ino
│   └── teste_pontuacao.ino
│
└── README.md
```

Os arquivos do projeto possuem comentários explicativos para facilitar o entendimento do código sem alterar sua lógica de funcionamento.

## 🛠️ Tecnologias

- HTML5
- CSS3
- JavaScript
- PWA
- Supabase
- PostgreSQL
- ESP32
- OLED SSD1306
- LDR
- Balança/sensor de peso
- QR Code

## 🎓 Projeto acadêmico

**TAMPAÊ – A Gamificação como Ferramenta de Incentivo aos Hábitos Sustentáveis**

**Curso Técnico em Desenvolvimento de Sistemas**  
**Etec Prof. Carmine Biagio Tundisi**

### Autores

- Felipe Alves de Azevedo
- Luísa Marinho Lucena
- Manoela Carolina da Silva Carvalho
- Mateus Amorim de Souza

### Orientadores

- Carlos Augusto Gomes
- Leonardo Victor Ferreira de Lima

## 📌 Status

🚧 Projeto acadêmico em desenvolvimento.

O TAMPAÊ reúne desenvolvimento web, banco de dados, gamificação e integração com hardware para incentivar hábitos sustentáveis.
