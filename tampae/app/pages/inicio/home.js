// Cliente do Supabase e proteção da página para usuários autenticados.
import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

// Atalho para buscar elementos pelo id.
const $ = (id) => document.getElementById(id);
const AVATAR_BUCKET = "avatars";
let chartState = { points: [], dates: [] };

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

// Desenha o gráfico com área preenchida, pontos destacados e interação por toque/mouse.
function drawChart() {
    const canvas = $("grafico");
    if (!canvas || canvas.hidden || !chartState.points.length) return;

    const context = canvas.getContext("2d");
    if (!context) return;
    const rect = canvas.getBoundingClientRect();
    const width = Math.max(300, Math.round(rect.width || 300));
    const height = Math.max(180, Math.round(rect.height || 180));
    const dpr = window.devicePixelRatio || 1;
    canvas.width = width * dpr;
    canvas.height = height * dpr;
    context.setTransform(dpr, 0, 0, dpr, 0, 0);
    context.clearRect(0, 0, width, height);

    const values = chartState.points;
    const maxValue = Math.max(...values, 1);
    const padding = { top: 20, right: 20, bottom: 34, left: 42 };
    const chartWidth = width - padding.left - padding.right;
    const chartHeight = height - padding.top - padding.bottom;
    const stepX = values.length > 1 ? chartWidth / (values.length - 1) : chartWidth;
    const points = values.map((value, index) => ({
        x: padding.left + stepX * index,
        y: height - padding.bottom - (value / maxValue) * chartHeight,
        value,
        index
    }));

    context.strokeStyle = "#edf1ed";
    context.lineWidth = 1;
    context.font = "11px sans-serif";
    context.fillStyle = "#7a817d";
    context.textAlign = "right";
    for (let i = 0; i <= 3; i++) {
        const y = padding.top + (chartHeight / 3) * i;
        const label = Math.round(maxValue - (maxValue / 3) * i);
        context.beginPath();
        context.moveTo(padding.left, y);
        context.lineTo(width - padding.right, y);
        context.stroke();
        context.fillText(String(label), padding.left - 8, y + 4);
    }

    // Área sob a linha para dar profundidade visual ao gráfico.
    const gradient = context.createLinearGradient(0, padding.top, 0, height - padding.bottom);
    gradient.addColorStop(0, "rgba(34,197,94,.28)");
    gradient.addColorStop(1, "rgba(34,197,94,0)");
    context.beginPath();
    context.moveTo(points[0].x, height - padding.bottom);
    points.forEach((point) => context.lineTo(point.x, point.y));
    context.lineTo(points[points.length - 1].x, height - padding.bottom);
    context.closePath();
    context.fillStyle = gradient;
    context.fill();

    context.strokeStyle = "#16a34a";
    context.lineWidth = 3;
    context.lineJoin = "round";
    context.lineCap = "round";
    context.beginPath();
    points.forEach((point, index) => index === 0 ? context.moveTo(point.x, point.y) : context.lineTo(point.x, point.y));
    context.stroke();

    context.fillStyle = "#fff";
    context.strokeStyle = "#16a34a";
    context.lineWidth = 3;
    points.forEach((point) => {
        context.beginPath();
        context.arc(point.x, point.y, 5, 0, Math.PI * 2);
        context.fill();
        context.stroke();
    });

    context.fillStyle = "#7a817d";
    context.font = "11px sans-serif";
    context.textAlign = "center";
    points.forEach((point) => context.fillText(String(point.index + 1), point.x, height - 11));
}

// Exibe os detalhes da coleta mais próxima do toque ou mouse.
function showChartTooltip(index, clientX, clientY) {
    const tooltip = $("graficoTooltip");
    if (!tooltip || index < 0 || index >= chartState.points.length) return;
    tooltip.hidden = false;
    tooltip.innerHTML = `<strong>Coleta ${index + 1}</strong><span>${formatNumber(chartState.points[index])} pontos</span><small>${chartState.dates[index]}</small>`;
    const box = $("graficoBox").getBoundingClientRect();
    tooltip.style.left = `${Math.max(8, Math.min(clientX - box.left, box.width - tooltip.offsetWidth - 8))}px`;
    tooltip.style.top = `${Math.max(8, clientY - box.top - tooltip.offsetHeight - 12)}px`;
}

function hideChartTooltip() {
    const tooltip = $("graficoTooltip");
    if (tooltip) tooltip.hidden = true;
}

function bindChartInteraction() {
    const canvas = $("grafico");
    if (!canvas || canvas.dataset.interactive === "true") return;
    canvas.dataset.interactive = "true";

    const locatePoint = (event) => {
        if (!chartState.points.length) return -1;
        const rect = canvas.getBoundingClientRect();
        const x = (event.clientX ?? event.touches?.[0]?.clientX ?? 0) - rect.left;
        const width = rect.width;
        const paddingLeft = 42;
        const paddingRight = 20;
        const chartWidth = width - paddingLeft - paddingRight;
        const stepX = chartState.points.length > 1 ? chartWidth / (chartState.points.length - 1) : chartWidth;
        let index = Math.round((x - paddingLeft) / stepX);
        index = Math.max(0, Math.min(chartState.points.length - 1, index));
        return index;
    };

    canvas.addEventListener("pointermove", (event) => {
        showChartTooltip(locatePoint(event), event.clientX, event.clientY);
    });
    canvas.addEventListener("pointerleave", hideChartTooltip);
    canvas.addEventListener("pointerdown", (event) => {
        showChartTooltip(locatePoint(event), event.clientX, event.clientY);
    });
}

async function loadChart(user) {
    const canvas = $("grafico");
    const empty = $("graficoVazio");
    if (!canvas || !empty) return;

    const { data: collections, error } = await supabase
        .from("collections")
        .select("criado_em,pontos")
        .eq("user_id", user.id)
        .order("criado_em", { ascending: true });
    if (error) throw error;

    if ((collections ?? []).length < 3) {
        setupEmptyChart();
        return;
    }

    canvas.hidden = false;
    empty.hidden = true;
    chartState.points = collections.map((collection) => Number(collection.pontos) || 0);
    chartState.dates = collections.map((collection) => new Intl.DateTimeFormat("pt-BR", { day: "2-digit", month: "2-digit", year: "numeric" }).format(new Date(collection.criado_em)));
    bindChartInteraction();
    requestAnimationFrame(drawChart);
    window.addEventListener("resize", drawChart);
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
        await loadChart(user);
    } catch (error) {
        console.error("Falha ao carregar a Home:", error);
    }
}

initHome();
