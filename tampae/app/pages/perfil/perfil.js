import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

const $ = (id) => document.getElementById(id);
const AVATAR_BUCKET = "avatars";
const MAX_INPUT_SIZE = 10 * 1024 * 1024;
const MAX_OUTPUT_SIZE = 350 * 1024;
const AVATAR_SIZE = 512;

async function init() {
    const user = await requireAuth();
    if (!user) return;

    $("perfilEmail").textContent = user.email ?? "—";
    $("perfilDesde").textContent = `membro desde ${new Intl.DateTimeFormat("pt-BR", { month: "long", year: "numeric" }).format(new Date(user.created_at))}`;

    const { data: profile, error } = await supabase
        .from("profiles")
        .select("id,nome,foto_path,pontos_totais,tampinhas_totais,peso_total_gramas")
        .eq("id", user.id)
        .maybeSingle();

    if (error || !profile) {
        console.error(error);
        return;
    }

    renderProfile(profile);
    await Promise.all([loadAchievements(user.id), loadHistory(user.id)]);

    $("btnEditarFoto")?.addEventListener("click", () => $("inputFotoPerfil")?.click());
    $("inputFotoPerfil")?.addEventListener("change", async (event) => {
        const file = event.target.files?.[0];
        event.target.value = "";
        if (!file) return;
        await updateProfilePhoto(user, file);
    });

    $("btnEditarPerfil")?.addEventListener("click", async () => {
        const nome = window.prompt("Como você quer ser chamado?", profile.nome ?? "");
        if (nome === null) return;
        const novoNome = nome.trim();
        if (!novoNome) return alert("O nome não pode ficar vazio.");
        const { error: updateError } = await supabase.from("profiles").update({ nome: novoNome }).eq("id", user.id);
        if (updateError) return alert("Não foi possível atualizar seu nome.");
        window.location.reload();
    });
}

async function updateProfilePhoto(user, file) {
    if (!file.type.startsWith("image/")) return alert("Selecione uma imagem válida.");
    if (file.size > MAX_INPUT_SIZE) return alert("A imagem original deve ter no máximo 10 MB.");

    try {
        const blob = await compressImage(file);

        // O bucket já é definido em .from(AVATAR_BUCKET), portanto o caminho
        // NÃO deve repetir "avatars/".
        // Resultado final no Storage: avatars/<user.id>/profile.webp
        const path = `${user.id}/profile.webp`;

        const { error: uploadError } = await supabase.storage
            .from(AVATAR_BUCKET)
            .upload(path, blob, {
                contentType: "image/webp",
                cacheControl: "3600",
                upsert: true
            });

        if (uploadError) throw uploadError;

        const { error: updateError } = await supabase
            .from("profiles")
            .update({ foto_path: path })
            .eq("id", user.id);

        if (updateError) throw updateError;

        window.location.reload();
    } catch (error) {
        console.error("Falha ao salvar foto de perfil:", error);
        alert("Não foi possível salvar a foto de perfil.");
    }
}

async function compressImage(file) {
    const image = await loadImage(file);
    let size = Math.min(AVATAR_SIZE, image.naturalWidth, image.naturalHeight);
    let quality = 0.82;
    let blob = null;

    for (let attempt = 0; attempt < 6; attempt++) {
        const canvas = document.createElement("canvas");
        canvas.width = size;
        canvas.height = size;
        const context = canvas.getContext("2d", { alpha: false });
        if (!context) throw new Error("Canvas não suportado.");

        context.fillStyle = "#ffffff";
        context.fillRect(0, 0, size, size);

        const scale = Math.max(size / image.naturalWidth, size / image.naturalHeight);
        const width = image.naturalWidth * scale;
        const height = image.naturalHeight * scale;
        const x = (size - width) / 2;
        const y = (size - height) / 2;
        context.drawImage(image, x, y, width, height);

        blob = await new Promise((resolve) => canvas.toBlob(resolve, "image/webp", quality));
        if (!blob) throw new Error("Não foi possível gerar a imagem.");
        if (blob.size <= MAX_OUTPUT_SIZE) return blob;

        if (quality > 0.58) {
            quality -= 0.08;
        } else {
            size = Math.max(256, Math.round(size * 0.8));
            quality = 0.72;
        }
    }

    if (!blob || blob.size > MAX_OUTPUT_SIZE) throw new Error("Não foi possível reduzir a imagem ao tamanho permitido.");
    return blob;
}

function loadImage(file) {
    return new Promise((resolve, reject) => {
        const url = URL.createObjectURL(file);
        const image = new Image();
        image.onload = () => {
            URL.revokeObjectURL(url);
            resolve(image);
        };
        image.onerror = () => {
            URL.revokeObjectURL(url);
            reject(new Error("Imagem inválida."));
        };
        image.src = url;
    });
}

function renderProfile(profile) {
    const nome = profile.nome || "Usuário";
    $("perfilNome").textContent = nome;
    $("avatarIniciais").textContent = initials(nome);
    $("avatarIniciais").style.backgroundImage = "";
    $("statPontos").textContent = Number(profile.pontos_totais || 0).toLocaleString("pt-BR");
    $("statTampinhas").textContent = Number(profile.tampinhas_totais || 0).toLocaleString("pt-BR");
    $("statPeso").textContent = formatWeight(profile.peso_total_gramas);

    if (profile.foto_path) {
        const { data } = supabase.storage.from(AVATAR_BUCKET).getPublicUrl(profile.foto_path);
        if (data?.publicUrl) {
            $("avatarIniciais").style.backgroundImage = `url("${data.publicUrl}?v=${Date.now()}")`;
            $("avatarIniciais").style.backgroundSize = "cover";
            $("avatarIniciais").style.backgroundPosition = "center";
            $("avatarIniciais").textContent = "";
        }
    }
}

async function loadAchievements(userId) {
    const { data, error } = await supabase.from("vw_conquistas_usuario").select("achievement_id,nome,descricao,tipo_meta,meta_valor,progresso_atual,conquistada").eq("user_id", userId).order("meta_valor");
    if (error) {
        console.error(error);
        return;
    }

    const list = $("conquistasLista");
    const rows = data ?? [];
    const done = rows.filter((row) => row.conquistada).length;
    $("conquistasContagem").textContent = `${done}/${rows.length}`;

    list.innerHTML = rows.length ? rows.map((row) => {
        const progress = Math.min(100, Number(row.meta_valor) ? Number(row.progresso_atual || 0) / Number(row.meta_valor) * 100 : 0);
        return `<div class="conquista ${row.conquistada ? "concluida" : ""}"><div class="conquista-icone"><span class="material-symbols-rounded">${row.conquistada ? "verified" : "emoji_events"}</span></div><b>${escapeHtml(row.nome)}</b><small>${escapeHtml(row.descricao || "")}</small><div class="barra"><span style="width:${progress}%"></span></div></div>`;
    }).join("") : `<div class="empty-state">Nenhuma conquista cadastrada ainda.</div>`;
}

async function loadHistory(userId) {
    const { data, error } = await supabase.from("collections").select("id,tipo_coleta,quantidade_real,quantidade_estimada,peso_real_gramas,peso_estimado_gramas,pontos,criado_em,machines(nome)").eq("user_id", userId).order("criado_em", { ascending: false }).limit(5);
    if (error) {
        console.error(error);
        return;
    }

    const list = $("historicoLista");
    const rows = data ?? [];
    list.innerHTML = rows.length ? rows.map((row) => {
        const quantity = row.quantidade_real ?? row.quantidade_estimada;
        const weight = row.peso_real_gramas ?? row.peso_estimado_gramas;
        const detail = quantity != null ? `${quantity} tampinhas` : `${formatWeight(weight)}`;
        return `<div class="historico-item"><div class="icone"><span class="material-symbols-rounded">recycling</span></div><div class="texto"><b>${escapeHtml(row.machines?.nome || "Coleta")}</b><small>${detail} • ${new Date(row.criado_em).toLocaleDateString("pt-BR")}</small></div><strong>+${Number(row.pontos || 0)} pts</strong></div>`;
    }).join("") : `<div class="empty-state">Você ainda não realizou coletas.</div>`;
}

function formatWeight(grams) {
    const value = Number(grams || 0);
    if (value >= 1000) return `${(value / 1000).toLocaleString("pt-BR", { maximumFractionDigits: 2 })} kg`;
    return `${value.toLocaleString("pt-BR", { maximumFractionDigits: 0 })} g`;
}

function initials(name) {
    return String(name).trim().split(/\s+/).slice(0, 2).map((part) => part[0]).join("").toUpperCase();
}

function escapeHtml(value) {
    return String(value ?? "").replace(/[&<>\"]/g, (char) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[char]));
}

init();
