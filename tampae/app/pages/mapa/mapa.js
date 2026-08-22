// Cliente do Supabase e proteção da página para usuários autenticados.
import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

// Estado da página: mapa, máquinas carregadas e marcadores atualmente exibidos.
const state = { map: null, machines: [], markers: [] };

const $ = (id) => document.getElementById(id);

// Exibe uma mensagem temporária na interface.
function showToast(message) {
    const toast = $("toast");
    if (!toast) return;
    toast.textContent = message;
    toast.classList.add("show");
    window.setTimeout(() => toast.classList.remove("show"), 2800);
}

// Converte os códigos de status do banco para textos amigáveis.
function formatStatus(status) {
    return ({ ativa: "Ativa", inativa: "Inativa", manutencao: "Em manutenção" })[status] ?? status;
}

// Calcula a distância aproximada entre duas coordenadas usando a fórmula de Haversine.
function distanceKm(a, b) {
    const R = 6371;
    const dLat = (b.lat - a.lat) * Math.PI / 180;
    const dLon = (b.lon - a.lon) * Math.PI / 180;
    const lat1 = a.lat * Math.PI / 180;
    const lat2 = b.lat * Math.PI / 180;
    const x = Math.sin(dLat / 2) ** 2 + Math.sin(dLon / 2) ** 2 * Math.cos(lat1) * Math.cos(lat2);
    return R * 2 * Math.atan2(Math.sqrt(x), Math.sqrt(1 - x));
}

// Abre o cartão da máquina selecionada e centraliza o mapa nela.
function openMachine(machine) {
    $("routeName").textContent = machine.nome;
    $("routeDetails").textContent = `${formatStatus(machine.status)} • ${Number(machine.latitude).toFixed(5)}, ${Number(machine.longitude).toFixed(5)}`;
    $("routeCard").classList.add("show");
    state.map.setView([machine.latitude, machine.longitude], 17);
}

// Remove marcadores antigos e cria um marcador para cada máquina carregada.
function addMarkers() {
    state.markers.forEach((marker) => marker.remove());
    state.markers = [];

    state.machines.forEach((machine) => {
        const marker = L.marker([machine.latitude, machine.longitude]).addTo(state.map);
        marker.bindPopup(`<strong>${machine.nome}</strong><br>${formatStatus(machine.status)}`);
        marker.on("click", () => openMachine(machine));
        state.markers.push(marker);
    });
}

// Busca as máquinas cadastradas no Supabase e coloca seus marcadores no mapa.
async function loadMachines() {
    const { data, error } = await supabase
        .from("machines")
        .select("id,nome,latitude,longitude,status")
        .order("nome");

    if (error) {
        console.error(error);
        showToast("Não foi possível carregar as máquinas.");
        return;
    }

    state.machines = data ?? [];
    addMarkers();

    if (!state.machines.length) {
        showToast("Ainda não há máquinas cadastradas.");
    }
}

// Cria o mapa Leaflet e define a posição inicial e a camada do OpenStreetMap.
function initMap() {
    state.map = L.map("map", { zoomControl: true }).setView([-15.78, -47.93], 5);
    L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
        attribution: "&copy; OpenStreetMap contributors",
        maxZoom: 19
    }).addTo(state.map);
}

// Obtém a localização do usuário e encontra a máquina ativa mais próxima.
async function locateNearest() {
    if (!state.machines.length) {
        showToast("Nenhuma máquina cadastrada.");
        return;
    }

    if (!navigator.geolocation) {
        showToast("Seu navegador não oferece localização.");
        return;
    }

    navigator.geolocation.getCurrentPosition(
        ({ coords }) => {
            const user = { lat: coords.latitude, lon: coords.longitude };
            const nearest = state.machines
                .filter((m) => m.status === "ativa")
                .map((m) => ({ machine: m, distance: distanceKm(user, { lat: Number(m.latitude), lon: Number(m.longitude) }) }))
                .sort((a, b) => a.distance - b.distance)[0];

            if (!nearest) {
                showToast("Não há máquinas ativas disponíveis.");
                return;
            }

            openMachine(nearest.machine);
            $("routeDetails").textContent = `${nearest.distance.toFixed(2)} km • ${formatStatus(nearest.machine.status)}`;
        },
        () => showToast("Não foi possível obter sua localização."),
        { enableHighAccuracy: true, timeout: 8000 }
    );
}

// Inicializa a página somente depois da autenticação.
async function init() {
    const user = await requireAuth();
    if (!user) return;

    initMap();
    await loadMachines();

    $("routeClose")?.addEventListener("click", () => $("routeCard").classList.remove("show"));
    $("locateBtn")?.addEventListener("click", locateNearest);
}

init();
