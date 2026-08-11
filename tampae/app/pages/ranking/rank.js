import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

const $ = (id) => document.getElementById(id);
const AVATAR_BUCKET = "avatars";
let eventos = [];
let eventoAtual = null;
let usuarioAtualId = null;
let avatars = new Map();

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
            <div>
                <b>${escapeHtml(event.nome)}</b>
                <small>${formatDate(event.data_inicio)} — ${formatDate(event.data_fim)}</small>
            </div>
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

function initials(name) {
    return String(name || "U").trim().split(/\s+/).slice(0, 2).map((part) => part[0]).join("").toUpperCase();
}

function corAvatar(nome) {
    const cores = [
        "#16a34a", "#0ea5e9", "#a855f7", "#f97316", "#ec4899",
        "#14b8a6", "#6366f1", "#eab308", "#ef4444", "#22c55e"
    ];

    let soma = 0;
    for (let i = 0; i < nome.length; i++) soma += nome.charCodeAt(i);
    return cores[soma % cores.length];
}

function avatarMarkup(pessoa, extraClass = "") {
    const path = avatars.get(pessoa.user_id);
    if (path) {
        const { data } = supabase.storage.from(AVATAR_BUCKET).getPublicUrl(path);
        if (data?.publicUrl) {
            return `<div class="avatar ${extraClass}" style="background-image:url('${data.publicUrl}?v=${encodeURIComponent(path)}');background-size:cover;background-position:center;background-repeat:no-repeat" aria-label="Foto de ${escapeHtml(pessoa.nome)}"></div>`;
        }
    }

    return `<div class="avatar ${extraClass}" style="background:${corAvatar(pessoa.nome)}">${initials(pessoa.nome)}</div>`;
}

async function loadAvatars(rows) {
    const ids = [...new Set(rows.map((row) => row.user_id).filter(Boolean))];
    avatars = new Map();
    if (!ids.length) return;

    const { data, error } = await supabase
        .from("profiles")
        .select("id,foto_path")
        .in("id", ids);

    if (error) {
        console.error("Falha ao carregar fotos do ranking:", error);
        return;
    }

    for (const profile of data ?? []) {
        if (profile.foto_path) avatars.set(profile.id, profile.foto_path);
    }
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

    const rankeados = [...rows].sort((a, b) => Number(b.pontos_total) - Number(a.pontos_total)).slice(0, 10);
    const [primeiro, segundo, terceiro] = rankeados;

    const ordemVisual = [
        { pessoa: segundo, classe: "second", pos: 2 },
        { pessoa: primeiro, classe: "first", pos: 1 },
        { pessoa: terceiro, classe: "third", pos: 3 }
    ].filter((item) => item.pessoa);

    podium.innerHTML = ordemVisual.map(({ pessoa, classe, pos }) => `
        <div class="podium-item ${classe}">
            <div class="avatar-wrap">
                ${pos === 1 ? '<span class="material-symbols-rounded crown">workspace_premium</span>' : ''}
                ${avatarMarkup(pessoa)}
                <div class="medal">${pos}</div>
            </div>
            <div class="nome">${escapeHtml(pessoa.nome)}</div>
            <div class="pontos">${Number(pessoa.pontos_total).toLocaleString("pt-BR")} pts</div>
            <div class="base">${pos}</div>
        </div>
    `).join("");

    list.innerHTML = rankeados.slice(3).map((pessoa, i) => {
        const posicao = i + 4;
        const destaque = pessoa.user_id === usuarioAtualId;

        return `
            <div class="rank-row ${destaque ? "me" : ""}">
                <div class="pos">${posicao}º</div>
                ${avatarMarkup(pessoa)}
                <div class="nome">
                    ${escapeHtml(pessoa.nome)}
                    ${destaque ? "<small>Você</small>" : ""}
                </div>
                <div class="pontos">
                    <span class="material-symbols-rounded">recycling</span>
                    ${Number(pessoa.pontos_total).toLocaleString("pt-BR")}
                </div>
            </div>`;
    }).join("");
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

    const rows = data ?? [];
    await loadAvatars(rows);
    renderRanking(rows);
}

async function init() {
    const user = await requireAuth();
    if (!user) return;

    usuarioAtualId = user.id;

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

function escapeHtml(value) {
    return String(value ?? "").replace(/[&<>\"]/g, (char) => ({
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;"
    }[char]));
}

init();
