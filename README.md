# ♻️ TAMPAÊ

### Gamificação como ferramenta de incentivo aos hábitos sustentáveis

O **TAMPAÊ** é um projeto desenvolvido como Trabalho de Conclusão de Curso do Técnico em Desenvolvimento de Sistemas da **Etec Prof. Carmine Biagio Tundisi**.

A proposta utiliza **gamificação, tecnologia web, Supabase e uma máquina coletora baseada em ESP32** para incentivar a separação e o descarte correto de tampinhas plásticas, transformando uma ação sustentável em uma experiência de participação, acompanhamento e conquista.

---

## 🎯 Sobre o projeto

O TAMPAÊ busca utilizar elementos de jogos — como **pontuação, conquistas, rankings e metas** — para tornar a participação em ações de reciclagem mais atrativa e estimular a repetição de comportamentos sustentáveis.

O projeto integra **software e hardware**. O usuário interage com o aplicativo PWA, enquanto a máquina coletora utiliza um ESP32 para identificar a máquina, consultar sessões e registrar as coletas no backend.

---

## 🧩 Arquitetura geral

O TAMPAÊ é dividido em três camadas principais:

```text
┌─────────────────────────────────────┐
│              USUÁRIO                │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│         PWA / APLICATIVO WEB        │
│ HTML + CSS + JavaScript             │
│ Login • Perfil • Coleta • Ranking   │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│              SUPABASE               │
│ Auth • PostgreSQL • Storage • RPC   │
│ RLS • Functions • Triggers • Views │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│             ESP32 / MÁQUINA         │
│ Wi-Fi • OLED • QR • LDR • Balança  │
│ Botão • Controle da sessão          │
└─────────────────────────────────────┘
```

O **Supabase atua como intermediário** entre o aplicativo e a máquina. O PWA cria ou encerra sessões, enquanto o ESP32 consulta o backend e registra o resultado da coleta por meio das funções disponíveis no banco.

---

## 🔄 Fluxo de uma coleta

O fluxo principal pode ser entendido da seguinte forma:

```text
1. Usuário acessa o TAMPAÊ
          ↓
2. Usuário identifica a máquina por QR Code
          ↓
3. Aplicação cria uma sessão
          ↓
4. ESP32 consulta a sessão no Supabase
          ↓
5. ESP32 identifica o usuário conectado
          ↓
6. Usuário deposita as tampinhas
          ↓
7. Sensores contabilizam a coleta
          ↓
8. ESP32 envia o resultado ao Supabase
          ↓
9. Supabase registra a collection
          ↓
10. Banco atualiza os totais e a pontuação
          ↓
11. Usuário acompanha o resultado no aplicativo
```

A relação fundamental entre os componentes é:

```text
Usuário
  ↓
Sessão
  ↓
Máquina
  ↓
Evento / coleta
  ↓
Pontuação
  ↓
Conquistas + Ranking
```

---

## ✨ Principais funcionalidades

### 👤 Usuários

- Cadastro e autenticação;
- Perfil do usuário;
- Foto de perfil;
- Pontuação acumulada;
- Quantidade total de tampinhas;
- Peso total coletado;
- Histórico de coletas.

### 🎮 Gamificação

- Sistema de pontuação;
- Conquistas baseadas em metas;
- Progresso das conquistas;
- Ranking por evento;
- Acompanhamento da evolução do usuário.

### ♻️ Coletas

O sistema contempla diferentes formas de registro de coleta, de acordo com o fluxo implementado no backend e no hardware.

- **Coleta unitária:** registro da quantidade real de tampinhas;
- **Coleta por peso:** utilização do peso para estimar a quantidade;
- Associação da coleta ao usuário e à máquina;
- Associação da sessão ao evento quando disponível.

### 🏆 Eventos

As coletas podem estar associadas a eventos, permitindo atividades específicas e rankings relacionados a determinada campanha ou período.

### 🤖 Máquina coletora

A máquina utiliza um **ESP32** para:

- identificar a máquina;
- conectar-se ao Wi-Fi;
- consultar sessões ativas;
- identificar o usuário conectado;
- controlar o OLED;
- gerar/exibir QR Code por meio do servidor local;
- detectar passagens pelo LDR;
- utilizar a leitura da balança quando aplicável;
- finalizar sessões;
- enviar o resultado da coleta ao Supabase.

---

## 📁 Estrutura atual do repositório

A estrutura está organizada principalmente em aplicativo e hardware:

```text
TAMPAÊ
│
├── tampae/
│   └── app/
│       ├── js/
│       │   ├── auth.js
│       │   ├── avatar.js
│       │   ├── config.js
│       │   └── supabase.js
│       │
│       ├── pages/
│       │   ├── login/
│       │   ├── inicio/
│       │   ├── coleta/
│       │   ├── mapa/
│       │   ├── perfil/
│       │   └── configura/
│       │
│       ├── assets/
│       └── arquivos do PWA
│
├── hardware/
│   ├── esp32_oled_qr_sessao.ino
│   └── teste_pontuacao.ino
│
└── README.md
```

### `tampae/app/js/`

Contém códigos JavaScript compartilhados pelo aplicativo.

| Arquivo | Função principal |
|---|---|
| `auth.js` | Autenticação, verificação de sessão e logout |
| `avatar.js` | Operações relacionadas ao avatar/foto de perfil |
| `config.js` | Configurações utilizadas pelo aplicativo |
| `supabase.js` | Inicialização e acesso ao cliente Supabase |

### `tampae/app/pages/`

Cada área principal do aplicativo possui seus próprios arquivos de interface e lógica.

| Pasta | Responsabilidade |
|---|---|
| `login/` | Entrada e autenticação do usuário |
| `inicio/` | Tela inicial e informações principais |
| `coleta/` | Fluxo de coleta e QR Code |
| `mapa/` | Visualização dos pontos/máquinas |
| `perfil/` | Perfil, estatísticas e conquistas |
| `configura/` | Configurações e gerenciamento da conta |

### `hardware/`

Contém os firmwares utilizados nos testes e na máquina ESP32.

- `esp32_oled_qr_sessao.ino`: firmware relacionado à máquina, OLED, QR Code e sessão;
- `teste_pontuacao.ino`: firmware utilizado para testes da lógica de pontuação, sensores e encerramento da coleta.

---

## 🧠 Organização do código

O código do aplicativo segue uma separação simples de responsabilidades:

```text
HTML
 ↓
estrutura da página

CSS
 ↓
aparência e layout

JavaScript
 ↓
comportamento da página
 ↓
Supabase
```

Os arquivos JavaScript compartilhados concentram funções utilizadas por diferentes páginas, enquanto cada página possui seu próprio JavaScript para as regras específicas daquela tela.

No hardware, o firmware segue uma lógica semelhante de separação por funções:

```text
setup()
 ↓
Inicialização do ESP32
 ↓
Wi-Fi + OLED + máquina + servidor

loop()
 ↓
Servidor
 ↓
Botão
 ↓
Sensores
 ↓
Consulta de sessão
 ↓
Comunicação com Supabase
```

Os arquivos do projeto também possuem comentários explicativos para documentar a finalidade das principais partes do código sem alterar sua lógica de funcionamento.

---

## 🗄️ Banco de dados

O projeto utiliza **Supabase com PostgreSQL**.

Entre as principais entidades utilizadas pelo sistema estão:

| Tabela | Responsabilidade |
|---|---|
| `profiles` | Dados e informações agregadas dos usuários |
| `machines` | Cadastro e identificação das máquinas |
| `events` | Eventos e campanhas |
| `collections` | Histórico das coletas |
| `machine_sessions` | Sessões entre usuário e máquina |
| `achievements` | Metas e conquistas |

Além das tabelas, o projeto utiliza recursos do PostgreSQL e do Supabase, incluindo:

- Functions / RPC;
- Triggers;
- Views;
- ENUMs;
- Índices;
- Row Level Security (RLS);
- Supabase Auth;
- Supabase Storage.

Os totais de pontos, tampinhas e peso exibidos no perfil estão relacionados às coletas registradas no sistema.

---

## 🔐 Segurança e validação

A arquitetura utiliza o banco para controlar operações importantes relacionadas às coletas.

O fluxo do hardware utiliza informações como:

- `machine_id`;
- `device_token`;
- `session_id`;
- `user_id`;
- `event_id`, quando associado à sessão.

As funções do banco são utilizadas para validar e registrar operações relacionadas às máquinas e sessões.

O projeto também utiliza **Row Level Security (RLS)** para restringir o acesso aos dados conforme as regras definidas no banco.

---

## 📱 PWA

O aplicativo é desenvolvido como **Progressive Web App (PWA)**.

Isso permite que a aplicação web tenha características de aplicativo, incluindo instalação no dispositivo e utilização por meio de uma interface própria.

A estrutura do aplicativo contém os arquivos necessários para o funcionamento do PWA, incluindo manifesto, service worker e recursos visuais.

---

## 🖥️ Hardware

O ESP32 é responsável pela integração física da máquina com o sistema.

### Componentes utilizados no firmware

O código do ESP32 utiliza bibliotecas para:

- Wi-Fi;
- servidor HTTP;
- requisições HTTP;
- JSON;
- comunicação I²C;
- display OLED SSD1306;
- QR Code;
- operações matemáticas.

### Sensores e controles

O firmware possui entradas para:

- **LDR** — utilizado para detectar passagem;
- **balança** — utilizada na lógica de leitura de peso;
- **botão** — utilizado para solicitar o encerramento da sessão;
- **OLED** — utilizado para apresentar o estado da máquina.

### Lógica de teste de pontuação

No firmware `teste_pontuacao.ino`, a lógica atual de teste trabalha com duas formas de contagem:

```text
LDR
 ↓
Cada passagem detectada = 1 tampinha
```

```text
Balança
 ↓
Peso / 13 g
 ↓
Quantidade estimada de tampinhas
```

As coletas são acumuladas durante a sessão e enviadas ao backend quando a sessão é finalizada.

---

## 🌐 Comunicação com o Supabase

A comunicação entre o ESP32 e o Supabase ocorre por HTTP/REST e chamadas RPC.

O fluxo conceitual é:

```text
ESP32
 │
 ├── REST → tabelas
 │
 └── RPC → funções PostgreSQL
              │
              ▼
           Supabase
```

O firmware utiliza chamadas RPC para operações relacionadas à sessão e ao registro da coleta.

---

## 🎮 Gamificação

A gamificação é um dos principais elementos do TAMPAÊ.

O sistema utiliza mecanismos como:

- **Pontos** — recompensa pelas coletas;
- **Conquistas** — metas que podem ser alcançadas pelo usuário;
- **Ranking** — comparação de pontuação entre participantes;
- **Feedback** — acompanhamento do progresso;
- **Metas** — objetivos relacionados às atividades de coleta.

A fundamentação do projeto utiliza referências relacionadas à gamificação e à formação de hábitos, incluindo **Deterding et al. (2011), Fogg (2009), Hamari, Koivisto e Sarsa (2014), Verplanken e Wood (2006) e Zichermann e Cunningham (2011)**.

---

## 🌱 Objetivo

### Objetivo geral

Analisar o uso da gamificação como ferramenta de incentivo à adoção de práticas sustentáveis, com foco na promoção da reciclagem e conscientização ambiental em contextos institucionais.

### Objetivo específico

Desenvolver um software capaz de realizar a gamificação da separação de resíduos.

---

## 📚 Fundamentação

O projeto relaciona três áreas principais:

```text
             TAMPAÊ
                │
      ┌─────────┼─────────┐
      ▼         ▼         ▼
Sustentabilidade Gamificação Tecnologia
      │         │         │
      ▼         ▼         ▼
  Reciclagem  Pontos     PWA
  Resíduos    Metas      Supabase
  Plástico    Ranking    ESP32
  Hábitos     Conquistas PostgreSQL
```

A fundamentação do TCC aborda:

- sustentabilidade;
- resíduos plásticos;
- polipropileno;
- tampinhas plásticas;
- hábitos sustentáveis;
- gamificação;
- motivação e comportamento;
- tecnologia aplicada à sustentabilidade.

---

## 🧪 Estudo de caso

Como parte do desenvolvimento da pesquisa, foi realizada uma entrevista semiestruturada com o **CEO da Youtan**, empresa de desenvolvimento de software.

O estudo de caso buscou compreender a utilização da gamificação em ambientes corporativos e avaliar aspectos relacionados ao projeto TAMPAÊ.

A entrevista contribuiu para validar a arquitetura geral proposta e destacou um dos principais desafios técnicos do projeto: **identificar corretamente os resíduos inseridos na máquina coletora**.

---

## ⚠️ Desafios do projeto

Um dos principais desafios identificados é a **validação física do material descartado**.

Não basta contabilizar a entrada de um objeto. É necessário garantir que o objeto corresponda ao material que o sistema pretende contabilizar.

Possíveis evoluções incluem estudos relacionados a:

- sensores;
- sistemas de identificação;
- visão computacional;
- validação automática dos resíduos;
- aprimoramento do hardware da máquina.

---

## 🚀 Possíveis evoluções

Entre as possibilidades futuras estão:

- sistema mais avançado de identificação das tampinhas;
- visão computacional;
- novos tipos de materiais recicláveis;
- campanhas e eventos institucionais;
- metas coletivas;
- recompensas;
- relatórios administrativos;
- métricas de impacto ambiental;
- expansão para diferentes máquinas;
- aprimoramento da comunicação entre hardware e backend.

---

## 🛠️ Tecnologias

### Software

- HTML5;
- CSS3;
- JavaScript;
- PWA;
- Supabase;
- REST/RPC.

### Banco de dados

- PostgreSQL;
- PL/pgSQL;
- Row Level Security;
- Triggers;
- Views;
- Functions;
- ENUMs;
- Índices.

### Hardware

- ESP32;
- OLED SSD1306;
- LDR;
- Balança/sensor de peso;
- Botão;
- QR Code.

---

## ▶️ Execução e configuração

### Aplicativo

O aplicativo está dentro de `tampae/app/` e pode ser servido como aplicação web/PWA.

Antes da execução, as configurações necessárias do projeto devem estar definidas nos arquivos de configuração utilizados pelo aplicativo, especialmente para a conexão com o Supabase.

### ESP32

Os firmwares estão em `hardware/`.

Para utilizar a máquina, é necessário configurar no firmware os dados correspondentes ao ambiente de rede e ao projeto Supabase antes de gravá-lo no ESP32.

O fluxo de inicialização do firmware é, em linhas gerais:

```text
ESP32 inicia
   ↓
Inicializa Serial
   ↓
Inicializa sensores e OLED
   ↓
Conecta ao Wi-Fi
   ↓
Cria/identifica a máquina no Supabase
   ↓
Inicia servidor web local
   ↓
Aguarda sessão
```

As credenciais e chaves utilizadas no ambiente de desenvolvimento não devem ser expostas publicamente quando contiverem informações privadas ou sensíveis.

---

## 📖 Referências principais

As principais referências utilizadas na fundamentação do projeto incluem:

- DETERDING, S.; DIXON, D.; KHALED, R.; NACKE, L. *From Game Design Elements to Gamefulness: Defining Gamification*. 2011.
- FOGG, B. J. *A Behavior Model for Persuasive Design*. 2009.
- HAMARI, J.; KOIVISTO, J.; SARSA, H. *Does Gamification Work? A Literature Review of Empirical Studies on Gamification*. 2014.
- VERPLANKEN, B.; WOOD, W. *Interventions to Break and Create Consumer Habits*. 2006.
- ZICHERMANN, G.; CUNNINGHAM, C. *Gamification by Design*. 2011.
- CMMAD. *Nosso Futuro Comum*. 1988.
- ONU. *Transformando Nosso Mundo: a Agenda 2030 para o Desenvolvimento Sustentável*. 2015.
- BARBIERI, José Carlos. *Gestão Ambiental Empresarial*. 2016.
- SACHS, Ignacy. *Caminhos para o Desenvolvimento Sustentável*. 2009.

As referências completas estão disponíveis na documentação do TCC.

---

## 🎓 Projeto acadêmico

**TAMPAÊ – A Gamificação como Ferramenta de Incentivo aos Hábitos Sustentáveis**

Projeto desenvolvido no:

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

---

## 📌 Status

> 🚧 Projeto acadêmico em desenvolvimento.

O TAMPAÊ está sendo desenvolvido como Trabalho de Conclusão de Curso, envolvendo desenvolvimento de software, banco de dados e integração com hardware.

---

## ♻️ TAMPAÊ

**Tecnologia para transformar uma ação simples em um hábito sustentável.**
