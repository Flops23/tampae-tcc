import { supabase } from "./supabase.js";

const AVATAR_BUCKET = "avatars";

export async function loadHeaderAvatar() {
    const host = document.querySelector(".top-bar .profile");
    if (!host) return;

    const { data: { user } } = await supabase.auth.getUser();
    if (!user) return;

    const { data: profile, error } = await supabase
        .from("profiles")
        .select("nome,foto_path")
        .eq("id", user.id)
        .maybeSingle();

    if (error || !profile) return;

    if (profile.foto_path) {
        const { data } = supabase.storage
            .from(AVATAR_BUCKET)
            .getPublicUrl(profile.foto_path);

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

loadHeaderAvatar();
