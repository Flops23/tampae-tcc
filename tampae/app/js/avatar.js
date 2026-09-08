// Importa o cliente Supabase usado para consultar o perfil e o Storage.
import { supabase } from "./supabase.js";

// Nome do bucket do Supabase Storage onde as imagens de perfil são armazenadas.
const AVATAR_BUCKET = "avatars";

// Procura o elemento de perfil existente na barra superior e coloca nele a foto do usuário.
export async function loadHeaderAvatar() {
    // Localiza o espaço reservado para o avatar na barra superior.
    const host = document.querySelector(".top-bar .profile");
    if (!host) return;

    // Obtém o usuário atualmente autenticado.
    const { data: { user } } = await supabase.auth.getUser();
    if (!user) return;

    // Busca no banco somente o nome e o caminho da foto associados ao usuário.
    const { data: profile, error } = await supabase
        .from("profiles")
        .select("nome,foto_path")
        .eq("id", user.id)
        .maybeSingle();

    // Se não houver perfil ou ocorrer erro, não altera o avatar.
    if (error || !profile) return;

    // Só tenta carregar uma imagem quando existe um caminho salvo no perfil.
    if (profile.foto_path) {
        // Obtém a URL pública do arquivo armazenado no bucket de avatares.
        const { data } = supabase.storage
            .from(AVATAR_BUCKET)
            .getPublicUrl(profile.foto_path);

        // Se houver uma URL válida, usa a imagem como fundo do elemento de perfil.
        if (data?.publicUrl) {
            host.innerHTML = "";
            host.style.backgroundImage = `url("${data.publicUrl}?v=${Date.now()}")`;
            host.style.backgroundSize = "cover";
            host.style.backgroundPosition = "center";
            host.style.backgroundRepeat = "no-repeat";
            host.setAttribute("aria-label", "Abrir perfil");
            host.classList.add("has-avatar");
        }
    }
}

// Executa o carregamento do avatar quando este arquivo é carregado.
loadHeaderAvatar();
