// Importa o cliente Supabase já configurado pelo projeto.
import { supabase } from "./supabase.js";

// Obtém a sessão atualmente armazenada pelo Supabase Auth.
export async function getSession() {
    const { data, error } = await supabase.auth.getSession();

    // Se houver erro, registra no console e informa que não há sessão disponível.
    if (error) {
        console.error("Erro ao obter sessão:", error);
        return null;
    }

    // Retorna a sessão atual ou null caso o usuário não esteja autenticado.
    return data.session ?? null;
}

// Obtém somente o usuário associado à sessão atual.
export async function getUser() {
    const session = await getSession();
    return session?.user ?? null;
}

// Exige que exista um usuário autenticado para permanecer na página atual.
export async function requireAuth(loginPath = "/app/pages/login/index.html") {
    const user = await getUser();

    // Caso não exista usuário, prepara o retorno para a página de login.
    if (!user) {
        const currentPath = `${window.location.pathname}${window.location.search}`;
        const separator = loginPath.includes("?") ? "&" : "?";
        const redirectUrl = `${loginPath}${separator}redirect=${encodeURIComponent(currentPath)}`;

        // Redireciona o visitante para o login e informa qual página deveria ser aberta depois.
        window.location.replace(redirectUrl);
        return null;
    }

    // Usuário autenticado: devolve os dados do usuário para quem chamou a função.
    return user;
}

// Encerra a sessão atual no Supabase e volta para a tela de login.
export async function logout(loginPath = "/app/pages/login/index.html") {
    const { error } = await supabase.auth.signOut();

    // Se o logout falhar, registra o erro e repassa a exceção.
    if (error) {
        console.error("Erro ao sair:", error);
        throw error;
    }

    // Após sair, envia o usuário para a página de login.
    window.location.replace(loginPath);
}

// Registra uma função para ser chamada quando o estado de autenticação mudar.
export function onAuthStateChange(callback) {
    const { data } = supabase.auth.onAuthStateChange((event, session) => {
        // Entrega para o código chamador o evento e a sessão atualizada.
        callback(event, session);
    });

    // Retorna a inscrição para que ela possa ser encerrada quando necessário.
    return data.subscription;
}
