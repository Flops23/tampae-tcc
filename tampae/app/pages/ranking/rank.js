import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

const $ = (id) => document.getElementById(id);
let eventos = [];
let eventoAtual = null;

function formatDate(value) {
    return new Intl.DateTimeFormat("pt-BR", { day: "2-digit", month: "2-digit" }).format(new Date(value));
}

function statusLabel(status) {
    return ({ agendado: "Agendado", em_andamento: "Em andamento", encerrado: "Encerrado" })[status] ?? status;
}

function renderEventSelector() {
    const host = $("eventSelect");
    if (!host) return;
    host.innerHTML = "";

    if (!eventos.length) {
        host.innerHTML = `<div class="info"><span class="material-symbols-rounded">emoji_events</span><div><b>Nenhum evento</b><small>Crie um evento para iniciar o ranking</small></div></div><span class="badge">Sem dados</span>`;
        return;
    }

    const event = eventoAtual ?? eventos[0];
    host.innerHTML = `
        <div class="info">
            <span class="material-symbols-rounded">emoji_events</span>
            <div><b>${escapeHtml(event.nome)}</b><small>${formatDate(event.data_inicio)} — ${formatDate(event.data_fim)}</small></div>
        </div>
        <span class="badge">${statusLabel(event.status)}</span>
        <select id="eventoSelectInput" aria-label="Selecionar evento">
            ${eventos.map((e) => `<option value="${e.id}" ${e.id === event.id ? "selected" : ""}>${escapeHtml(e.nome)}</option>`).join("")}
        </select>`;

    $("eventoSelectInput")?.addEventListener("change", (e) => {
        eventoAtual = eventos.find((item) => item.id === e.target.value) ?? eventos[0];
        loadRanking();
    });
}

function renderRanking(rows) {
    const podium = $("podium");
    const list = $("rankList");
    podium.innerHTML = "";
    list.innerHTML = "";

    if (!rows.length) {
        list.innerHTML = `<div class="empty-state">Ainda não há coletas neste evento.</div>`;
        return;
    }

    const medals = ["🥇", "🥈", "🥉"];
    podium.innerHTML = rows.slice(0, 3).map((row, index) => `
        <div class="podium-item">
            <div class="podium-avatar">${initials(row.nome)}</div>
            <b>${escapeHtml(row.nome)}</b>
            <strong>${Number(row.pontos_total).toLocaleString("pt-BR")} pts</strong>
            <span>${medals[index]}</span>
        </div>`).join("");

    list.innerHTML = rows.map((row, index) => `
        <div class="rank-row">
            <span class="posicao">${index + 1}</span>
            <div class="avatar">${initials(row.nome)}</div>
            <div class="nome"><b>${escapeHtml(row.nome)}</b><small>${Number(row.pontos_total).toLocaleString("pt-BR")} pontos</small></div>
        </div>`).join("");
}

async function loadRanking() {
    renderEventSelector();
    if (!eventoAtual) return renderRanking([]);

    const { data, error } = await supabase
        .from("vw_ranking_eventos")
        .select("evento_id,user_id,nome,pontos_total")
        .eq("evento_id", eventoAtual.id)
        .order("pontos_total", { ascending: false });

    if (error) {
        console.error(error);
        renderRanking([]);
        return;
    }

    renderRanking(data ?? []);
}

async function init() {
    const user = await requireAuth();
    if (!user) return;

    const { data, error } = await supabase
        .from("events")
        .select("id,nome,descricao,data_inicio,data_fim,status")
        .order("data_inicio", { ascending: false });

    if (error) {
        console.error(error);
        eventos = [];
    } else {
        eventos = data ?? [];
        eventoAtual = eventos.find((e) => e.status === "em_andamento") ?? eventos[0] ?? null;
    }

    await loadRanking();
}

function initials(name) {
    return String(name || "U").trim().split(/\s+/).slice(0, 2).map((part) => part[0]).join("").toUpperCase();
}

function escapeHtml(value) {
    return String(value ?? "").replace(/[&<>\"]/g, (char) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[char]));
}

init();
