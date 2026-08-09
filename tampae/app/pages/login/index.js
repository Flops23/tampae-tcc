import { supabase } from "../../js/supabase.js";

const formLogin = document.getElementById("formLogin");
const formCadastro = document.getElementById("formCadastro");
const abaLogin = document.getElementById("abaLogin");
const abaCadastro = document.getElementById("abaCadastro");
const linkParaCadastro = document.getElementById("linkParaCadastro");
const linkParaLogin = document.getElementById("linkParaLogin");

function mostrarMensagem(mensagem, tipo = "erro") {
    const elemento = document.getElementById("mensagem");
    if (!elemento) return;
    elemento.textContent = mensagem;
    elemento.className = `mensagem ${tipo}`;
    elemento.hidden = false;
}

function limparMensagem() {
    const elemento = document.getElementById("mensagem");
    if (!elemento) return;
    elemento.textContent = "";
    elemento.hidden = true;
}

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

function trocarModo(modo) {
    const cadastro = modo === "cadastro";

    abaLogin?.classList.toggle("ativa", !cadastro);
    abaCadastro?.classList.toggle("ativa", cadastro);
    formLogin?.classList.toggle("ativo", !cadastro);
    formCadastro?.classList.toggle("ativo", cadastro);
    limparMensagem();
}

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

function registrarServiceWorker() {
    if (!("serviceWorker" in navigator)) return;

    navigator.serviceWorker.register("../../../sw.js", { scope: "../../../" })
        .then(() => console.info("TampAê: Service Worker registrado."))
        .catch((error) => console.error("TampAê: erro ao registrar Service Worker:", error));
}

function validarEmail(email) {
    return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email);
}

function getRedirectPath() {
    const redirect = new URLSearchParams(window.location.search).get("redirect");
    return redirect && redirect.startsWith("/app/")
        ? redirect
        : "/app/pages/inicio/home.html";
}

async function verificarSessaoExistente() {
    const { data, error } = await supabase.auth.getSession();
    if (error) {
        console.error("Erro ao verificar sessão:", error);
        return;
    }
    if (data.session) window.location.replace(getRedirectPath());
}

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
            mostrarMensagem(
                "Cadastro realizado! Verifique seu e-mail para confirmar a conta.",
                "sucesso"
            );
            return;
        }

        mostrarMensagem("Cadastro realizado com sucesso!", "sucesso");
        window.location.replace(getRedirectPath());
    } finally {
        setLoading(formCadastro, false);
    }
}

function traduzirErroAuth(error) {
    const mensagem = (error?.message || "").toLowerCase();

    if (mensagem.includes("invalid login credentials")) return "E-mail ou senha incorretos.";
    if (mensagem.includes("email not confirmed")) return "Confirme seu e-mail antes de entrar.";
    if (mensagem.includes("user already registered")) return "Este e-mail já está cadastrado.";
    if (mensagem.includes("password should be at least")) return "A senha precisa ter pelo menos 6 caracteres.";
    if (mensagem.includes("rate limit")) return "Muitas tentativas. Aguarde alguns instantes e tente novamente.";

    return error?.message || "Não foi possível concluir a operação.";
}

abaLogin?.addEventListener("click", () => trocarModo("login"));
abaCadastro?.addEventListener("click", () => trocarModo("cadastro"));
linkParaCadastro?.addEventListener("click", () => trocarModo("cadastro"));
linkParaLogin?.addEventListener("click", () => trocarModo("login"));
formLogin?.addEventListener("submit", fazerLogin);
formCadastro?.addEventListener("submit", fazerCadastro);

configurarToggleSenha();
registrarServiceWorker();
verificarSessaoExistente();
