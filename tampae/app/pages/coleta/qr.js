// Cliente do Supabase e proteção da página para usuários autenticados.
import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

// Atalho para buscar elementos pelo id.
const $ = (id) => document.getElementById(id);

// Estado local do leitor e da sessão atual.
let stream = null;
let scanning = false;
let animationFrame = null;
let sessionTimer = null;
let sessionWatcher = null;
let currentSession = null;
let currentUser = null;

// Alterna entre os diferentes estados visuais da tela de coleta.
function showState(state) {
    ["scanOverlay", "statePermissao", "stateErro", "stateConectando", "sessionView", "stateEncerrada"].forEach((id) => {
        const element = $(id);
        if (element) element.style.display = "none";
    });
    const target = $(state);
    if (target) target.style.display = state === "scanOverlay" ? "block" : "flex";
}

// Para a câmera e cancela o loop de leitura do QR Code.
async function stopCamera() {
    scanning = false;
    if (animationFrame) cancelAnimationFrame(animationFrame);
    animationFrame = null;
    if (stream) stream.getTracks().forEach((track) => track.stop());
    stream = null;
    if ($("video")) $("video").srcObject = null;
}

// Solicita acesso à câmera traseira e inicia a leitura dos quadros.
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

// Captura um quadro da câmera, envia os pixels ao jsQR e continua até encontrar um código.
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

// Aceita o payload JSON do QR e também mantém compatibilidade com um QR contendo apenas o ID.
function parseQr(raw) {
    const text = String(raw).trim();
    try {
        const parsed = JSON.parse(text);
        return { machineId: parsed.machine_id || parsed.machineId || parsed.id, eventId: parsed.event_id || parsed.eventId || null };
    } catch {
        return { machineId: text, eventId: null };
    }
}

// Valida a máquina e cria uma nova sessão vinculando usuário, máquina e evento.
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

    // Cria a sessão no banco. O eventId vindo do QR é gravado em evento_id.
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
    $("maquinaNome").textContent = machine.nome;
    startSessionTimer(new Date(session.expira_em));
    startSessionWatcher();
    showState("sessionView");
    await stopCamera();
}

// Fecha sessões antigas do mesmo usuário que ainda estejam aguardando.
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

// Mantém o contador visual sincronizado com o horário de expiração da sessão.
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
            finishFromServer("Sessão expirada");
        }
    };
    tick();
    sessionTimer = setInterval(tick, 1000);
}

// Consulta periodicamente o banco para descobrir se a sessão foi encerrada pela máquina ou pelo usuário.
function startSessionWatcher() {
    clearInterval(sessionWatcher);
    sessionWatcher = setInterval(checkSessionState, 1200);
    checkSessionState();
}

function stopSessionWatcher() {
    clearInterval(sessionWatcher);
    sessionWatcher = null;
}

// Verifica o estado atual da sessão no banco.
async function checkSessionState() {
    if (!currentSession?.id) return;
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
        await finishFromServer("Sessão expirada", true);
    }
}

// Soma os pontos das coletas associadas à sessão.
async function getSessionPoints() {
    if (!currentSession) return 0;
    const { data } = await supabase
        .from("collections")
        .select("pontos")
        .eq("user_id", currentSession.user_id)
        .eq("machine_id", currentSession.machine_id)
        .eq("evento_id", currentSession.evento_id)
        .gte("criado_em", currentSession.criado_em);
    return (data || []).reduce((sum, row) => sum + Number(row.pontos || 0), 0);
}

// Finaliza a sessão, tenta obter os pontos registrados e apresenta o resultado ao usuário.
async function finishFromServer(title, requestClose = true) {
    if (!currentSession?.id) {
        showClosed(title, "+0 pts", "A sessão foi encerrada.");
        return;
    }
    const sessionBeforeClose = { ...currentSession };
    clearInterval(sessionTimer);
    stopSessionWatcher();

    let points = 0;
    let closeError = null;

    if (requestClose) {
        // A RPC encerra a sessão e devolve o total de pontos calculado pelo backend.
        const { data, error } = await supabase.rpc("encerrar_sessao_usuario", {
            p_session_id: sessionBeforeClose.id
        });
        closeError = error;
        if (!error && data) {
            const row = Array.isArray(data) ? data[0] : data;
            points = Number(row?.pontos_sessao || 0);
        }
    }

    // Se a RPC falhar ou não for solicitada, usa as collections como fallback para calcular os pontos.
    if (closeError || !requestClose) {
        const { data: fallback } = await supabase
            .from("collections")
            .select("pontos")
            .eq("user_id", sessionBeforeClose.user_id)
            .eq("machine_id", sessionBeforeClose.machine_id)
            .eq("evento_id", sessionBeforeClose.evento_id)
            .gte("criado_em", sessionBeforeClose.criado_em);
        points = (fallback || []).reduce((sum, item) => sum + Number(item.pontos || 0), 0);
    }

    currentSession = null;
    showClosed(
        title,
        `+${points} pts`,
        points > 0 ? "Sua sessão foi encerrada e seus pontos foram contabilizados." : "Sua sessão foi encerrada. Nenhuma coleta foi registrada nesta sessão."
    );
}

// Atualiza a tela final com o motivo do encerramento e os pontos ganhos.
function showClosed(title, points, text) {
    clearInterval(sessionTimer);
    stopSessionWatcher();
    $("encerradaTitulo").textContent = title;
    $("pontosGanhos").textContent = points;
    $("encerradaTexto").textContent = text;
    showState("stateEncerrada");
}

// Inicializa a página, exige autenticação e conecta os botões às ações.
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

// Ao sair da página, libera câmera e timers para evitar recursos ativos em segundo plano.
window.addEventListener("pagehide", async () => {
    await stopCamera();
    stopSessionWatcher();
});

init();
