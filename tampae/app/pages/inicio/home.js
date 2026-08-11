import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

const $ = (id) => document.getElementById(id);
const AVATAR_BUCKET = "avatars";

function formatNumber(value) {
    return new Intl.NumberFormat("pt-BR").format(Number(value) || 0);
}

function formatWeight(grams) {
    const value = Number(grams) || 0;
    return value >= 1000
        ? `${(value / 1000).toLocaleString("pt-BR", { maximumFractionDigits: 2 })} kg`
        : `${value.toLocaleString("pt-BR")} g`;
}

async function loadProfile(user) {
    const { data: profile, error } = await supabase
        .from("profiles")
        .select("nome,pontos_totais,peso_total_gramas,tampinhas_totais,foto_path")
        .eq("id", user.id)
        .maybeSingle();
    if (error) throw error;
    if (!profile) return;

    $("saudacaoNome").textContent = profile.nome || "Usuário";
    $("pontosTotais").textContent = formatNumber(profile.pontos_totais);
    $("statTampinhas").textContent = formatNumber(profile.tampinhas_totais);
    $("statPeso").textContent = formatWeight(profile.peso_total_gramas);

    if (profile.foto_path) {
        const { data } = supabase.storage.from(AVATAR_BUCKET).getPublicUrl(profile.foto_path);
        const link = document.querySelector(".profile");
        if (link && data?.publicUrl) {
            const img = document.createElement("img");
            img.src = `${data.publicUrl}?v=${Date.now()}`;
            img.alt = "Foto de perfil";
            link.replaceChildren(img);
        }
    }
}

async function loadEventAndRanking(user) {
    const { data: events, error } = await supabase
        .from("events")
        .select("id,nome,data_inicio,data_fim,status")
        .in("status", ["em_andamento", "agendado"])
        .order("data_inicio", { ascending: true });
    if (error) throw error;

    const event = events?.find((e) => e.status === "em_andamento") ?? events?.[0];
    if (!event) {
        $("eventoNome").textContent = "Nenhum evento disponível";
        $("eventoData").textContent = "Novos eventos aparecerão aqui";
        $("statPosicao").textContent = "—";
        return;
    }

    $("eventoTag").textContent = event.status === "em_andamento" ? "Em andamento" : "Próximo evento";
    $("eventoNome").textContent = event.nome;
    $("eventoData").textContent = `${formatDate(event.data_inicio)} — ${formatDate(event.data_fim)}`;

    const { data: ranking, error: rankError } = await supabase
        .from("vw_ranking_eventos")
        .select("user_id,pontos_total")
        .eq("evento_id", event.id)
        .order("pontos_total", { ascending: false });
    if (rankError) throw rankError;

    const position = (ranking ?? []).findIndex((row) => row.user_id === user.id);
    $("statPosicao").textContent = position >= 0 ? `${position + 1}º` : "—";
}

async function loadAchievement(user) {
    const { data, error } = await supabase
        .from("vw_conquistas_usuario")
        .select("nome,descricao,meta_valor,progresso_atual,conquistada")
        .eq("user_id", user.id)
        .order("conquistada", { ascending: false })
        .order("meta_valor", { ascending: true })
        .limit(1);
    if (error) throw error;

    const achievement = data?.[0];
    if (!achievement) {
        $("conquistaNome").textContent = "Conquistas";
        $("conquistaDescricao").textContent = "As metas aparecerão aqui quando estiverem disponíveis.";
        $("conquistaContagem").textContent = "—";
        $("conquistaBarra").style.width = "0%";
        return;
    }

    const progress = Math.min(100, Number(achievement.meta_valor) ? Number(achievement.progresso_atual || 0) / Number(achievement.meta_valor) * 100 : 0);
    $("conquistaNome").textContent = achievement.nome;
    $("conquistaDescricao").textContent = achievement.descricao || `${achievement.progresso_atual || 0} de ${achievement.meta_valor}`;
    $("conquistaContagem").textContent = achievement.conquistada ? "Concluída" : `${Math.round(progress)}%`;
    $("conquistaBarra").style.width = `${progress}%`;
}

function setupEmptyChart() {
    const canvas = $("grafico");
    const empty = $("graficoVazio");
    if (!canvas || !empty) return;
    canvas.hidden = true;
    empty.hidden = false;
}

function formatDate(value) {
    return new Intl.DateTimeFormat("pt-BR", { day: "2-digit", month: "2-digit" }).format(new Date(value));
}

function escapeHtml(value) {
    return String(value ?? "").replace(/[&<>\"]/g, (char) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[char]));
}

async function initHome() {
    const user = await requireAuth();
    if (!user) return;
    try {
        await loadProfile(user);
        await Promise.all([loadEventAndRanking(user), loadAchievement(user)]);
        setupEmptyChart();
    } catch (error) {
        console.error("Falha ao carregar a Home:", error);
    }
}

initHome();
