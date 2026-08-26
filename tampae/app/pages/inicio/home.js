// Cliente do Supabase e proteção da página para usuários autenticados.
import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";
import { loadAppData } from "../../js/app-data.js";

const $ = (id) => document.getElementById(id);
const AVATAR_BUCKET = "avatars";
let chartState = { points: [], dates: [], counts: [], dayPoints: [] };
let tooltipTimer = null;

function formatNumber(value) { return new Intl.NumberFormat("pt-BR").format(Number(value) || 0); }
function formatWeight(grams) { const value = Number(grams) || 0; return value >= 1000 ? `${(value / 1000).toLocaleString("pt-BR", { maximumFractionDigits: 2 })} kg` : `${value.toLocaleString("pt-BR")} g`; }

function loadProfile(appData) {
    const profile = appData.profile;
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
            img.src = `${data.publicUrl}?v=${encodeURIComponent(appData.loadedAt)}`;
            img.alt = "Foto de perfil";
            link.replaceChildren(img);
        }
    }
}

async function loadEvent(user) {
    const { data: events, error } = await supabase.from("events").select("id,nome,data_inicio,data_fim,status").in("status", ["em_andamento", "agendado"]).order("data_inicio", { ascending: true });
    if (error) throw error;
    const event = events?.find((e) => e.status === "em_andamento") ?? events?.[0];
    if (!event) {
        $("eventoNome").textContent = "Nenhum evento disponível";
        $("eventoData").textContent = "Novos eventos aparecerão aqui";
        return;
    }
    $("eventoTag").textContent = event.status === "em_andamento" ? "Em andamento" : "Próximo evento";
    $("eventoNome").textContent = event.nome;
    $("eventoData").textContent = `${formatDate(event.data_inicio)} — ${formatDate(event.data_fim)}`;
}

async function loadAchievement(user) {
    const { data, error } = await supabase.from("vw_conquistas_usuario").select("nome,descricao,meta_valor,progresso_atual,conquistada").eq("user_id", user.id).order("conquistada", { ascending: false }).order("meta_valor", { ascending: true }).limit(1);
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

function setupEmptyChart() { const canvas = $("grafico"); const empty = $("graficoVazio"); if (!canvas || !empty) return; canvas.hidden = true; empty.hidden = false; }

function groupCollectionsByDay(collections) {
    const groups = new Map();
    collections.forEach((collection) => {
        const date = new Date(collection.criado_em);
        const key = new Intl.DateTimeFormat("en-CA", { year: "numeric", month: "2-digit", day: "2-digit" }).format(date);
        const current = groups.get(key) || { pontos: 0, quantidade: 0 };
        current.pontos += Number(collection.pontos) || 0;
        current.quantidade += 1;
        groups.set(key, current);
    });
    let acumulado = 0;
    return [...groups.entries()].map(([key, value]) => {
        acumulado += value.pontos;
        return { key, pontos: acumulado, pontosDoDia: value.pontos, quantidade: value.quantidade, data: new Intl.DateTimeFormat("pt-BR", { day: "2-digit", month: "2-digit", year: "numeric" }).format(new Date(`${key}T12:00:00`)) };
    });
}

function drawChart() {
    const canvas = $("grafico"); if (!canvas || canvas.hidden || !chartState.points.length) return;
    const context = canvas.getContext("2d"); if (!context) return;
    const rect = canvas.getBoundingClientRect(); const width = Math.max(300, Math.round(rect.width || 300)); const height = Math.max(180, Math.round(rect.height || 180)); const dpr = window.devicePixelRatio || 1;
    canvas.width = width * dpr; canvas.height = height * dpr; context.setTransform(dpr, 0, 0, dpr, 0, 0); context.clearRect(0, 0, width, height);
    const values = chartState.points; const maxValue = Math.max(...values, 1); const padding = { top: 20, right: 20, bottom: 38, left: 42 }; const chartWidth = width - padding.left - padding.right; const chartHeight = height - padding.top - padding.bottom; const stepX = values.length > 1 ? chartWidth / (values.length - 1) : chartWidth;
    const points = values.map((value, index) => ({ x: padding.left + stepX * index, y: height - padding.bottom - (value / maxValue) * chartHeight, value, index }));
    context.strokeStyle = "#edf1ed"; context.lineWidth = 1; context.font = "11px sans-serif"; context.fillStyle = "#7a817d"; context.textAlign = "right";
    for (let i = 0; i <= 3; i++) { const y = padding.top + (chartHeight / 3) * i; const label = Math.round(maxValue - (maxValue / 3) * i); context.beginPath(); context.moveTo(padding.left, y); context.lineTo(width - padding.right, y); context.stroke(); context.fillText(String(label), padding.left - 8, y + 4); }
    const gradient = context.createLinearGradient(0, padding.top, 0, height - padding.bottom); gradient.addColorStop(0, "rgba(34,197,94,.28)"); gradient.addColorStop(1, "rgba(34,197,94,0)"); context.beginPath(); context.moveTo(points[0].x, height - padding.bottom); points.forEach((point) => context.lineTo(point.x, point.y)); context.lineTo(points[points.length - 1].x, height - padding.bottom); context.closePath(); context.fillStyle = gradient; context.fill();
    context.strokeStyle = "#16a34a"; context.lineWidth = 3; context.lineJoin = "round"; context.lineCap = "round"; context.beginPath(); points.forEach((point, index) => index === 0 ? context.moveTo(point.x, point.y) : context.lineTo(point.x, point.y)); context.stroke();
    context.fillStyle = "#fff"; context.strokeStyle = "#16a34a"; context.lineWidth = 3; points.forEach((point) => { context.beginPath(); context.arc(point.x, point.y, 5, 0, Math.PI * 2); context.fill(); context.stroke(); });
    context.fillStyle = "#7a817d"; context.font = "11px sans-serif"; context.textAlign = "center"; points.forEach((point) => context.fillText(chartState.dates[point.index].slice(0, 5), point.x, height - 12));
}

function showChartTooltip(index, clientX, clientY, isTouch = false) {
    const tooltip = $("graficoTooltip"); const boxElement = $("graficoBox"); if (!tooltip || !boxElement || index < 0 || index >= chartState.points.length) return;
    tooltip.hidden = false; tooltip.innerHTML = `<strong>${chartState.dates[index]}</strong><span>${formatNumber(chartState.points[index])} pontos acumulados</span><small>+${formatNumber(chartState.dayPoints[index])} no dia · ${chartState.counts[index]} ${chartState.counts[index] === 1 ? "coleta" : "coletas"}</small>`;
    const box = boxElement.getBoundingClientRect();
    if (isTouch) { tooltip.style.left = "50%"; tooltip.style.top = "8px"; tooltip.style.transform = "translateX(-50%)"; return; }
    tooltip.style.transform = "translateX(-50%)"; tooltip.style.left = `${Math.max(8, Math.min(clientX - box.left, box.width - tooltip.offsetWidth - 8))}px`; tooltip.style.top = `${Math.max(8, clientY - box.top - tooltip.offsetHeight - 12)}px`;
}
function hideChartTooltip() { const tooltip = $("graficoTooltip"); if (tooltip) tooltip.hidden = true; }
function scheduleTouchTooltipHide() { clearTimeout(tooltipTimer); tooltipTimer = setTimeout(hideChartTooltip, 3500); }
function bindChartInteraction() {
    const canvas = $("grafico"); if (!canvas || canvas.dataset.interactive === "true") return; canvas.dataset.interactive = "true";
    const locatePoint = (event) => { if (!chartState.points.length) return -1; const rect = canvas.getBoundingClientRect(); const x = (event.clientX ?? 0) - rect.left; const paddingLeft = 42; const paddingRight = 20; const chartWidth = Math.max(1, rect.width - paddingLeft - paddingRight); const stepX = chartState.points.length > 1 ? chartWidth / (chartState.points.length - 1) : chartWidth; return Math.max(0, Math.min(chartState.points.length - 1, Math.round((x - paddingLeft) / stepX))); };
    canvas.addEventListener("pointermove", (event) => { const index = locatePoint(event); showChartTooltip(index, event.clientX, event.clientY, event.pointerType === "touch"); if (event.pointerType === "touch") scheduleTouchTooltipHide(); });
    canvas.addEventListener("pointerdown", (event) => { const index = locatePoint(event); showChartTooltip(index, event.clientX, event.clientY, event.pointerType === "touch"); if (event.pointerType === "touch") scheduleTouchTooltipHide(); });
    canvas.addEventListener("pointerup", (event) => { if (event.pointerType === "touch") scheduleTouchTooltipHide(); });
    canvas.addEventListener("pointerleave", (event) => { if (event.pointerType !== "touch") hideChartTooltip(); });
}

function loadChart(appData) {
    const canvas = $("grafico"); const empty = $("graficoVazio"); if (!canvas || !empty) return;
    const collections = appData.collections ?? [];
    if (collections.length < 3) { setupEmptyChart(); return; }
    const daily = groupCollectionsByDay(collections);
    canvas.hidden = false; empty.hidden = true; chartState.points = daily.map((day) => day.pontos); chartState.dates = daily.map((day) => day.data); chartState.counts = daily.map((day) => day.quantidade); chartState.dayPoints = daily.map((day) => day.pontosDoDia); bindChartInteraction(); requestAnimationFrame(drawChart);
    if (!window.__tampaeChartResizeBound) { window.__tampaeChartResizeBound = true; window.addEventListener("resize", drawChart); }
}

function formatDate(value) { return new Intl.DateTimeFormat("pt-BR", { day: "2-digit", month: "2-digit" }).format(new Date(value)); }

async function initHome() {
    const user = await requireAuth(); if (!user) return;
    try {
        const appData = await loadAppData();
        if (!appData) return;
        loadProfile(appData);
        loadChart(appData);
        // Ranking continua fora do estado compartilhado por enquanto, conforme definido.
        await Promise.all([loadEvent(user), loadAchievement(user)]);
    } catch (error) { console.error("Falha ao carregar a Home:", error); }
}

initHome();
