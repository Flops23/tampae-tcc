// Importa o cliente Supabase para autenticação e acesso ao backend.
import { supabase } from "../../js/supabase.js";

// Referências aos elementos principais da tela de login/cadastro.
const formLogin = document.getElementById("formLogin");
const formCadastro = document.getElementById("formCadastro");
const abaLogin = document.getElementById("abaLogin");
const abaCadastro = document.getElementById("abaCadastro");
const linkParaCadastro = document.getElementById("linkParaCadastro");
const linkParaLogin = document.getElementById("linkParaLogin");

// Remove a tela inicial depois que a verificação de autenticação terminou.
function esconderCarregamentoInicial() {
    const carregamento = document.getElementById("carregamentoInicial");
    carregamento?.classList.add("oculto");
}

// Mostra uma mensagem de erro ou sucesso na interface.
function mostrarMensagem(mensagem, tipo = "erro") {
    const elemento = document.getElementById("mensagem");
    if (!elemento) return;
    elemento.textContent = mensagem;
    elemento.className = `mensagem ${tipo}`;
    elemento.hidden = false;
}

// Limpa e esconde a mensagem atual.
function limparMensagem() {
    const elemento = document.getElementById("mensagem");
    if (!elemento) return;
    elemento.textContent = "";
    elemento.hidden = true;
}

// Desabilita/habilita o botão de envio e altera seu texto durante uma operação.
function setLoading(form, loading) {
    if (!form) return;
    const botao = form.querySelector('button[type="submit"]');
    if (!botao) return;

    if (!botao.dataset.textoOriginal) {
        botao.dataset.textoOriginal = botao.querySelector(".texto-btn")?.textContent || botao.textContent;
    }

    botao.disabled = loading;
    botao.classList.toggle("carregando", loading);

    const texto = botao.querySelector(".texto-btn");
    if (texto) texto.textContent = loading ? "Aguarde..." : botao.dataset.textoOriginal;
}

// Alterna entre o formulário de login e o formulário de cadastro.
function trocarModo(modo) {
    const cadastro = modo === "cadastro";

    abaLogin?.classList.toggle("ativa", !cadastro);
    abaCadastro?.classList.toggle("ativa", cadastro);
    formLogin?.classList.toggle("ativo", !cadastro);
    formCadastro?.classList.toggle("ativo", cadastro);
    limparMensagem();
}

// Configura os botões que permitem mostrar ou ocultar a senha digitada.
function configurarToggleSenha() {
    document.querySelectorAll(".toggle-senha").forEach((botao) => {
        botao.addEventListener("click", () => {
            const alvo = document.getElementById(botao.dataset.alvo);
            const icone = botao.querySelector(".material-symbols-rounded");
            if (!alvo) return;

            const mostrar = alvo.type === "password";
            alvo.type = mostrar ? "text" : "password";
            botao.setAttribute("aria-label", mostrar ? "Ocultar senha" : "Mostrar senha");
            if (icone) icone.textContent = mostrar ? "visibility_off" : "visibility";
        });
    });
}

// Registra o Service Worker responsável pelos recursos PWA/offline definidos no projeto.
function registrarServiceWorker() {
    if (!("serviceWorker" in navigator)) return;

    navigator.serviceWorker.register("../../../sw.js", { scope: "../../../app/" })
        .then(() => console.info("TampAê: Service Worker registrado."))
        .catch((error) => console.error("TampAê: erro ao registrar Service Worker:", error));
}

// Validação simples do formato do endereço de e-mail.
function validarEmail(email) {
    return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email);
}

// Monta a URL da página inicial usando a localização atual do arquivo.
function getHomeUrl() {
    return new URL("../inicio/home.html", window.location.href).href;
}

// Recupera um possível destino de redirecionamento e impede redirecionamentos externos.
function getRedirectPath() {
    const redirect = new URLSearchParams(window.location.search).get("redirect");
    if (!redirect) return getHomeUrl();

    try {
        const destino = new URL(redirect, window.location.origin);
        const appPath = new URL("../../", window.location.href).pathname;
        if (destino.origin === window.location.origin && destino.pathname.startsWith(appPath)) {
            return destino.href;
        }
    } catch (error) {
        console.warn("Redirect inválido ignorado:", error);
    }

    return getHomeUrl();
}

// Verifica se já existe uma sessão autenticada antes de mostrar novamente o login.
async function verificarSessaoExistente() {
    const { data, error } = await supabase.auth.getSession();

    if (error) {
        console.error("Erro ao verificar sessão:", error);
        esconderCarregamentoInicial();
        return;
    }

    if (data.session) {
        // Mantém a tela de carregamento visível enquanto o redirecionamento acontece.
        window.location.replace(getRedirectPath());
        return;
    }

    // Só libera o login depois que o Supabase confirmou que não há sessão.
    esconderCarregamentoInicial();
}

// Valida os dados do formulário e autentica o usuário pelo Supabase Auth.
async function fazerLogin(event) {
    event.preventDefault();
    limparMensagem();

    const email = document.getElementById("loginEmail")?.value.trim();
    const senha = document.getElementById("loginSenha")?.value;

    if (!email || !senha) {
        mostrarMensagem("Preencha o e-mail e a senha.");
        return;
    }
    if (!validarEmail(email)) {
        mostrarMensagem("Digite um e-mail válido.");
        return;
    }

    setLoading(formLogin, true);
    try {
        const { error } = await supabase.auth.signInWithPassword({
            email,
            password: senha
        });

        if (error) {
            console.error("Erro no login:", error);
            mostrarMensagem(traduzirErroAuth(error));
            return;
        }

        window.location.replace(getRedirectPath());
    } finally {
        setLoading(formLogin, false);
    }
}

// Valida os dados do cadastro e cria a conta no Supabase Auth.
async function fazerCadastro(event) {
    event.preventDefault();
    limparMensagem();

    const nome = document.getElementById("cadastroNome")?.value.trim();
    const email = document.getElementById("cadastroEmail")?.value.trim();
    const senha = document.getElementById("cadastroSenha")?.value;
    const confirmarSenha = document.getElementById("cadastroConfirma")?.value;
    const termos = document.getElementById("cadastroTermos")?.checked;

    if (!nome || !email || !senha || !confirmarSenha) {
        mostrarMensagem("Preencha todos os campos do cadastro.");
        return;
    }
    if (!validarEmail(email)) {
        mostrarMensagem("Digite um e-mail válido.");
        return;
    }
    if (senha.length < 6) {
        mostrarMensagem("A senha deve ter pelo menos 6 caracteres.");
        return;
    }
    if (senha !== confirmarSenha) {
        mostrarMensagem("As senhas não coincidem.");
        return;
    }
    if (!termos) {
        mostrarMensagem("Aceite os termos para continuar.");
        return;
    }

    setLoading(formCadastro, true);
    try {
        const { data, error } = await supabase.auth.signUp({
            email,
            password: senha,
            options: {
                data: { nome }
            }
        });

        if (error) {
            console.error("Erro no cadastro:", error);
            mostrarMensagem(traduzirErroAuth(error));
            return;
        }

        if (!data.session) {
            mostrarMensagem("Cadastro realizado! Verifique seu e-mail para confirmar a conta.", "sucesso");
            return;
        }

        mostrarMensagem("Cadastro realizado com sucesso!", "sucesso");
        window.location.replace(getRedirectPath());
    } finally {
        setLoading(formCadastro, false);
    }
}

// Traduz algumas mensagens comuns do Supabase Auth para mensagens em português.
function traduzirErroAuth(error) {
    const mensagem = (error?.message || "").toLowerCase();

    if (mensagem.includes("invalid login credentials")) return "E-mail ou senha incorretos.";
    if (mensagem.includes("email not confirmed")) return "Confirme seu e-mail antes de entrar.";
    if (mensagem.includes("user already registered")) return "Este e-mail já está cadastrado.";
    if (mensagem.includes("password should be at least")) return "A senha precisa ter pelo menos 6 caracteres.";
    if (mensagem.includes("rate limit")) return "Muitas tentativas. Aguarde alguns instantes e tente novamente.";

    return error?.message || "Não foi possível concluir a operação.";
}

// Liga os eventos dos controles da tela às funções correspondentes.
abaLogin?.addEventListener("click", () => trocarModo("login"));
abaCadastro?.addEventListener("click", () => trocarModo("cadastro"));
linkParaCadastro?.addEventListener("click", () => trocarModo("cadastro"));
linkParaLogin?.addEventListener("click", () => trocarModo("login"));
formLogin?.addEventListener("submit", fazerLogin);
formCadastro?.addEventListener("submit", fazerCadastro);

// Inicializa os comportamentos necessários quando a página é carregada.
configurarToggleSenha();
registrarServiceWorker();
verificarSessaoExistente();
