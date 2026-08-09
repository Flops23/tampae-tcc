# ♻️ TAMPAÊ

### Gamificação como ferramenta de incentivo aos hábitos sustentáveis

O **TAMPAÊ** é um projeto desenvolvido como Trabalho de Conclusão de Curso do Técnico em Desenvolvimento de Sistemas da **Etec Prof. Carmine Biagio Tundisi**.

A proposta consiste em utilizar **gamificação, tecnologia web e uma máquina coletora com ESP32** para incentivar a separação e o descarte correto de tampinhas plásticas, transformando uma ação sustentável em uma experiência de participação, acompanhamento e conquista.

---

## 🎯 Sobre o projeto

A preocupação com a geração e o descarte inadequado de resíduos plásticos torna necessária a criação de estratégias capazes de incentivar hábitos sustentáveis.

O TAMPAÊ busca atuar justamente nesse ponto: utilizar elementos de jogos — como **pontuação, conquistas, rankings e metas** — para tornar a participação em ações de reciclagem mais atrativa e estimular a repetição desses comportamentos.

A proposta está fundamentada na ideia de que a conscientização ambiental, quando acompanhada de mecanismos de motivação e feedback, pode favorecer o engajamento dos indivíduos em práticas sustentáveis.

O projeto também busca integrar **software e hardware**, permitindo que o usuário interaja com uma máquina coletora por meio de um aplicativo web.

---

## 💡 Como funciona

O fluxo principal do TAMPAÊ foi pensado para conectar o usuário, o aplicativo, o banco de dados e a máquina coletora:

```text
┌──────────────┐
│    Usuário   │
└──────┬───────┘
       │
       │ QR Code
       ▼
┌──────────────┐
│     PWA      │
│  Aplicativo  │
└──────┬───────┘
       │
       │ cria sessão
       ▼
┌──────────────┐
│   Supabase   │
│  PostgreSQL  │
└──────┬───────┘
       │
       │ consulta sessão
       ▼
┌──────────────┐
│    ESP32     │
│    Máquina   │
└──────┬───────┘
       │
       │ registra coleta
       ▼
┌──────────────┐
│  Pontuação   │
│  Conquistas  │
│   Ranking    │
└──────────────┘
```

O aplicativo e a máquina não precisam se comunicar diretamente. O **Supabase atua como intermediário**, permitindo que o ESP32 consulte uma sessão criada pelo usuário e registre a coleta posteriormente.

---

## ✨ Principais funcionalidades

### 👤 Usuários

* Cadastro e autenticação;
* Perfil do usuário;
* Foto de perfil;
* Pontuação acumulada;
* Quantidade total de tampinhas;
* Peso total coletado;
* Histórico de coletas.

### 🎮 Gamificação

* Sistema de pontuação;
* Conquistas baseadas em metas;
* Progresso das conquistas;
* Ranking por evento;
* Acompanhamento da evolução do usuário.

### ♻️ Coletas

O sistema contempla diferentes formas de registro:

* **Coleta unitária**

  * quantidade real de tampinhas;
  * peso estimado.

* **Coleta por peso**

  * peso real;
  * quantidade estimada.

Cada coleta é vinculada ao usuário e à máquina responsável pelo registro.

### 🏆 Eventos

As coletas podem estar associadas a eventos, permitindo a criação de atividades específicas e rankings relacionados a determinada campanha ou período.

### 🤖 Máquina coletora

A máquina utiliza um **ESP32** para:

* identificar a máquina;
* consultar sessões disponíveis;
* receber a identificação do usuário;
* registrar a coleta;
* comunicar-se com o backend por meio do Supabase.

---

## 🏗️ Arquitetura

A arquitetura do TAMPAÊ é composta principalmente por três partes:

### Frontend

Aplicação **PWA (Progressive Web App)** responsável pela interação com o usuário.

Entre suas responsabilidades estão:

* autenticação;
* perfil;
* histórico;
* pontuação;
* conquistas;
* rankings;
* interação com as máquinas.

### Backend / Banco de dados

O projeto utiliza **Supabase com PostgreSQL**.

O banco possui entidades para:

* `profiles`
* `machines`
* `events`
* `collections`
* `machine_sessions`
* `achievements`

Além disso, o sistema utiliza:

* ENUMs;
* índices;
* triggers;
* views;
* funções PostgreSQL;
* Row Level Security (RLS).

### Hardware

A máquina coletora utiliza **ESP32** para realizar a comunicação com o backend e controlar o processo de coleta.

---

## 🗄️ Banco de dados

O banco foi estruturado buscando manter a integridade dos dados e separar as responsabilidades de cada entidade.

### Principais tabelas

| Tabela             | Responsabilidade                |
| ------------------ | ------------------------------- |
| `profiles`         | Dados e totais dos usuários     |
| `machines`         | Cadastro e estado das máquinas  |
| `events`           | Eventos e campanhas             |
| `collections`      | Histórico das coletas           |
| `machine_sessions` | Sessões entre usuário e máquina |
| `achievements`     | Metas e conquistas              |

Os totais de pontos, tampinhas e peso presentes no perfil são mantidos como valores agregados a partir das coletas registradas.

O banco também possui uma trigger responsável por atualizar esses totais após uma nova coleta.

---

## 🔐 Segurança

A segurança é uma parte importante da arquitetura do TAMPAÊ.

O aplicativo não deve realizar diretamente alterações que possam modificar a pontuação do usuário.

As coletas são registradas por meio de funções do PostgreSQL que validam:

* máquina;
* `device_token`;
* sessão;
* validade da sessão;
* usuário relacionado à sessão.

O banco também utiliza **Row Level Security (RLS)** para restringir o acesso aos dados dos usuários.

Dessa forma, um usuário não deve conseguir simplesmente enviar uma requisição direta criando uma coleta para aumentar sua própria pontuação.

---

## 🔄 Fluxo de uma coleta

```text
1. Usuário acessa o TAMPAÊ
          ↓
2. Usuário identifica a máquina
          ↓
3. Aplicação cria uma sessão
          ↓
4. ESP32 consulta o Supabase
          ↓
5. ESP32 encontra a sessão
          ↓
6. Usuário realiza o descarte
          ↓
7. Máquina registra a coleta
          ↓
8. Supabase grava a collection
          ↓
9. Trigger atualiza os totais
          ↓
10. Usuário recebe a pontuação
```

As sessões possuem tempo de expiração, evitando que uma autorização antiga permaneça disponível indefinidamente.

---

## 🏆 Gamificação

A gamificação é um dos principais elementos do TAMPAÊ.

O sistema utiliza mecanismos como:

* **Pontos** — recompensa pelas coletas;
* **Conquistas** — metas que podem ser alcançadas pelo usuário;
* **Ranking** — comparação de pontuação entre participantes de eventos;
* **Feedback** — acompanhamento do progresso;
* **Metas** — objetivos relacionados a tampinhas, peso ou pontos.

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

* sustentabilidade;
* resíduos plásticos;
* polipropileno;
* tampinhas plásticas;
* hábitos sustentáveis;
* gamificação;
* motivação e comportamento;
* tecnologia aplicada à sustentabilidade.

---

## 🧪 Estudo de caso

Como parte do desenvolvimento da pesquisa, foi realizada uma entrevista semiestruturada com o **CEO da Youtan**, empresa de desenvolvimento de software.

O estudo de caso buscou compreender a utilização da gamificação em ambientes corporativos e avaliar aspectos relacionados ao projeto TAMPAÊ.

A entrevista contribuiu para validar a arquitetura geral proposta e destacou um dos principais desafios técnicos do projeto: **identificar corretamente os resíduos inseridos na máquina coletora**.

Esse ponto é especialmente importante para garantir a confiabilidade das pontuações e evitar registros indevidos.

---

## ⚠️ Desafios do projeto

Um dos principais desafios identificados é a **validação física do material descartado**.

Não basta contabilizar a entrada de um objeto. É necessário garantir que o objeto corresponda ao material que o sistema pretende contabilizar.

Por isso, possíveis evoluções do projeto incluem estudos relacionados a:

* sensores;
* sistemas de identificação;
* visão computacional;
* validação automática dos resíduos;
* aprimoramento do hardware da máquina.

---

## 🚀 Possíveis evoluções

O TAMPAÊ possui espaço para expansão além do escopo inicial.

Entre as possibilidades futuras estão:

* sistema mais avançado de identificação das tampinhas;
* visão computacional;
* novos tipos de materiais recicláveis;
* campanhas e eventos institucionais;
* metas coletivas;
* recompensas;
* relatórios administrativos;
* métricas de impacto ambiental;
* expansão para diferentes máquinas;
* aprimoramento da comunicação entre hardware e backend.

---

## 🛠️ Tecnologias

### Software

* PWA
* Supabase
* PostgreSQL
* REST/RPC

### Banco de dados

* PostgreSQL
* PL/pgSQL
* Row Level Security
* Triggers
* Views
* Functions

### Hardware

* ESP32
* Máquina coletora
* Sensores de identificação/coleta

---

## 📁 Estrutura conceitual

```text
TAMPAÊ
│
├── PWA
│   ├── Autenticação
│   ├── Perfil
│   ├── Histórico
│   ├── Conquistas
│   └── Ranking
│
├── Backend
│   └── Supabase
│       ├── Auth
│       ├── PostgreSQL
│       ├── Functions
│       ├── Views
│       └── RLS
│
├── Hardware
│   └── ESP32
│       └── Máquina coletora
│
└── Documentação
    └── TCC
```

---

## 📖 Referências principais

As principais referências utilizadas na fundamentação do projeto incluem:

* DETERDING, S.; DIXON, D.; KHALED, R.; NACKE, L. *From Game Design Elements to Gamefulness: Defining Gamification*. 2011.
* FOGG, B. J. *A Behavior Model for Persuasive Design*. 2009.
* HAMARI, J.; KOIVISTO, J.; SARSA, H. *Does Gamification Work? A Literature Review of Empirical Studies on Gamification*. 2014.
* VERPLANKEN, B.; WOOD, W. *Interventions to Break and Create Consumer Habits*. 2006.
* ZICHERMANN, G.; CUNNINGHAM, C. *Gamification by Design*. 2011.
* CMMAD. *Nosso Futuro Comum*. 1988.
* ONU. *Transformando Nosso Mundo: a Agenda 2030 para o Desenvolvimento Sustentável*. 2015.
* BARBIERI, José Carlos. *Gestão Ambiental Empresarial*. 2016.
* SACHS, Ignacy. *Caminhos para o Desenvolvimento Sustentável*. 2009.

As referências completas estão disponíveis na documentação do TCC.

---

## 🎓 Projeto acadêmico

**TAMPAÊ – A Gamificação como Ferramenta de Incentivo aos Hábitos Sustentáveis**

Projeto desenvolvido no:

**Curso Técnico em Desenvolvimento de Sistemas**
**Etec Prof. Carmine Biagio Tundisi**

### Autores

* Felipe Alves de Azevedo
* Luísa Marinho Lucena
* Manoela Carolina da Silva Carvalho
* Mateus Amorim de Souza

### Orientadores

* Carlos Augusto Gomes
* Leonardo Victor Ferreira de Lima

---

## 📌 Status

> 🚧 Projeto acadêmico em desenvolvimento.

O TAMPAÊ está sendo desenvolvido como Trabalho de Conclusão de Curso, envolvendo desenvolvimento de software, banco de dados e integração com hardware.

---

## ♻️ TAMPAÊ

**Tecnologia para transformar uma ação simples em um hábito sustentável.**
