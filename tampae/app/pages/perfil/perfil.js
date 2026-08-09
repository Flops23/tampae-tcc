import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

const $ = (id) => document.getElementById(id);

async function init() {
    const user = await requireAuth();
    if (!user) return;

    $("perfilEmail").textContent = user.email ?? "—";
    $("perfilDesde").textContent = `membro desde ${new Intl.DateTimeFormat("pt-BR", { month: "long", year: "numeric" }).format(new Date(user.created_at))}`;

    const { data: profile, error } = await supabase
        .from("profiles")
        .select("id,nome,foto_url,pontos_totais,tampinhas_totais,peso_total_gramas")
        .eq("id", user.id)
        .maybeSingle();

    if (error || !profile) {
        console.error(error);
        return;
    }

    renderProfile(profile);
    await Promise.all([loadAchievements(user.id), loadHistory(user.id)]);

    $("btnEditarFoto")?.addEventListener("click", async () => {
        const url = window.prompt("URL da sua foto de perfil (deixe vazio para remover):", profile.foto_url ?? "");
        if (url === null) return;
        const { error: updateError } = await supabase.from("profiles").update({ foto_url: url.trim() || null }).eq("id", user.id);
        if (updateError) return alert("Não foi possível atualizar a foto.");
        window.location.reload();
    });

    $("btnEditarPerfil")?.addEventListener("click", async () => {
        const nome = window.prompt("Como você quer ser chamado?", profile.nome ?? "");
        if (nome === null) return;
        const novoNome = nome.trim();
        if (!novoNome) return alert("O nome não pode ficar vazio.");
        const { error: updateError } = await supabase.from("profiles").update({ nome: novoNome }).eq("id", user.id);
        if (updateError) return alert("Não foi possível atualizar seu nome.");
        window.location.reload();
    });
}

function renderProfile(profile) {
    const nome = profile.nome || "Usuário";
    $("perfilNome").textContent = nome;
    $("avatarIniciais").textContent = initials(nome);
    $("statPontos").textContent = Number(profile.pontos_totais || 0).toLocaleString("pt-BR");
    $("statTampinhas").textContent = Number(profile.tampinhas_totais || 0).toLocaleString("pt-BR");
    $("statPeso").textContent = formatWeight(profile.peso_total_gramas);

    if (profile.foto_url) {
        $("avatarIniciais").style.backgroundImage = `url("${profile.foto_url.replaceAll('"', '%22')}")`;
        $("avatarIniciais").style.backgroundSize = "cover";
        $("avatarIniciais").style.backgroundPosition = "center";
        $("avatarIniciais").textContent = "";
    }
}

async function loadAchievements(userId) {
    const { data, error } = await supabase.from("vw_conquistas_usuario").select("achievement_id,nome,descricao,tipo_meta,meta_valor,progresso_atual,conquistada").eq("user_id", userId).order("meta_valor");
    if (error) {
        console.error(error);
        return;
    }

    const list = $("conquistasLista");
    const rows = data ?? [];
    const done = rows.filter((row) => row.conquistada).length;
    $("conquistasContagem").textContent = `${done}/${rows.length}`;

    list.innerHTML = rows.length ? rows.map((row) => {
        const progress = Math.min(100, Number(row.meta_valor) ? Number(row.progresso_atual || 0) / Number(row.meta_valor) * 100 : 0);
        return `<div class="conquista ${row.conquistada ? "concluida" : ""}"><div class="conquista-icone"><span class="material-symbols-rounded">${row.conquistada ? "verified" : "emoji_events"}</span></div><b>${escapeHtml(row.nome)}</b><small>${escapeHtml(row.descricao || "")}</small><div class="barra"><span style="width:${progress}%"></span></div></div>`;
    }).join("") : `<div class="empty-state">Nenhuma conquista cadastrada ainda.</div>`;
}

async function loadHistory(userId) {
    const { data, error } = await supabase.from("collections").select("id,tipo_coleta,quantidade_real,quantidade_estimada,peso_real_gramas,peso_estimado_gramas,pontos,criado_em,machines(nome)").eq("user_id", userId).order("criado_em", { ascending: false }).limit(5);
    if (error) {
        console.error(error);
        return;
    }

    const list = $("historicoLista");
    const rows = data ?? [];
    list.innerHTML = rows.length ? rows.map((row) => {
        const quantity = row.quantidade_real ?? row.quantidade_estimada;
        const weight = row.peso_real_gramas ?? row.peso_estimado_gramas;
        const detail = quantity != null ? `${quantity} tampinhas` : `${formatWeight(weight)}`;
        return `<div class="historico-item"><div class="icone"><span class="material-symbols-rounded">recycling</span></div><div class="texto"><b>${escapeHtml(row.machines?.nome || "Coleta")}</b><small>${detail} • ${new Date(row.criado_em).toLocaleDateString("pt-BR")}</small></div><strong>+${Number(row.pontos || 0)} pts</strong></div>`;
    }).join("") : `<div class="empty-state">Você ainda não realizou coletas.</div>`;
}

function formatWeight(grams) {
    const value = Number(grams || 0);
    if (value >= 1000) return `${(value / 1000).toLocaleString("pt-BR", { maximumFractionDigits: 2 })} kg`;
    return `${value.toLocaleString("pt-BR", { maximumFractionDigits: 0 })} g`;
}

function initials(name) {
    return String(name).trim().split(/\s+/).slice(0, 2).map((part) => part[0]).join("").toUpperCase();
}

function escapeHtml(value) {
    return String(value ?? "").replace(/[&<>\"]/g, (char) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[char]));
}

init();
