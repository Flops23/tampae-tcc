import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

const $ = (id) => document.getElementById(id);

function formatNumber(value) {
    return new Intl.NumberFormat("pt-BR").format(Number(value) || 0);
}

function formatWeight(grams) {
    const value = Number(grams) || 0;

    if (value >= 1000) {
        return `${(value / 1000).toLocaleString("pt-BR", {
            minimumFractionDigits: 0,
            maximumFractionDigits: 2
        })} kg`;
    }

    return `${value.toLocaleString("pt-BR", {
        minimumFractionDigits: 0,
        maximumFractionDigits: 0
    })} g`;
}

async function loadProfile(user) {
    const { data: profile, error } = await supabase
        .from("profiles")
        .select("nome, pontos_totais, peso_total_gramas, tampinhas_totais, foto_url")
        .eq("id", user.id)
        .maybeSingle();

    if (error) {
        console.error("Erro ao carregar perfil:", error);
        throw error;
    }

    if (!profile) {
        console.warn("Perfil não encontrado para o usuário autenticado.");
        $("saudacaoNome").textContent = "Usuário";
        return;
    }

    $("saudacaoNome").textContent = profile.nome || "Usuário";
    $("pontosTotais").textContent = formatNumber(profile.pontos_totais);
    $("statTampinhas").textContent = formatNumber(profile.tampinhas_totais);
    $("statPeso").textContent = formatWeight(profile.peso_total_gramas);

    if (profile.foto_url) {
        const profileLink = document.querySelector(".profile");
        if (profileLink) {
            profileLink.innerHTML = `<img src="${profile.foto_url}" alt="Foto de perfil">`;
        }
    }
}

function setupEmptyChart() {
    const canvas = $("grafico");
    const empty = $("graficoVazio");

    if (!canvas || !empty) return;

    // O histórico de collections ainda não está conectado à Home.
    // Não inventamos dados: mostramos o estado vazio até essa integração existir.
    canvas.hidden = true;
    empty.hidden = false;
}

async function initHome() {
    const user = await requireAuth();
    if (!user) return;

    try {
        await loadProfile(user);
        setupEmptyChart();
    } catch (error) {
        console.error("Falha ao carregar a Home:", error);
        $("saudacaoNome").textContent = "Não foi possível carregar seus dados";
        $("pontosTotais").textContent = "—";
        $("statTampinhas").textContent = "—";
        $("statPeso").textContent = "—";
    }
}

initHome();
