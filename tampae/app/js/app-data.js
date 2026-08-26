// Carregamento central dos dados básicos do usuário.
// A primeira abertura do aplicativo faz a leitura no Supabase.
// As demais páginas reutilizam o resultado salvo em sessionStorage.

import { supabase } from "./supabase.js";
import { getSession } from "./auth.js";
import { getAppState, setAppState, isAppStateForUser, isAppStateInvalidated, markAppStateFresh } from "./app-state.js";

export async function loadAppData(force = false) {
    const session = await getSession();
    const user = session?.user ?? null;
    if (!user) return null;

    const cached = getAppState();
    if (!force && isAppStateForUser(user.id) && !isAppStateInvalidated()) {
        return cached;
    }

    const { data: profile, error: profileError } = await supabase
        .from("profiles")
        .select("id,nome,pontos_totais,peso_total_gramas,tampinhas_totais,foto_path")
        .eq("id", user.id)
        .maybeSingle();
    if (profileError) throw profileError;

    const { data: collections, error: collectionsError } = await supabase
        .from("collections")
        .select("id,tipo_coleta,quantidade_real,quantidade_estimada,peso_real_gramas,peso_estimado_gramas,pontos,criado_em,machines(nome)")
        .eq("user_id", user.id)
        .order("criado_em", { ascending: false });
    if (collectionsError) throw collectionsError;

    const data = {
        userId: user.id,
        user: {
            email: user.email ?? null,
            created_at: user.created_at ?? null
        },
        profile: profile ?? null,
        collections: collections ?? [],
        invalidated: false,
        loadedAt: Date.now()
    };

    setAppState(data);
    markAppStateFresh();
    return data;
}

export function getCachedAppData() {
    return getAppState();
}
