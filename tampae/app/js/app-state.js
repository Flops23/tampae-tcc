// Estado compartilhado do aplicativo.
// Os dados principais são carregados na entrada do app e reutilizados entre as páginas.
// A atualização é invalidada quando uma nova coleta é concluída.

const STORAGE_KEY = "tampae_app_state_v1";
let state = null;

function readStoredState() {
    try {
        const raw = sessionStorage.getItem(STORAGE_KEY);
        return raw ? JSON.parse(raw) : null;
    } catch (error) {
        console.warn("Não foi possível ler o estado compartilhado:", error);
        return null;
    }
}

function writeStoredState(value) {
    try {
        sessionStorage.setItem(STORAGE_KEY, JSON.stringify(value));
    } catch (error) {
        console.warn("Não foi possível salvar o estado compartilhado:", error);
    }
}

export function getAppState() {
    if (!state) state = readStoredState();
    return state;
}

export function setAppState(value) {
    state = value;
    writeStoredState(value);
    return state;
}

export function clearAppState() {
    state = null;
    try {
        sessionStorage.removeItem(STORAGE_KEY);
    } catch (error) {
        console.warn("Não foi possível limpar o estado compartilhado:", error);
    }
}

export function isAppStateForUser(userId) {
    const current = getAppState();
    return Boolean(current?.userId && current.userId === userId);
}

export function invalidateAppState() {
    const current = getAppState();
    if (!current) return;
    current.invalidated = true;
    setAppState(current);
}

export function isAppStateInvalidated() {
    return Boolean(getAppState()?.invalidated);
}

export function markAppStateFresh() {
    const current = getAppState();
    if (!current) return;
    current.invalidated = false;
    current.loadedAt = Date.now();
    setAppState(current);
}
