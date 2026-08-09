import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

const $ = (id) => document.getElementById(id);
let stream = null;
let scanning = false;
let animationFrame = null;
let sessionTimer = null;
let currentSession = null;
const SESSION_SECONDS = 120;

function showState(state) {
    ["scanOverlay", "statePermissao", "stateErro", "stateConectando", "sessionView", "stateEncerrada"].forEach((id) => {
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

    const { data: session, error } = await supabase
        .from("machine_sessions")
        .insert({ machine_id: machine.id, user_id: user.id, evento_id: eventId || null })
        .select("id,criado_em,expira_em,machine_id")
        .single();

    if (error) {
        console.error(error);
        $("erroTexto").textContent = error.message || "Não foi possível iniciar a sessão.";
        showState("stateErro");
        return;
    }

    currentSession = session;
    $("maquinaNome").textContent = machine.nome;
    startSessionTimer(new Date(session.expira_em));
    showState("sessionView");
    await stopCamera();
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
            showClosed("Sessão expirada", "+0 pts", "O tempo da sessão terminou. A máquina pode concluir a coleta registrada.");
        }
    };
    tick();
    sessionTimer = setInterval(tick, 1000);
}

function showClosed(title, points, text) {
    clearInterval(sessionTimer);
    $("encerradaTitulo").textContent = title;
    $("pontosGanhos").textContent = points;
    $("encerradaTexto").textContent = text;
    showState("stateEncerrada");
}

async function init() {
    const user = await requireAuth();
    if (!user) return;

    $("btnPedirPermissao")?.addEventListener("click", startCamera);
    $("btnTentarNovamente")?.addEventListener("click", startCamera);
    $("btnEscanearDeNovo")?.addEventListener("click", startCamera);
    $("btnEncerrar")?.addEventListener("click", () => {
        showClosed("Sessão encerrada", "+0 pts", "A sessão do aplicativo foi encerrada. A coleta só será pontuada quando a máquina registrar o depósito.");
    });

    showState("statePermissao");
}

window.addEventListener("pagehide", stopCamera);
init();
