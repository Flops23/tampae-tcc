import { supabase } from "../../js/supabase.js";
import { requireAuth } from "../../js/auth.js";

const state = { map: null, machines: [], markers: [] };

const $ = (id) => document.getElementById(id);

function showToast(message) {
    const toast = $("toast");
    if (!toast) return;
    toast.textContent = message;
    toast.classList.add("show");
    window.setTimeout(() => toast.classList.remove("show"), 2800);
}

function formatStatus(status) {
    return ({ ativa: "Ativa", inativa: "Inativa", manutencao: "Em manutenção" })[status] ?? status;
}

function distanceKm(a, b) {
    const R = 6371;
    const dLat = (b.lat - a.lat) * Math.PI / 180;
    const dLon = (b.lon - a.lon) * Math.PI / 180;
    const lat1 = a.lat * Math.PI / 180;
    const lat2 = b.lat * Math.PI / 180;
    const x = Math.sin(dLat / 2) ** 2 + Math.sin(dLon / 2) ** 2 * Math.cos(lat1) * Math.cos(lat2);
    return R * 2 * Math.atan2(Math.sqrt(x), Math.sqrt(1 - x));
}

function openMachine(machine) {
    $("routeName").textContent = machine.nome;
    $("routeDetails").textContent = `${formatStatus(machine.status)} • ${Number(machine.latitude).toFixed(5)}, ${Number(machine.longitude).toFixed(5)}`;
    $("routeCard").classList.add("show");
    state.map.setView([machine.latitude, machine.longitude], 17);
}

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

function initMap() {
    state.map = L.map("map", { zoomControl: true }).setView([-15.78, -47.93], 5);
    L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
        attribution: "&copy; OpenStreetMap contributors",
        maxZoom: 19
    }).addTo(state.map);
}

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

async function init() {
    const user = await requireAuth();
    if (!user) return;

    initMap();
    await loadMachines();

    $("routeClose")?.addEventListener("click", () => $("routeCard").classList.remove("show"));
    $("locateBtn")?.addEventListener("click", locateNearest);
}

init();
