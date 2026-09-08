import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

const $ = (id) => document.getElementById(id);
const state = { campaigns: [], events: [], machines: [], achievements: [] };

function esc(value = "") {
  return String(value).replace(/[&<>'"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "'": "&#39;", '"': "&quot;" }[c]));
}

function dateInputValue(value) {
  if (!value) return "";
  const d = new Date(value);
  const pad = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

function iso(value) {
  return value ? new Date(value).toISOString() : null;
}

function notify(message) { window.alert(message); }

async function isCurrentUserAdmin(user) {
  const { data, error } = await supabase.from("profiles").select("is_admin").eq("id", user.id).maybeSingle();
  if (error) throw error;
  return data?.is_admin === true;
}

async function claimFirstAdmin() {
  const { data, error } = await supabase.rpc("claim_first_admin");
  if (error) throw error;
  if (!data) throw new Error("Já existe um administrador ou o perfil ainda não está disponível.");
  await initAdmin();
}

function showAccess(message, canClaim = false) {
  $("adminPage").hidden = true;
  $("access").hidden = false;
  $("accessText").textContent = message;
  $("btnClaim").hidden = !canClaim;
}

function setupTabs() {
  document.querySelectorAll(".tab").forEach((button) => {
    button.addEventListener("click", () => {
      document.querySelectorAll(".tab").forEach((b) => b.classList.toggle("active", b === button));
      document.querySelectorAll(".tab-panel").forEach((panel) => panel.hidden = panel.dataset.panel !== button.dataset.tab);
    });
  });
}

async function loadData() {
  const [campaigns, events, machines, achievements] = await Promise.all([
    supabase.from("campaigns").select("*").order("criado_em", { ascending: false }),
    supabase.from("events").select("*").order("data_inicio", { ascending: false }),
    supabase.from("machines").select("*").order("criado_em", { ascending: false }),
    supabase.from("achievements").select("*").order("nome")
  ]);
  for (const result of [campaigns, events, machines, achievements]) if (result.error) throw result.error;
  state.campaigns = campaigns.data || [];
  state.events = events.data || [];
  state.machines = machines.data || [];
  state.achievements = achievements.data || [];
  renderAll();
}

async function loadStats() {
  const { data, error } = await supabase.rpc("admin_campaign_stats");
  if (error) throw error;
  const rows = data || [];
  $("statsGrid").innerHTML = rows.map((r) => `
    <article class="stat-card">
      <h3>${esc(r.campanha_nome)}</h3>
      <div class="stat-meta">${Number(r.eventos)} evento(s) · ${Number(r.usuarios)} participante(s)</div>
      <div class="metrics">
        <div class="metric"><b>${Number(r.pontos).toLocaleString("pt-BR")}</b><small>pontos</small></div>
        <div class="metric"><b>${Number(r.tampinhas).toLocaleString("pt-BR")}</b><small>tampinhas</small></div>
        <div class="metric"><b>${Number(r.peso_gramas).toLocaleString("pt-BR")}</b><small>gramas</small></div>
        <div class="metric"><b>${Number(r.coletas).toLocaleString("pt-BR")}</b><small>coletas</small></div>
      </div>
    </article>
  `).join("");
  $("statsEmpty").hidden = rows.length > 0;
}

function renderCampaigns() {
  $("campanhasList").innerHTML = state.campaigns.map((c) => `
    <div class="list-item">
      <div><strong>${esc(c.nome)}</strong><small>${esc(c.descricao || "Sem descrição")} · ${c.ativo ? "Ativa" : "Inativa"}</small></div>
      <div class="item-actions"><button class="btn ghost" data-edit-campaign="${c.id}">Editar</button><button class="btn danger" data-delete-campaign="${c.id}">Excluir</button></div>
    </div>
  `).join("") || `<div class="empty">Nenhuma campanha cadastrada.</div>`;
}

function renderEvents() {
  const names = Object.fromEntries(state.campaigns.map((c) => [c.id, c.nome]));
  $("eventosList").innerHTML = state.events.map((e) => `
    <div class="list-item">
      <div><strong>${esc(e.nome)}</strong><small>${esc(names[e.campanha_id] || "Sem campanha")} · ${esc(e.status)}</small></div>
      <div class="item-actions"><button class="btn ghost" data-edit-event="${e.id}">Editar</button><button class="btn danger" data-delete-event="${e.id}">Excluir</button></div>
    </div>
  `).join("") || `<div class="empty">Nenhum evento cadastrado.</div>`;
  const options = `<option value="">Sem campanha</option>` + state.campaigns.map((c) => `<option value="${c.id}">${esc(c.nome)}</option>`).join("");
  $("eventoCampanha").innerHTML = options;
}

function renderMachines() {
  $("maquinasList").innerHTML = state.machines.map((m) => `
    <div class="list-item">
      <div><strong>${esc(m.nome)}</strong><small>${esc(m.status)} · ${esc(m.latitude)}, ${esc(m.longitude)} · token ${esc(m.device_token)}</small></div>
      <div class="item-actions"><button class="btn ghost" data-edit-machine="${m.id}">Editar</button><button class="btn danger" data-delete-machine="${m.id}">Excluir</button></div>
    </div>
  `).join("") || `<div class="empty">Nenhuma máquina cadastrada.</div>`;
}

function renderAchievements() {
  const names = Object.fromEntries(state.campaigns.map((c) => [c.id, c.nome]));
  $("conquistasList").innerHTML = state.achievements.map((a) => `
    <div class="list-item">
      <div><strong>${esc(a.nome)}</strong><small>${esc(a.tipo_meta)}: ${Number(a.meta_valor).toLocaleString("pt-BR")} · ${esc(names[a.campanha_id] || "Global")}</small></div>
      <div class="item-actions"><button class="btn ghost" data-edit-achievement="${a.id}">Editar</button><button class="btn danger" data-delete-achievement="${a.id}">Excluir</button></div>
    </div>
  `).join("") || `<div class="empty">Nenhuma conquista cadastrada.</div>`;
  const options = `<option value="">Global</option>` + state.campaigns.map((c) => `<option value="${c.id}">${esc(c.nome)}</option>`).join("");
  $("conquistaCampanha").innerHTML = options;
}

function renderAll() { renderCampaigns(); renderEvents(); renderMachines(); renderAchievements(); }

function resetCampaignForm() {
  $("campanhaForm").reset(); $("campanhaId").value = ""; $("campanhaAtivo").checked = true;
}
function resetEventForm() { $("eventoForm").reset(); $("eventoId").value = ""; }
function resetMachineForm() { $("maquinaForm").reset(); $("maquinaId").value = ""; $("maquinaToken").value = crypto.randomUUID(); $("maquinaStatus").value = "ativa"; }
function resetAchievementForm() { $("conquistaForm").reset(); $("conquistaId").value = ""; $("conquistaTipo").value = "tampinhas"; }

async function saveCampaign(event) {
  event.preventDefault();
  const id = $("campanhaId").value;
  const payload = { nome: $("campanhaNome").value.trim(), descricao: $("campanhaDescricao").value.trim() || null, data_inicio: iso($("campanhaInicio").value), data_fim: iso($("campanhaFim").value), ativo: $("campanhaAtivo").checked };
  const result = id ? await supabase.from("campaigns").update(payload).eq("id", id) : await supabase.from("campaigns").insert(payload);
  if (result.error) throw result.error;
  resetCampaignForm(); await refresh();
}

async function saveEvent(event) {
  event.preventDefault();
  const id = $("eventoId").value;
  const payload = { nome: $("eventoNome").value.trim(), campanha_id: $("eventoCampanha").value || null, descricao: $("eventoDescricao").value.trim() || null, data_inicio: iso($("eventoInicio").value), data_fim: iso($("eventoFim").value), status: $("eventoStatus").value };
  const result = id ? await supabase.from("events").update(payload).eq("id", id) : await supabase.from("events").insert(payload);
  if (result.error) throw result.error;
  resetEventForm(); await refresh();
}

async function saveMachine(event) {
  event.preventDefault();
  const id = $("maquinaId").value;
  const payload = { nome: $("maquinaNome").value.trim(), latitude: Number($("maquinaLatitude").value), longitude: Number($("maquinaLongitude").value), status: $("maquinaStatus").value, device_token: $("maquinaToken").value.trim() || crypto.randomUUID() };
  const result = id ? await supabase.from("machines").update(payload).eq("id", id) : await supabase.from("machines").insert(payload);
  if (result.error) throw result.error;
  resetMachineForm(); await refresh();
}

async function saveAchievement(event) {
  event.preventDefault();
  const id = $("conquistaId").value;
  const payload = { nome: $("conquistaNome").value.trim(), campanha_id: $("conquistaCampanha").value || null, descricao: $("conquistaDescricao").value.trim() || null, tipo_meta: $("conquistaTipo").value, meta_valor: Number($("conquistaMeta").value) };
  const result = id ? await supabase.from("achievements").update(payload).eq("id", id) : await supabase.from("achievements").insert(payload);
  if (result.error) throw result.error;
  resetAchievementForm(); await refresh();
}

function editCampaign(id) { const c = state.campaigns.find((x) => x.id === id); if (!c) return; $("campanhaId").value = c.id; $("campanhaNome").value = c.nome; $("campanhaDescricao").value = c.descricao || ""; $("campanhaInicio").value = dateInputValue(c.data_inicio); $("campanhaFim").value = dateInputValue(c.data_fim); $("campanhaAtivo").checked = c.ativo; }
function editEvent(id) { const e = state.events.find((x) => x.id === id); if (!e) return; $("eventoId").value = e.id; $("eventoNome").value = e.nome; $("eventoCampanha").value = e.campanha_id || ""; $("eventoDescricao").value = e.descricao || ""; $("eventoInicio").value = dateInputValue(e.data_inicio); $("eventoFim").value = dateInputValue(e.data_fim); $("eventoStatus").value = e.status; }
function editMachine(id) { const m = state.machines.find((x) => x.id === id); if (!m) return; $("maquinaId").value = m.id; $("maquinaNome").value = m.nome; $("maquinaLatitude").value = m.latitude; $("maquinaLongitude").value = m.longitude; $("maquinaStatus").value = m.status; $("maquinaToken").value = m.device_token; }
function editAchievement(id) { const a = state.achievements.find((x) => x.id === id); if (!a) return; $("conquistaId").value = a.id; $("conquistaNome").value = a.nome; $("conquistaCampanha").value = a.campanha_id || ""; $("conquistaDescricao").value = a.descricao || ""; $("conquistaTipo").value = a.tipo_meta; $("conquistaMeta").value = a.meta_valor; }

async function remove(table, id, label) {
  if (!confirm(`Excluir ${label}? Essa ação não pode ser desfeita.`)) return;
  const { error } = await supabase.from(table).delete().eq("id", id);
  if (error) throw error;
  await refresh();
}

async function refresh() { await Promise.all([loadData(), loadStats()]); }

function setupForms() {
  $("campanhaForm").addEventListener("submit", (e) => saveCampaign(e).catch((err) => notify(`Erro: ${err.message}`)));
  $("eventoForm").addEventListener("submit", (e) => saveEvent(e).catch((err) => notify(`Erro: ${err.message}`)));
  $("maquinaForm").addEventListener("submit", (e) => saveMachine(e).catch((err) => notify(`Erro: ${err.message}`)));
  $("conquistaForm").addEventListener("submit", (e) => saveAchievement(e).catch((err) => notify(`Erro: ${err.message}`)));
  $("cancelarCampanha").onclick = resetCampaignForm; $("cancelarEvento").onclick = resetEventForm; $("cancelarMaquina").onclick = resetMachineForm; $("cancelarConquista").onclick = resetAchievementForm;
}

function setupActions() {
  document.addEventListener("click", (event) => {
    const t = event.target.closest("button"); if (!t) return;
    if (t.dataset.editCampaign) editCampaign(t.dataset.editCampaign);
    if (t.dataset.deleteCampaign) remove("campaigns", t.dataset.deleteCampaign, "esta campanha").catch((e) => notify(`Erro: ${e.message}`));
    if (t.dataset.editEvent) editEvent(t.dataset.editEvent);
    if (t.dataset.deleteEvent) remove("events", t.dataset.deleteEvent, "este evento").catch((e) => notify(`Erro: ${e.message}`));
    if (t.dataset.editMachine) editMachine(t.dataset.editMachine);
    if (t.dataset.deleteMachine) remove("machines", t.dataset.deleteMachine, "esta máquina").catch((e) => notify(`Erro: ${e.message}`));
    if (t.dataset.editAchievement) editAchievement(t.dataset.editAchievement);
    if (t.dataset.deleteAchievement) remove("achievements", t.dataset.deleteAchievement, "esta conquista").catch((e) => notify(`Erro: ${e.message}`));
  });
}

async function initAdmin() {
  const user = await requireAuth();
  if (!user) return;
  let admin = await isCurrentUserAdmin(user);
  if (!admin) {
    const { count, error } = await supabase.from("profiles").select("id", { count: "exact", head: true }).eq("is_admin", true);
    if (error) throw error;
    if (!count) return showAccess("Ainda não existe um administrador. Se você é o responsável pela implantação, pode ativar o primeiro administrador.", true);
    return showAccess("Sua conta não possui permissão administrativa.");
  }
  $("access").hidden = true;
  $("adminPage").hidden = false;
  setupTabs(); setupForms(); setupActions(); resetMachineForm();
  await refresh();
}

$("btnClaim")?.addEventListener("click", () => claimFirstAdmin().catch((e) => notify(`Não foi possível ativar: ${e.message}`)));
$("btnAtualizar")?.addEventListener("click", () => refresh().catch((e) => notify(`Erro ao atualizar: ${e.message}`)));

initAdmin().catch((error) => showAccess(`Não foi possível carregar o painel: ${error.message}`));
