import { supabase } from "../../js/supabase.js";
import { requireAuth, logout } from "../../js/auth.js";

const SETTINGS_KEY = "tampae_settings";
const $ = (id) => document.getElementById(id);

function loadSettings() {
    const saved = JSON.parse(localStorage.getItem(SETTINGS_KEY) || "{}");
    $("toggleSom").checked = saved.som !== false;
    $("toggleVibracao").checked = saved.vibracao !== false;
    $("toggleAvisos").checked = saved.avisos !== false;
}

function saveSettings() {
    localStorage.setItem(SETTINGS_KEY, JSON.stringify({
        som: $("toggleSom").checked,
        vibracao: $("toggleVibracao").checked,
        avisos: $("toggleAvisos").checked
    }));
}

async function changePassword() {
    const nova = $("senhaNova").value;
    const confirma = $("senhaConfirma").value;

    if (nova.length < 6) return alert("A nova senha precisa ter pelo menos 6 caracteres.");
    if (nova !== confirma) return alert("A confirmação da nova senha não confere.");

    const { error } = await supabase.auth.updateUser({ password: nova });
    if (error) {
        alert(`Não foi possível alterar a senha: ${error.message}`);
        return;
    }

    $("senhaAtual").value = "";
    $("senhaNova").value = "";
    $("senhaConfirma").value = "";
    $("painelSenha").hidden = true;
    alert("Senha alterada com sucesso.");
}

async function init() {
    const user = await requireAuth();
    if (!user) return;

    loadSettings();
    ["toggleSom", "toggleVibracao", "toggleAvisos"].forEach((id) => $(id)?.addEventListener("change", saveSettings));

    $("itemSair")?.addEventListener("click", async () => {
        if (!confirm("Deseja realmente sair da conta?")) return;
        try {
            await logout();
        } catch {
            alert("Não foi possível sair da conta.");
        }
    });

    $("itemAlterarSenha")?.addEventListener("click", () => {
        $("painelSenha").hidden = !$("painelSenha").hidden;
    });

    $("btnCancelarSenha")?.addEventListener("click", () => {
        $("painelSenha").hidden = true;
    });

    $("btnSalvarSenha")?.addEventListener("click", changePassword);
}

init();
