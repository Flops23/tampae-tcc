// Cliente do Supabase e proteção da página para usuários autenticados.
import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

const $ = (id) => document.getElementById(id);

let stream = null;
let scanning = false;
let animationFrame = null;
let sessionTimer = null;
let sessionWatcher = null;
let currentSession = null;
let currentUser = null;
let finalizando = false;

// Tempo de segurança para a máquina detectar e registrar as últimas tampinhas.
const JANELA_FINALIZACAO_MS = 5000;

function showState(state) {
    ["scanOverlay", "statePermissao", "stateErro", "stateConectando", "sessionView", "stateFinalizando", "stateEncerrada"].forEach((id) => {
        const element = $(id);
        if (element) element.style.display = "none";
    });
    const target = $(state);
    if (target) target.style.display = state === "scanOverlay" ? "block" : "flex";
}

async function stopCamera() {
    scanning = false;
    if (animationFrame) cancelAnimationFrame(animationFrame);
    animationFrame = null;
    if (stream) stream.getTracks().forEach((track) => track.stop());
    stream = null;
    if ($("video")) $("video").srcObject = null;
}

async function startCamera() {
    await stopCamera();
    if (!navigator.mediaDevices?.getUserMedia) {
        $("erroTexto").textContent = "Seu navegador não oferece acesso à câmera.";
        showState("stateErro");
        return;
    }
    try {
        stream = await navigator.mediaDevices.getUserMedia({ video: { facingMode: { ideal: "environment" } }, audio: false });
        $("video").srcObject = stream;
        await $("video").play();
        scanning = true;
        showState("scanOverlay");
        scanFrame();
    } catch (error) {
        console.error(error);
        $("erroTexto").textContent = "Permita a câmera nas configurações do navegador e tente novamente.";
        showState("stateErro");
    }
}

function scanFrame() {
    if (!scanning) return;
    const video = $("video");
    const canvas = $("canvas");
    if (video.readyState >= 2 && video.videoWidth && video.videoHeight) {
        canvas.width = video.videoWidth;
        canvas.height = video.videoHeight;
        const ctx = canvas.getContext("2d", { willReadFrequently: true });
        ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
        const image = ctx.getImageData(0, 0, canvas.width, canvas.height);
        const code = window.jsQR?.(image.data, image.width, image.height, { inversionAttempts: "attemptBoth" });
        if (code?.data) {
            scanning = false;
            handleQr(code.data);
            return;
        }
    }
    animationFrame = requestAnimationFrame(scanFrame);
}

function parseQr(raw) {
    const text = String(raw).trim();
    try {
        const parsed = JSON.parse(text);
        return { machineId: parsed.machine_id || parsed.machineId || parsed.id, eventId: parsed.event_id || parsed.eventId || null };
    } catch {
        return { machineId: text, eventId: null };
    }
}

async function handleQr(raw) {
    showState("stateConectando");
    $("conectandoTexto").textContent = "Validando máquina...";
    const { machineId, eventId } = parseQr(raw);
    if (!machineId) {
        $("erroTexto").textContent = "O QR Code não contém um identificador de máquina válido.";
        showState("stateErro");
        return;
    }

    const { data: machine, error: machineError } = await supabase
        .from("machines")
        .select("id,nome,status")
        .eq("id", machineId)
        .maybeSingle();

    if (machineError || !machine) {
        $("erroTexto").textContent = "Essa máquina não está cadastrada no TAMPAÊ.";
        showState("stateErro");
        return;
    }
    if (machine.status !== "ativa") {
        $("erroTexto").textContent = `A máquina está ${machine.status === "manutencao" ? "em manutenção" : "inativa"}.`;
        showState("stateErro");
        return;
    }

    const user = await requireAuth();
    if (!user) return;
    currentUser = user;
    await closeOwnWaitingSessions();

    const { data: session, error } = await supabase
        .from("machine_sessions")
        .insert({ machine_id: machine.id, user_id: user.id, evento_id: eventId || null })
        .select("id,criado_em,expira_em,machine_id,user_id,evento_id")
        .single();

    if (error) {
        console.error(error);
        $("erroTexto").textContent = error.message || "Não foi possível iniciar a sessão.";
        showState("stateErro");
        return;
    }

    currentSession = session;
    finalizando = false;
    $("maquinaNome").textContent = machine.nome;
    startSessionTimer(new Date(session.expira_em));
    startSessionWatcher();
    showState("sessionView");
    await stopCamera();
}

async function closeOwnWaitingSessions() {
    const { data } = await supabase
        .from("machine_sessions")
        .select("id")
        .eq("user_id", currentUser.id)
        .eq("status", "aguardando");
    for (const row of data || []) {
        await supabase.rpc("encerrar_sessao_usuario", { p_session_id: row.id });
    }
}

function startSessionTimer(expiresAt) {
    clearInterval(sessionTimer);
    const total = Math.max(1, Math.round((expiresAt.getTime() - Date.now()) / 1000));
    const ring = $("anelTempo");
    const circumference = 2 * Math.PI * 44;
    ring.style.strokeDasharray = circumference;
    const tick = () => {
        const remaining = Math.max(0, Math.ceil((expiresAt.getTime() - Date.now()) / 1000));
        const minutes = String(Math.floor(remaining / 60)).padStart(2, "0");
        const seconds = String(remaining % 60).padStart(2, "0");
        $("tempoRestante").textContent = `${minutes}:${seconds}`;
        ring.style.strokeDashoffset = circumference * (1 - remaining / total);
        if (remaining <= 0) {
            clearInterval(sessionTimer);
            finishFromServer("Sessão expirada", false);
        }
    };
    tick();
    sessionTimer = setInterval(tick, 1000);
}

function startSessionWatcher() {
    clearInterval(sessionWatcher);
    sessionWatcher = setInterval(checkSessionState, 1200);
    checkSessionState();
}

function stopSessionWatcher() {
    clearInterval(sessionWatcher);
    sessionWatcher = null;
}

async function checkSessionState() {
    if (!currentSession?.id || finalizando) return;
    const { data, error } = await supabase
        .from("machine_sessions")
        .select("id,status,expira_em,concluida_em")
        .eq("id", currentSession.id)
        .maybeSingle();
    if (error) {
        console.error(error);
        return;
    }
    if (!data || data.status === "concluida") {
        await finishFromServer("Sessão encerrada", false);
        return;
    }
    if (new Date(data.expira_em).getTime() <= Date.now()) {
        await finishFromServer("Sessão expirada", false);
    }
}

async function getSessionPoints(session) {
    if (!session) return 0;
    const { data } = await supabase
        .from("collections")
        .select("pontos")
        .eq("user_id", session.user_id)
        .eq("machine_id", session.machine_id)
        .eq("evento_id", session.evento_id)
        .gte("criado_em", session.criado_em)
        .lte("criado_em", new Date().toISOString());
    return (data || []).reduce((sum, row) => sum + Number(row.pontos || 0), 0);
}

// Dá uma janela para a máquina detectar as últimas tampinhas antes de fechar no banco.
async function waitForFinalCollections() {
    showState("stateFinalizando");
    const fim = Date.now() + JANELA_FINALIZACAO_MS;

    const tick = () => {
        const restante = Math.max(0, fim - Date.now());
        $("tempoFinalizacao").textContent = `${Math.ceil(restante / 1000)}s`;
    };

    tick();
    const interval = setInterval(tick, 100);
    await new Promise((resolve) => setTimeout(resolve, JANELA_FINALIZACAO_MS));
    clearInterval(interval);
}

async function finishFromServer(title, requestClose = true) {
    if (!currentSession?.id) {
        showClosed(title, "+0 pts", "A sessão foi encerrada.");
        return;
    }
    if (finalizando) return;

    const sessionBeforeClose = { ...currentSession };
    clearInterval(sessionTimer);
    stopSessionWatcher();

    // No encerramento manual, aguarda 5 s para a máquina registrar tampinhas que ainda estejam caindo.
    if (requestClose) {
        finalizando = true;
        await waitForFinalCollections();
    }

    let points = 0;
    let closeError = null;

    if (requestClose) {
        const { data, error } = await supabase.rpc("encerrar_sessao_usuario", {
            p_session_id: sessionBeforeClose.id
        });
        closeError = error;
        if (!error && data) {
            const row = Array.isArray(data) ? data[0] : data;
            points = Number(row?.pontos_sessao || 0);
        }
    }

    if (closeError || !requestClose) {
        points = await getSessionPoints(sessionBeforeClose);
    }

    currentSession = null;
    finalizando = false;
    showClosed(
        title,
        `+${points} pts`,
        points > 0 ? "Sessão encerrada. Seus pontos foram contabilizados." : "Sessão encerrada. Nenhuma coleta foi registrada nesta sessão."
    );
}

function showClosed(title, points, text) {
    clearInterval(sessionTimer);
    stopSessionWatcher();
    $("encerradaTitulo").textContent = title;
    $("pontosGanhos").textContent = points;
    $("encerradaTexto").textContent = text;
    showState("stateEncerrada");
}

async function init() {
    const user = await requireAuth();
    if (!user) return;
    currentUser = user;
    $("btnPedirPermissao")?.addEventListener("click", startCamera);
    $("btnTentarNovamente")?.addEventListener("click", startCamera);
    $("btnEscanearDeNovo")?.addEventListener("click", startCamera);
    $("btnEncerrar")?.addEventListener("click", () => finishFromServer("Sessão encerrada", true));
    showState("statePermissao");
}

window.addEventListener("pagehide", async () => {
    await stopCamera();
    stopSessionWatcher();
});

init();
