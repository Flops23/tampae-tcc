import { supabase } from "../../js/supabase.js";

const formLogin = document.getElementById("formLogin");
const formCadastro = document.getElementById("formCadastro");

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
    if (!botao.dataset.textoOriginal) botao.dataset.textoOriginal = botao.textContent;
    botao.disabled = loading;
    botao.textContent = loading ? "Aguarde..." : botao.dataset.textoOriginal;
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

    const email = document.getElementById("email")?.value.trim();
    const senha = document.getElementById("senha")?.value;

    if (!email || !senha) return mostrarMensagem("Preencha o e-mail e a senha.");
    if (!validarEmail(email)) return mostrarMensagem("Digite um e-mail válido.");

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

    const nome = document.getElementById("nome")?.value.trim();
    const email = document.getElementById("emailCadastro")?.value.trim();
    const senha = document.getElementById("senhaCadastro")?.value;
    const confirmarSenha = document.getElementById("confirmarSenha")?.value;
    const termos = document.getElementById("termos")?.checked;

    if (!nome || !email || !senha || !confirmarSenha) {
        return mostrarMensagem("Preencha todos os campos do cadastro.");
    }
    if (!validarEmail(email)) return mostrarMensagem("Digite um e-mail válido.");
    if (senha.length < 6) return mostrarMensagem("A senha deve ter pelo menos 6 caracteres.");
    if (senha !== confirmarSenha) return mostrarMensagem("As senhas não coincidem.");
    if (!termos) return mostrarMensagem("Aceite os termos para continuar.");

    setLoading(formCadastro, true);
    try {
        const { data, error } = await supabase.auth.signUp({
            email,
            password: senha,
            options: { data: { nome } }
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

if (formLogin) formLogin.addEventListener("submit", fazerLogin);
if (formCadastro) formCadastro.addEventListener("submit", fazerCadastro);

verificarSessaoExistente();
