// Cliente do Supabase e proteção da página para usuários autenticados.
import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

// Atalho para buscar elementos pelo id.
const $ = (id) => document.getElementById(id);
const AVATAR_BUCKET = "avatars";

// Formata números conforme o padrão brasileiro.
function formatNumber(value) {
    return new Intl.NumberFormat("pt-BR").format(Number(value) || 0);
}

// Exibe pesos em gramas ou quilogramas conforme o valor.
function formatWeight(grams) {
    const value = Number(grams) || 0;
    return value >= 1000
        ? `${(value / 1000).toLocaleString("pt-BR", { maximumFractionDigits: 2 })} kg`
        : `${value.toLocaleString("pt-BR")} g`;
}

// Busca no banco os dados básicos do perfil e atualiza os indicadores da Home.
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

    // Se existir foto, transforma o caminho do Storage em uma URL pública.
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

// Carrega o evento disponível e calcula a posição do usuário no ranking.
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

    // A view de ranking já fornece a pontuação agregada por usuário no evento.
    const { data: ranking, error: rankError } = await supabase
        .from("vw_ranking_eventos")
        .select("user_id,pontos_total")
        .eq("evento_id", event.id)
        .order("pontos_total", { ascending: false });
    if (rankError) throw rankError;

    const position = (ranking ?? []).findIndex((row) => row.user_id === user.id);
    $("statPosicao").textContent = position >= 0 ? `${position + 1}º` : "—";
}

// Busca uma conquista para mostrar o progresso na Home.
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

    // Converte o progresso atual da conquista em porcentagem para a barra visual.
    const progress = Math.min(100, Number(achievement.meta_valor) ? Number(achievement.progresso_atual || 0) / Number(achievement.meta_valor) * 100 : 0);
    $("conquistaNome").textContent = achievement.nome;
    $("conquistaDescricao").textContent = achievement.descricao || `${achievement.progresso_atual || 0} de ${achievement.meta_valor}`;
    $("conquistaContagem").textContent = achievement.conquistada ? "Concluída" : `${Math.round(progress)}%`;
    $("conquistaBarra").style.width = `${progress}%`;
}

// Mostra o estado vazio do gráfico quando o usuário ainda não possui 3 coletas.
function setupEmptyChart() {
    const canvas = $("grafico");
    const empty = $("graficoVazio");
    if (!canvas || !empty) return;
    canvas.hidden = true;
    empty.hidden = false;
}

// Carrega as coletas do usuário e libera o gráfico somente a partir da terceira coleta.
async function loadChart(user) {
    const canvas = $("grafico");
    const empty = $("graficoVazio");
    if (!canvas || !empty) return;

    const { data: collections, error } = await supabase
        .from("collections")
        .select("criado_em,tampinhas,peso_gramas")
        .eq("user_id", user.id)
        .order("criado_em", { ascending: true });
    if (error) throw error;

    // O gráfico só é exibido depois de 3 coletas registradas.
    if ((collections ?? []).length < 3) {
        setupEmptyChart();
        return;
    }

    canvas.hidden = false;
    empty.hidden = true;

    // Desenha um gráfico simples usando Canvas, sem depender de biblioteca externa.
    const context = canvas.getContext("2d");
    if (!context) return;

    const rect = canvas.getBoundingClientRect();
    const width = Math.max(300, Math.round(rect.width || canvas.clientWidth || 300));
    const height = Math.max(180, Math.round(rect.height || canvas.clientHeight || 180));
    const dpr = window.devicePixelRatio || 1;
    canvas.width = width * dpr;
    canvas.height = height * dpr;
    context.setTransform(dpr, 0, 0, dpr, 0, 0);
    context.clearRect(0, 0, width, height);

    const values = collections.map((collection) => Number(collection.tampinhas) || 0);
    const maxValue = Math.max(...values, 1);
    const padding = { top: 20, right: 16, bottom: 30, left: 36 };
    const chartWidth = width - padding.left - padding.right;
    const chartHeight = height - padding.top - padding.bottom;
    const stepX = values.length > 1 ? chartWidth / (values.length - 1) : chartWidth;

    context.strokeStyle = "#d9dee7";
    context.lineWidth = 1;
    context.beginPath();
    context.moveTo(padding.left, padding.top);
    context.lineTo(padding.left, height - padding.bottom);
    context.lineTo(width - padding.right, height - padding.bottom);
    context.stroke();

    context.strokeStyle = "#2f6fed";
    context.lineWidth = 3;
    context.beginPath();

    values.forEach((value, index) => {
        const x = padding.left + stepX * index;
        const y = height - padding.bottom - (value / maxValue) * chartHeight;
        if (index === 0) context.moveTo(x, y);
        else context.lineTo(x, y);
    });
    context.stroke();

    context.fillStyle = "#2f6fed";
    values.forEach((value, index) => {
        const x = padding.left + stepX * index;
        const y = height - padding.bottom - (value / maxValue) * chartHeight;
        context.beginPath();
        context.arc(x, y, 4, 0, Math.PI * 2);
        context.fill();
    });

    context.fillStyle = "#555";
    context.font = "12px sans-serif";
    context.textAlign = "center";
    values.forEach((value, index) => {
        const x = padding.left + stepX * index;
        context.fillText(String(index + 1), x, height - 10);
    });
}

// Formata uma data para dia/mês no padrão brasileiro.
function formatDate(value) {
    return new Intl.DateTimeFormat("pt-BR", { day: "2-digit", month: "2-digit" }).format(new Date(value));
}

// Escapa caracteres HTML antes de inserir valores em trechos de HTML.
function escapeHtml(value) {
    return String(value ?? "").replace(/[&<>\"]/g, (char) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[char]));
}

// Inicializa a Home somente depois de confirmar a autenticação do usuário.
async function initHome() {
    const user = await requireAuth();
    if (!user) return;
    try {
        await loadProfile(user);
        await Promise.all([loadEventAndRanking(user), loadAchievement(user)]);
        await loadChart(user);
    } catch (error) {
        console.error("Falha ao carregar a Home:", error);
    }
}

initHome();
