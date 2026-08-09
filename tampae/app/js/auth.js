import { supabase } from "./supabase.js";

export async function getSession() {
    const { data, error } = await supabase.auth.getSession();

    if (error) {
        console.error("Erro ao obter sessão:", error);
        return null;
    }

    return data.session ?? null;
}

export async function getUser() {
    const session = await getSession();
    return session?.user ?? null;
}

export async function requireAuth(loginPath = "/app/pages/login/index.html") {
    const user = await getUser();

    if (!user) {
        const currentPath = `${window.location.pathname}${window.location.search}`;
        const separator = loginPath.includes("?") ? "&" : "?";
        const redirectUrl = `${loginPath}${separator}redirect=${encodeURIComponent(currentPath)}`;

        window.location.replace(redirectUrl);
        return null;
    }

    return user;
}

export async function logout(loginPath = "/app/pages/login/index.html") {
    const { error } = await supabase.auth.signOut();

    if (error) {
        console.error("Erro ao sair:", error);
        throw error;
    }

    window.location.replace(loginPath);
}

export function onAuthStateChange(callback) {
    const { data } = supabase.auth.onAuthStateChange((event, session) => {
        callback(event, session);
    });

    return data.subscription;
}
