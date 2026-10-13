/**
 * SÍTIO ONLINE - SISTEMA DE MONITORAMENTO AGRÍCOLA ESP32
 * Lógica Completa da Aplicação Web, Navegação Matricial 4x4, Fila Offline e Gestão de Energia/Bateria
 */

// ============================================================================
// 1. ESTADO GLOBAL DA APLICAÇÃO
// ============================================================================

// Base de Dados de Demonstração (Utilizada exclusivamente no Modo Simulador)
const MOCK_ESPS = [
        {
            id: 'esp-1',
            num: 1,
            name: 'ESP 1',
            connectivity: 'ONLINE',
            mode: 'DEEP SLEEP',
            lastUpdate: '14:20 - 11/10/2026',
            nextUpdate: '15:20 - 11/10/2026',
            sensorValue: '62% (Umidade Solo)',
            interval: '60 min',
            unit: '% Umidade',
            batteryPct: 94,
            uptimeHours: 320,
            powerSource: 'BATTERY', // 'BATTERY', 'DC', 'SOLAR'
            batteryCapacityMah: 2500
        },
        {
            id: 'esp-2',
            num: 2,
            name: 'ESP 2',
            connectivity: 'DORMINDO',
            mode: 'LIGHT SLEEP',
            lastUpdate: '14:15 - 11/10/2026',
            nextUpdate: '14:45 - 11/10/2026',
            sensorValue: '24.8 °C (Temperatura)',
            interval: '30 min',
            unit: '°C',
            batteryPct: 88,
            uptimeHours: 195,
            powerSource: 'BATTERY',
            batteryCapacityMah: 2500
        },
        {
            id: 'esp-3',
            num: 3,
            name: 'ESP 3',
            connectivity: 'ONLINE',
            mode: 'ONLINE',
            lastUpdate: '14:32 - 11/10/2026',
            nextUpdate: '14:33 - 11/10/2026',
            sensorValue: 'Seco (Sensor MH-RD)',
            interval: '1 min',
            unit: 'Estado Chuva',
            batteryPct: 100,
            uptimeHours: 65,
            powerSource: 'DC', // Fonte DC contínua na tomada
            batteryCapacityMah: null
        },
        {
            id: 'esp-4',
            num: 4,
            name: 'ESP 4',
            connectivity: 'DORMINDO',
            mode: 'DEEP SLEEP',
            lastUpdate: '13:50 - 11/10/2026',
            nextUpdate: '15:50 - 11/10/2026',
            sensorValue: '480 ADC (Solo)',
            interval: '120 min',
            unit: 'ADC',
            batteryPct: 76,
            uptimeHours: 540,
            powerSource: 'BATTERY',
            batteryCapacityMah: 5000 // Bateria Dupla Li-ion
        },
        {
            id: 'esp-5',
            num: 5,
            name: 'ESP 5',
            connectivity: 'DORMINDO',
            mode: 'LIGHT SLEEP',
            lastUpdate: '14:05 - 11/10/2026',
            nextUpdate: '14:20 - 11/10/2026',
            sensorValue: '12 km/h (Anemômetro)',
            interval: '15 min',
            unit: 'km/h',
            batteryPct: 92,
            uptimeHours: 210,
            powerSource: 'SOLAR', // Painel Solar + Bateria Híbrido
            batteryCapacityMah: 2500
        },
        {
            id: 'esp-6',
            num: 6,
            name: 'ESP 6',
            connectivity: 'ONLINE',
            mode: 'ONLINE',
            lastUpdate: '14:31 - 11/10/2026',
            nextUpdate: '14:35 - 11/10/2026',
            sensorValue: '75% (Umidade Solo)',
            interval: '5 min',
            unit: '% Umidade',
            batteryPct: 100,
            uptimeHours: 90,
            powerSource: 'DC', // Tomada / Rede Elétrica
            batteryCapacityMah: null
        },
        {
            id: 'esp-7',
            num: 7,
            name: 'ESP 7',
            connectivity: 'DORMINDO',
            mode: 'LIGHT SLEEP',
            lastUpdate: '14:10 - 11/10/2026',
            nextUpdate: '14:40 - 11/10/2026',
            sensorValue: 'Chovendo (MH-RD)',
            interval: '30 min',
            unit: 'Estado Chuva',
            batteryPct: 91,
            uptimeHours: 310,
            powerSource: 'BATTERY',
            batteryCapacityMah: 1200 // Pilhas AA / Compacta
        },
        {
            id: 'esp-8',
            num: 8,
            name: 'ESP 8',
            connectivity: 'DORMINDO',
            mode: 'DEEP SLEEP',
            lastUpdate: '13:00 - 11/10/2026',
            nextUpdate: '14:30 - 11/10/2026',
            sensorValue: '1013 hPa (Barômetro)',
            interval: '90 min',
            unit: 'hPa',
            batteryPct: 85,
            uptimeHours: 420,
            powerSource: 'BATTERY',
            batteryCapacityMah: 5000 // Bateria Dupla
        },
        {
            id: 'esp-9',
            num: 9,
            name: 'ESP 9',
            connectivity: 'ONLINE',
            mode: 'DEEP SLEEP',
            lastUpdate: '14:28 - 11/10/2026',
            nextUpdate: '15:28 - 11/10/2026',
            sensorValue: '58% (Umidade Solo)',
            interval: '60 min',
            unit: '% Umidade',
            batteryPct: 19,
            uptimeHours: 850,
            powerSource: 'BATTERY',
            batteryCapacityMah: 2500
        },
        {
            id: 'esp-10',
            num: 10,
            name: 'ESP 10',
            connectivity: 'DORMINDO',
            mode: 'LIGHT SLEEP',
            lastUpdate: '14:22 - 11/10/2026',
            nextUpdate: '14:42 - 11/10/2026',
            sensorValue: '420 W/m² (Radiação UV)',
            interval: '20 min',
            unit: 'W/m²',
            batteryPct: 84,
            uptimeHours: 160,
            powerSource: 'SOLAR', // Painel Solar + Bateria 3000 mAh
            batteryCapacityMah: 3000
        }
];

const MOCK_NOTIFICATIONS = [
    'Modo Simulação Ativo: exibindo exemplos de sensores agrícolas.',
    'O ESP 3 está online!',
    'O ESP 9 está online!'
];

const AppState = {
    currentScreen: 'main-menu',
    screenHistory: [],
    focusIndex: 0,
    selectedEspId: null,

    // Paginação Vertical de Linhas (Nós ESP32) nas telas de Status
    statusRowPage: 0,
    statusRowsPerPage: 5,
    
    // Configurações da API REST do ESP32
    api: {
        baseUrl: 'http://192.168.4.1',
        pollingInterval: 4000,
        simulatorMode: true,
        isOnline: true,
        pollingTimer: null
    },

    // Janela Agendada de Recepção de Dados dos Periféricos
    receptionSchedule: {
        enabled: true,
        cycleSeconds: 60,
        windowDurationSeconds: 10,
        secondsUntilWindow: 60,
        inReceptionWindow: false,
        windowSecondsRemaining: 0,
        timer: null
    },

    // Fundo Dinâmico
    bgSlideTimer: null,
    activeBgIndex: 1,

    // Teclado Virtual (Slide 8)
    vk: {
        fieldKey: 'name',
        fieldTitle: '(NOME SENDO DIGITADO)',
        text: 'ESP 1',
        activeRow: 1,
        activeCol: 0,
        rows: [
            ['1', '2', '3', '4', '5', '6', '7', '8', '9', '0'],
            ['Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P'],
            ['A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'Ç'],
            ['Z', 'X', 'C', 'V', 'B', 'N', 'M', '.', '%', '-'],
            ['ESPAÇO', 'APAGAR']
        ]
    },

    // Fila de Alterações Offline (Deep/Light Sleep)
    pendingQueue: [],

    // Base de Dados dos Nós ESP32 Conectados
    esps: JSON.parse(JSON.stringify(MOCK_ESPS)),

    // Notificações
    notifications: [...MOCK_NOTIFICATIONS]
};

// ============================================================================
// 2. INICIALIZAÇÃO DA APLICAÇÃO (COM PROTEÇÃO CONTRA ERROS)
// ============================================================================

document.addEventListener('DOMContentLoaded', () => {
    try {
        initDynamicBackground();
    } catch (e) {
        console.error('[Init] Erro ao iniciar fundo dinâmico:', e);
    }

    try {
        initVirtualKeyboardGrid();
    } catch (e) {
        console.error('[Init] Erro ao iniciar teclado virtual:', e);
    }

    try {
        setupEventListeners();
    } catch (e) {
        console.error('[Init] Erro ao configurar eventos:', e);
    }

    try {
        updateUI();
    } catch (e) {
        console.error('[Init] Erro ao atualizar interface:', e);
    }

    try {
        startPollingLoop();
        initReceptionSchedule();
        simulateBackgroundEvents();
    } catch (e) {
        console.error('[Init] Erro na simulação de eventos ou agendamento:', e);
    }
});

// ============================================================================
// 3. FUNDO DINÂMICO (TRANSIÇÃO SUAVE A CADA 4 SEGUNDOS)
// ============================================================================

function initDynamicBackground() {
    const slide1 = document.getElementById('bg-slide-1');
    const slide2 = document.getElementById('bg-slide-2');

    if (!slide1 || !slide2) return;

    if (AppState.bgSlideTimer) clearInterval(AppState.bgSlideTimer);

    AppState.bgSlideTimer = setInterval(() => {
        if (AppState.activeBgIndex === 1) {
            slide1.classList.remove('bg-active');
            slide2.classList.add('bg-active');
            AppState.activeBgIndex = 2;
        } else {
            slide2.classList.remove('bg-active');
            slide1.classList.add('bg-active');
            AppState.activeBgIndex = 1;
        }
    }, 4000);
}

// ============================================================================
// 4. MÁQUINA DE ESTADOS E NAVEGAÇÃO DE TELAS
// ============================================================================

function navigateTo(screenId) {
    if (AppState.currentScreen === screenId) return;

    AppState.screenHistory.push(AppState.currentScreen);
    AppState.currentScreen = screenId;
    AppState.focusIndex = 0;

    applySubpageBackdropOpacity();
    updateUI();
}

function navigateBack() {
    if (AppState.screenHistory.length > 0) {
        AppState.currentScreen = AppState.screenHistory.pop();
        AppState.focusIndex = 0;
        applySubpageBackdropOpacity();
        updateUI();
    }
}

function applySubpageBackdropOpacity() {
    const appEl = document.getElementById('app');
    if (!appEl) return;
    const isSubpage = AppState.currentScreen !== 'main-menu';
    
    // Regra estrita: 14% de opacidade aplicada nas abas de Status e Configurações
    if (isSubpage) {
        appEl.classList.add('subpage-active');
    } else {
        appEl.classList.remove('subpage-active');
    }
}

// ============================================================================
// 5. CÁLCULO E ESTIMATIVA DE BATERIA DO ESP32 (COM SUPORTE A FONTES DE ENERGIA)
// ============================================================================

/**
 * Calcula a autonomia estimada e consumo médio do ESP32 com base:
 * - Fonte de Alimentação: Fonte DC (tomada), Painel Solar Híbrido ou Bateria
 * - Capacidade configurada da bateria em mAh (ex: 1200, 2500, 5000 mAh)
 * - Porcentagem de carga atual (batteryPct)
 * - Modo operacional (ONLINE, LIGHT SLEEP, DEEP SLEEP)
 * - Ciclo de atividade e intervalo de medição/envio REST
 * - Tempo total acumulado em operação (uptimeHours)
 */
function calculateBatteryEstimate(esp) {
    const powerSource = (esp.powerSource || 'BATTERY').toUpperCase();
    const capacityTotal = esp.batteryCapacityMah || 2500; // mAh
    const pct = typeof esp.batteryPct === 'number' ? esp.batteryPct : 80;
    const remainingMah = (pct / 100) * capacityTotal;

    // Extrai o intervalo em minutos
    let intervalMin = 60;
    if (esp.interval) {
        const match = esp.interval.match(/(\d+)/);
        if (match) intervalMin = Math.max(1, parseInt(match[1], 10));
    }
    const cycleSec = intervalMin * 60;
    const activeSec = 3.0; // segundos acordado (leitura ADC + WiFi TX/RX REST)
    const activeMa = 180.0; // consumo em transmissão ativa com WiFi do ESP32

    let avgMa = 180.0;
    const mode = (esp.mode || 'ONLINE').toUpperCase();

    if (mode === 'DEEP SLEEP') {
        const sleepMa = 0.015; // 15 uA no Deep Sleep (RTC timer + ULP)
        avgMa = ((activeSec * activeMa) + Math.max(0, cycleSec - activeSec) * sleepMa) / cycleSec;
    } else if (mode === 'LIGHT SLEEP') {
        const sleepMa = 1.5; // 1.5 mA no Light Sleep (RAM e periféricos retidos)
        avgMa = ((activeSec * activeMa) + Math.max(0, cycleSec - activeSec) * sleepMa) / cycleSec;
    } else {
        avgMa = 180.0; // ONLINE contínuo (rádio e CPU ligados)
    }

    const uptimeH = esp.uptimeHours || 120;
    let uptimeFormatted = '';
    if (uptimeH < 24) {
        uptimeFormatted = `${uptimeH}h`;
    } else {
        const d = Math.floor(uptimeH / 24);
        const h = uptimeH % 24;
        uptimeFormatted = `${d}d ${h}h`;
    }

    // Caso 1: Fonte DC / Tomada (Rede Elétrica contínua)
    if (powerSource === 'DC') {
        return {
            pct: 100,
            remainingHours: Infinity,
            remainingFormatted: '∞ Contínuo (Fonte DC)',
            uptimeHours: uptimeH,
            uptimeFormatted,
            avgMa: avgMa < 1 ? avgMa.toFixed(3) : avgMa.toFixed(1),
            badgeClass: 'dc',
            powerSourceLabel: 'Fonte DC 🔌'
        };
    }

    // Caso 2: Painel Solar + Bateria (Híbrido Autossuficiente)
    if (powerSource === 'SOLAR') {
        return {
            pct,
            remainingHours: Infinity,
            remainingFormatted: 'Autossuficiente (Solar)',
            uptimeHours: uptimeH,
            uptimeFormatted,
            avgMa: avgMa < 1 ? avgMa.toFixed(3) : avgMa.toFixed(1),
            badgeClass: 'solar',
            powerSourceLabel: `Solar + ${capacityTotal}mAh ☀️`
        };
    }

    // Caso 3: Bateria (Li-ion 18650, LiPo, Pilhas, etc.)
    const remainingHours = avgMa > 0 ? (remainingMah / avgMa) : 0;

    let remainingFormatted = '';
    if (remainingHours < 1) {
        remainingFormatted = `~${Math.max(1, Math.round(remainingHours * 60))} min`;
    } else if (remainingHours < 48) {
        remainingFormatted = `~${Math.round(remainingHours)}h`;
    } else if (remainingHours < 720) {
        const days = (remainingHours / 24).toFixed(1);
        remainingFormatted = `~${days} dias`;
    } else {
        const months = (remainingHours / 720).toFixed(1);
        remainingFormatted = `~${months} meses`;
    }

    let badgeClass = 'high';
    if (pct < 25) badgeClass = 'low';
    else if (pct < 60) badgeClass = 'medium';

    return {
        pct,
        remainingHours,
        remainingFormatted,
        uptimeHours: uptimeH,
        uptimeFormatted,
        avgMa: avgMa < 1 ? avgMa.toFixed(3) : avgMa.toFixed(1),
        badgeClass,
        powerSourceLabel: `Bateria ${capacityTotal}mAh 🔋`
    };
}

// ============================================================================
// 6. ATUALIZAÇÃO DA INTERFACE (RENDERIZAÇÃO)
// ============================================================================

function updateUI() {
    // 1. Alterna tela visível
    document.querySelectorAll('.screen-view').forEach(s => s.classList.remove('active'));
    
    const screenMap = {
        'main-menu': 'screen-main-menu',
        'status-p1': 'screen-status-p1',
        'status-p2': 'screen-status-p2',
        'config-list': 'screen-config-list',
        'esp-options': 'screen-esp-options',
        'virtual-keyboard': 'screen-virtual-keyboard',
        'mode-selector': 'screen-mode-selector',
        'power-selector': 'screen-power-selector'
    };

    const targetEl = document.getElementById(screenMap[AppState.currentScreen]);
    if (targetEl) targetEl.classList.add('active');

    // 2. Renderiza dados específicos de cada tela
    switch (AppState.currentScreen) {
        case 'main-menu':
            renderMainMenu();
            break;
        case 'status-p1':
            renderStatusP1();
            break;
        case 'status-p2':
            renderStatusP2();
            break;
        case 'config-list':
            renderConfigList();
            break;
        case 'esp-options':
            renderEspOptions();
            break;
        case 'virtual-keyboard':
            renderVirtualKeyboard();
            break;
        case 'mode-selector':
            renderModeSelector();
            break;
        case 'power-selector':
            renderPowerSelector();
            break;
    }

    // 3. Atualiza badges do cabeçalho
    updateHeaderBadges();
}

function updateHeaderBadges() {
    const queueBadge = document.getElementById('queue-status-badge');
    const queueText = document.getElementById('queue-count-text');
    const count = AppState.pendingQueue.length;

    if (queueBadge && queueText) {
        if (count > 0) {
            queueBadge.style.display = 'flex';
            queueText.textContent = `${count} alteração(ões) pendente(s) (Nós Dormindo)`;
        } else {
            queueBadge.style.display = 'none';
        }
    }

    const apiText = document.getElementById('api-status-text');
    const statusDot = document.querySelector('.status-dot');

    if (apiText && statusDot) {
        if (AppState.receptionSchedule && AppState.receptionSchedule.inReceptionWindow) {
            apiText.textContent = '📡 Conexão Web Pausada (ESP Central recebendo periféricos)';
            statusDot.className = 'status-dot paused';
        } else if (AppState.api.simulatorMode) {
            apiText.textContent = 'API Simulada: Modo Autônomo Ativo';
            statusDot.className = 'status-dot online';
        } else if (AppState.api.isOnline) {
            apiText.textContent = `ESP32 REST: Conectado (${AppState.api.baseUrl})`;
            statusDot.className = 'status-dot online';
        } else {
            apiText.textContent = `ESP32 REST: Desconectado (${AppState.api.baseUrl})`;
            statusDot.className = 'status-dot offline';
        }
    }

    // Atualiza botão de simulação na barra superior
    const btnSim = document.getElementById('btn-toggle-simulation');
    if (btnSim) {
        if (AppState.api.simulatorMode) {
            btnSim.className = 'top-btn simulation-active';
            btnSim.textContent = '🧪 Simulação: ATIVA';
        } else {
            btnSim.className = 'top-btn simulation-inactive';
            btnSim.textContent = '📡 Simulação: DESATIVADA (ESP Real)';
        }
    }

    updateReceptionBadge();
}

function updateReceptionBadge() {
    const badge = document.getElementById('reception-status-badge');
    const textEl = document.getElementById('reception-text');
    if (!badge || !textEl) return;

    const sched = AppState.receptionSchedule;
    if (sched.inReceptionWindow) {
        badge.classList.add('in-reception');
        textEl.textContent = `📡 RECEBENDO DADOS: ${sched.windowSecondsRemaining}s (Pausado)`;
        badge.title = 'ESP Central em janela de recepção de dados dos periféricos. Conexão web pausada para priorizar o rádio.';
    } else {
        badge.classList.remove('in-reception');
        textEl.textContent = `Recepção em ${sched.secondsUntilWindow}s`;
        badge.title = `Próxima janela agendada de recepção de periféricos em ${sched.secondsUntilWindow}s (duração: ${sched.windowDurationSeconds}s)`;
    }
}

// Rastreamento de movimento real do cursor para sincronização unificada de foco (evita brilho duplo)
let lastMouseX = -1;
let lastMouseY = -1;

function mouseMoved(e) {
    if (!e) return true;
    if (e.clientX === lastMouseX && e.clientY === lastMouseY) return false;
    lastMouseX = e.clientX;
    lastMouseY = e.clientY;
    return true;
}

// ----------------------------------------------------------------------------
// Renderização do Menu Principal
// ----------------------------------------------------------------------------
function renderMainMenu() {
    const options = [
        document.getElementById('menu-opt-status'),
        document.getElementById('menu-opt-config')
    ];

    options.forEach((opt, idx) => {
        if (!opt) return;
        if (idx === AppState.focusIndex) {
            opt.classList.add('active-target');
        } else {
            opt.classList.remove('active-target');
        }
    });

    // Notificações
    const notifContainer = document.getElementById('notifications-content');
    const notifBox = document.getElementById('notifications-box');
    
    // Evidencia se não houver nenhum ESP conectado
    const hasAnyConnected = AppState.esps.some(e => e.connectivity === 'ONLINE' || e.connectivity === 'DORMINDO');
    const isRealModeEmpty = !AppState.api.simulatorMode && (!hasAnyConnected || AppState.esps.length === 0);

    if (notifBox) {
        if (isRealModeEmpty) {
            notifBox.classList.add('empty-warning');
        } else {
            notifBox.classList.remove('empty-warning');
        }
    }

    if (notifContainer) {
        notifContainer.innerHTML = '';
        if (isRealModeEmpty) {
            const pWarn = document.createElement('p');
            pWarn.className = 'notif-item warning';
            pWarn.innerHTML = '⚠️ <strong>Nenhum ESP conectado</strong> ao sistema no momento.';
            notifContainer.appendChild(pWarn);

            const pInfo = document.createElement('p');
            pInfo.className = 'notif-item info';
            pInfo.textContent = 'ℹ️ Aguardando dados dos microcontroladores periféricos via ESP-NOW...';
            notifContainer.appendChild(pInfo);

            AppState.notifications.forEach(msg => {
                if (!msg.includes('Nenhum ESP')) {
                    const p = document.createElement('p');
                    p.className = 'notif-item';
                    p.textContent = msg;
                    notifContainer.appendChild(p);
                }
            });
        } else if (AppState.notifications.length === 0) {
            notifContainer.innerHTML = '<p class="notif-item empty">Nenhuma notificação</p>';
        } else {
            AppState.notifications.forEach(msg => {
                const p = document.createElement('p');
                p.className = 'notif-item';
                if (msg.includes('⚠️') || msg.includes('Atenção') || msg.includes('Nenhum ESP')) {
                    p.className = 'notif-item warning';
                } else if (msg.includes('ℹ️') || msg.includes('📡')) {
                    p.className = 'notif-item info';
                }
                p.textContent = msg;
                notifContainer.appendChild(p);
            });
        }
    }
}

// ----------------------------------------------------------------------------
// Renderização do Status - Página 1 (Slide 3)
// ----------------------------------------------------------------------------
function renderStatusP1() {
    const tbody = document.getElementById('tbody-status-p1');
    const emptyMsg = document.getElementById('empty-status-message');
    const table = document.getElementById('table-status-p1');
    const rowIndicator = document.getElementById('status-row-indicator');

    if (!tbody || !table) return;

    tbody.innerHTML = '';

    const hasOnlineOrConfigured = AppState.esps.some(e => e.connectivity === 'ONLINE' || e.connectivity === 'DORMINDO');

    if (!hasOnlineOrConfigured || AppState.esps.length === 0) {
        table.style.display = 'none';
        if (emptyMsg) {
            emptyMsg.classList.add('visible');
            emptyMsg.textContent = 'Nenhum ESP Online ou Configurado';
        }
        if (rowIndicator) rowIndicator.textContent = 'Linhas: 0/0';
        return;
    }

    table.style.display = 'table';
    if (emptyMsg) emptyMsg.classList.remove('visible');

    const perPage = AppState.statusRowsPerPage || 5;
    const totalRowPages = Math.max(1, Math.ceil(AppState.esps.length / perPage));
    if (AppState.statusRowPage >= totalRowPages) {
        AppState.statusRowPage = 0;
    }

    const startIdx = AppState.statusRowPage * perPage;
    const endIdx = Math.min(startIdx + perPage, AppState.esps.length);
    const visibleEsps = AppState.esps.slice(startIdx, endIdx);

    if (rowIndicator) {
        rowIndicator.textContent = `Linhas: ${AppState.statusRowPage + 1}/${totalRowPages} (Nós ${startIdx + 1} a ${endIdx} de ${AppState.esps.length})`;
    }

    visibleEsps.forEach((esp, idx) => {
        const globalIdx = startIdx + idx;
        const tr = document.createElement('tr');
        if (globalIdx === AppState.focusIndex) {
            tr.classList.add('selected-row');
        }

        const badgeClass = esp.connectivity.toLowerCase();
        const est = calculateBatteryEstimate(esp);

        let badgeContent = '';
        if (est.badgeClass === 'dc') {
            badgeContent = '🔌 Rede DC (Contínuo)';
        } else if (est.badgeClass === 'solar') {
            badgeContent = `☀️ ${est.pct}% (Solar - ${esp.batteryCapacityMah || 2500}mAh)`;
        } else {
            badgeContent = `🔋 ${est.pct}% (${est.remainingFormatted})`;
        }

        tr.innerHTML = `
            <td><strong>${esp.name}:</strong></td>
            <td><span class="badge-status ${badgeClass}">${esp.connectivity}</span></td>
            <td>${esp.mode}</td>
            <td>
                <div class="battery-cell">
                    <span class="battery-badge ${est.badgeClass}">${badgeContent}</span>
                    <span class="battery-subtext">${est.powerSourceLabel} • ${est.avgMa} mA méd. • ${est.uptimeFormatted} ativo</span>
                </div>
            </td>
            <td>${esp.lastUpdate}</td>
            <td><strong>${esp.sensorValue}</strong></td>
        `;

        tr.addEventListener('mousemove', (e) => {
            if (mouseMoved(e) && AppState.focusIndex !== globalIdx) {
                AppState.focusIndex = globalIdx;
                updateUI();
            }
        });

        tbody.appendChild(tr);
    });
}

// ----------------------------------------------------------------------------
// Renderização do Status - Página 2 (Slide 4)
// ----------------------------------------------------------------------------
function renderStatusP2() {
    const tbody = document.getElementById('tbody-status-p2');
    const rowIndicatorP2 = document.getElementById('status-row-indicator-p2');
    const table = document.getElementById('table-status-p2');
    const emptyMsg = document.getElementById('empty-status-message-p2');
    if (!tbody || !table) return;

    tbody.innerHTML = '';

    const hasOnlineOrConfigured = AppState.esps.some(e => e.connectivity === 'ONLINE' || e.connectivity === 'DORMINDO');

    if (!hasOnlineOrConfigured || AppState.esps.length === 0) {
        table.style.display = 'none';
        if (emptyMsg) {
            emptyMsg.classList.add('visible');
            emptyMsg.textContent = 'Nenhum ESP Online ou Configurado';
        }
        if (rowIndicatorP2) rowIndicatorP2.textContent = 'Linhas: 0/0';
        return;
    }

    table.style.display = 'table';
    if (emptyMsg) emptyMsg.classList.remove('visible');

    const perPage = AppState.statusRowsPerPage || 5;
    const totalRowPages = Math.max(1, Math.ceil(AppState.esps.length / perPage));
    if (AppState.statusRowPage >= totalRowPages) {
        AppState.statusRowPage = 0;
    }

    const startIdx = AppState.statusRowPage * perPage;
    const endIdx = Math.min(startIdx + perPage, AppState.esps.length);
    const visibleEsps = AppState.esps.slice(startIdx, endIdx);

    if (rowIndicatorP2) {
        rowIndicatorP2.textContent = `Linhas: ${AppState.statusRowPage + 1}/${totalRowPages} (Nós ${startIdx + 1} a ${endIdx} de ${AppState.esps.length})`;
    }

    visibleEsps.forEach((esp, idx) => {
        const globalIdx = startIdx + idx;
        const tr = document.createElement('tr');
        if (globalIdx === AppState.focusIndex) {
            tr.classList.add('selected-row');
        }

        const est = calculateBatteryEstimate(esp);
        const pendingForEsp = AppState.pendingQueue.filter(q => q.espId === esp.id);
        const queueHtml = pendingForEsp.length > 0 
            ? `<span style="color:#f59e0b; font-weight:700;">⏳ ${pendingForEsp.length} comando(s) aguardando despertar</span>`
            : `<span style="color:#10b981;">✓ Sincronizado</span>`;

        tr.innerHTML = `
            <td><strong>${esp.name}:</strong></td>
            <td>${esp.nextUpdate}</td>
            <td><span style="color:#38bdf8; font-weight:600;">⚡ ~${est.avgMa} mA</span> <span style="font-size:0.75rem; color:#94a3b8;">(${esp.mode})</span></td>
            <td><strong>🕒 ${est.uptimeFormatted}</strong> <span style="font-size:0.75rem; color:#94a3b8;">(${est.powerSourceLabel})</span></td>
            <td>${queueHtml}</td>
        `;

        tr.addEventListener('mousemove', (e) => {
            if (mouseMoved(e) && AppState.focusIndex !== globalIdx) {
                AppState.focusIndex = globalIdx;
                updateUI();
            }
        });

        tbody.appendChild(tr);
    });
}

// ----------------------------------------------------------------------------
// Renderização da Lista de Configurações (Slides 5 e 6)
// ----------------------------------------------------------------------------
function renderConfigList() {
    const container = document.getElementById('esp-config-list-container');
    const emptyMsg = document.getElementById('empty-config-message');
    const instructionText = document.getElementById('config-instruction-text');

    if (!container) return;

    container.innerHTML = '';

    if (AppState.esps.length === 0) {
        if (emptyMsg) emptyMsg.classList.add('visible');
        if (instructionText) instructionText.style.display = 'none';
        return;
    }

    if (emptyMsg) emptyMsg.classList.remove('visible');
    if (instructionText) {
        instructionText.style.display = 'block';
        instructionText.textContent = `1 A ${AppState.esps.length} PARA SELECIONAR`;
    }

    AppState.esps.forEach((esp, idx) => {
        const card = document.createElement('div');
        card.className = 'esp-card-item';
        if (idx === AppState.focusIndex) {
            card.classList.add('active-target');
        }

        card.innerHTML = `
            <span class="esp-num">${esp.num}</span>
            <span class="esp-name">${esp.name}</span>
            <span class="badge-status ${esp.connectivity.toLowerCase()}" style="margin-left:auto;">${esp.connectivity}</span>
        `;

        card.addEventListener('mousemove', (e) => {
            if (mouseMoved(e) && AppState.focusIndex !== idx) {
                AppState.focusIndex = idx;
                updateUI();
            }
        });

        card.addEventListener('click', () => {
            AppState.focusIndex = idx;
            selectEspForConfig(esp.id);
        });

        container.appendChild(card);
    });
}

// ----------------------------------------------------------------------------
// Renderização das Opções do ESP Selecionado (Slide 7)
// ----------------------------------------------------------------------------
function renderEspOptions() {
    const esp = AppState.esps.find(e => e.id === AppState.selectedEspId);
    if (!esp) return;

    const titleEl = document.getElementById('selected-esp-title');
    if (titleEl) titleEl.textContent = `${esp.name} (${esp.connectivity})`;

    const intPreview = document.getElementById('val-preview-interval');
    if (intPreview) intPreview.textContent = `(atual: ${esp.interval})`;

    const unitPreview = document.getElementById('val-preview-unit');
    if (unitPreview) unitPreview.textContent = `(atual: ${esp.unit})`;

    const modePreview = document.getElementById('val-preview-mode');
    if (modePreview) modePreview.textContent = `(atual: ${esp.mode})`;

    const powerPreview = document.getElementById('val-preview-power');
    if (powerPreview) {
        const est = calculateBatteryEstimate(esp);
        powerPreview.textContent = `(atual: ${est.powerSourceLabel})`;
    }

    const subopts = document.querySelectorAll('.esp-subopt');
    subopts.forEach((opt, idx) => {
        if (idx === AppState.focusIndex) {
            opt.classList.add('active-target');
        } else {
            opt.classList.remove('active-target');
        }
    });

    // Status de sincronização
    const syncBadge = document.getElementById('esp-sync-badge');
    const syncText = document.getElementById('esp-sync-status-text');
    const pendingForEsp = AppState.pendingQueue.filter(q => q.espId === esp.id);

    if (syncBadge && syncText) {
        if (pendingForEsp.length > 0) {
            syncBadge.style.display = 'flex';
            syncText.textContent = `${pendingForEsp.length} alteração(ões) aguardando o ESP acordar (${esp.mode})`;
        } else {
            syncBadge.style.display = 'none';
        }
    }
}

// ----------------------------------------------------------------------------
// Renderização do Seletor de Modo (ONLINE / LIGHT / DEEP SLEEP)
// ----------------------------------------------------------------------------
function renderModeSelector() {
    const esp = AppState.esps.find(e => e.id === AppState.selectedEspId);
    if (!esp) return;

    const titleEl = document.getElementById('mode-esp-title') || document.getElementById('mode-target-esp-name');
    if (titleEl) titleEl.textContent = esp.name;

    const cards = [
        document.getElementById('mode-opt-online') || document.getElementById('mode-card-online'),
        document.getElementById('mode-opt-light') || document.getElementById('mode-card-light'),
        document.getElementById('mode-opt-deep') || document.getElementById('mode-card-deep')
    ].filter(Boolean);

    cards.forEach((card, idx) => {
        if (idx === AppState.focusIndex) {
            card.classList.add('selected', 'active-target');
        } else {
            card.classList.remove('selected', 'active-target');
        }
    });
}

// ----------------------------------------------------------------------------
// Renderização do Seletor de Fonte de Alimentação (NOVA FUNCIONALIDADE)
// ----------------------------------------------------------------------------
function renderPowerSelector() {
    const esp = AppState.esps.find(e => e.id === AppState.selectedEspId);
    if (!esp) return;

    const titleEl = document.getElementById('power-esp-title');
    if (titleEl) {
        const est = calculateBatteryEstimate(esp);
        titleEl.textContent = `${esp.name} — Fonte atual: ${est.powerSourceLabel}`;
    }

    const cards = [
        document.getElementById('power-opt-bat2500'),
        document.getElementById('power-opt-bat5000'),
        document.getElementById('power-opt-bat1200'),
        document.getElementById('power-opt-dc'),
        document.getElementById('power-opt-solar'),
        document.getElementById('power-opt-custom')
    ].filter(Boolean);

    cards.forEach((card, idx) => {
        if (idx === AppState.focusIndex) {
            card.classList.add('active-target');
        } else {
            card.classList.remove('active-target');
        }
    });
}

// ----------------------------------------------------------------------------
// Renderização do Teclado Virtual (Slide 8)
// ----------------------------------------------------------------------------
function renderVirtualKeyboard() {
    const labelEl = document.getElementById('vk-field-label') || document.getElementById('vk-field-indicator');
    if (labelEl) labelEl.textContent = AppState.vk.fieldTitle;

    const previewEl = document.getElementById('vk-text-preview') || document.getElementById('vk-buffer-text');
    if (previewEl) previewEl.textContent = AppState.vk.text || '';

    // Remove destaque anterior de foco
    document.querySelectorAll('.vk-key').forEach(k => {
        k.classList.remove('active-key', 'focused');
    });

    // Adiciona classe focused na célula correspondente
    const selector = `.vk-key[data-r="${AppState.vk.activeRow}"][data-c="${AppState.vk.activeCol}"]`;
    const targetKeyEl = document.querySelector(selector);
    if (targetKeyEl) {
        targetKeyEl.classList.add('active-key', 'focused');
        targetKeyEl.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    }
}

function initVirtualKeyboardGrid() {
    const grid = document.getElementById('vk-grid') || document.getElementById('virtual-keyboard-grid');
    if (!grid) {
        console.warn('[Init] Elemento do grid do teclado virtual não encontrado no HTML');
        return;
    }
    grid.innerHTML = '';

    AppState.vk.rows.forEach((row, rIdx) => {
        const rowEl = document.createElement('div');
        rowEl.className = 'vk-row';

        row.forEach((char, cIdx) => {
            const keyEl = document.createElement('button');
            keyEl.type = 'button';
            keyEl.className = 'vk-key';
            keyEl.textContent = char;
            keyEl.dataset.r = rIdx;
            keyEl.dataset.c = cIdx;

            if (char === 'ESPAÇO') keyEl.classList.add('key-wide', 'key-space');
            if (char === 'APAGAR') keyEl.classList.add('key-backspace');

            keyEl.addEventListener('mousemove', (e) => {
                if (mouseMoved(e) && (AppState.vk.activeRow !== rIdx || AppState.vk.activeCol !== cIdx)) {
                    AppState.vk.activeRow = rIdx;
                    AppState.vk.activeCol = cIdx;
                    renderVirtualKeyboard();
                }
            });

            keyEl.addEventListener('click', (e) => {
                e.preventDefault();
                e.stopPropagation();
                AppState.vk.activeRow = rIdx;
                AppState.vk.activeCol = cIdx;
                typeKey(char);
            });

            rowEl.appendChild(keyEl);
        });

        grid.appendChild(rowEl);
    });
}

// ============================================================================
// 7. MANIPULAÇÃO DE ENTRADAS DO TECLADO MATRICIAL 4x4
// ============================================================================

function handleKeypadPress(key) {
    console.log(`[Comando] Tecla/Ação acionada: "${key}" | Tela: ${AppState.currentScreen}`);

    // Roteamento de comandos por tela
    switch (AppState.currentScreen) {
        case 'main-menu':
            handleKeypadMainMenu(key);
            break;
        case 'status-p1':
            handleKeypadStatusP1(key);
            break;
        case 'status-p2':
            handleKeypadStatusP2(key);
            break;
        case 'config-list':
            handleKeypadConfigList(key);
            break;
        case 'esp-options':
            handleKeypadEspOptions(key);
            break;
        case 'virtual-keyboard':
            handleKeypadVirtualKeyboard(key);
            break;
        case 'mode-selector':
            handleKeypadModeSelector(key);
            break;
        case 'power-selector':
            handleKeypadPowerSelector(key);
            break;
    }
}

// --- Menu Principal ---
function handleKeypadMainMenu(key) {
    if (key === 'B') { // Subir
        AppState.focusIndex = Math.max(0, AppState.focusIndex - 1);
        updateUI();
    } else if (key === 'C') { // Descer
        AppState.focusIndex = Math.min(1, AppState.focusIndex + 1);
        updateUI();
    } else if (key === 'A' || key === '0') { // Selecionar
        if (AppState.focusIndex === 0) {
            navigateTo('status-p1');
        } else {
            navigateTo('config-list');
        }
    }
}

// --- Status Página 1 (Slide 3) ---
function handleKeypadStatusP1(key) {
    const perPage = AppState.statusRowsPerPage || 5;
    const totalRowPages = Math.max(1, Math.ceil(AppState.esps.length / perPage));

    if (key === 'D') { // Retornar ao menu principal
        navigateBack();
    } else if (key === 'B') { // Próxima Página de Colunas (vai para Página 2)
        navigateTo('status-p2');
    } else if (key === 'C') { // Descer / Trocar de página de linhas (mais ESPs conectados)
        AppState.statusRowPage = (AppState.statusRowPage + 1) % totalRowPages;
        showToastNotification(`Página de nós: Linhas ${AppState.statusRowPage + 1} de ${totalRowPages}`);
        updateUI();
    }
}

// --- Status Página 2 (Slide 4) ---
function handleKeypadStatusP2(key) {
    const perPage = AppState.statusRowsPerPage || 5;
    const totalRowPages = Math.max(1, Math.ceil(AppState.esps.length / perPage));

    if (key === 'D') { // Retornar à página anterior de Colunas (volta para Página 1)
        navigateBack();
    } else if (key === 'C') { // Descer / Trocar de página de linhas (mais ESPs conectados)
        AppState.statusRowPage = (AppState.statusRowPage + 1) % totalRowPages;
        showToastNotification(`Página de nós: Linhas ${AppState.statusRowPage + 1} de ${totalRowPages}`);
        updateUI();
    }
}

// --- Lista de Configurações (Slide 5) ---
function handleKeypadConfigList(key) {
    if (key === 'D') { // Retornar
        navigateBack();
        return;
    }

    // Teclas 1 a 9 para selecionar direto (Slide 5)
    const num = parseInt(key, 10);
    if (!isNaN(num) && num >= 1 && num <= AppState.esps.length) {
        selectEspForConfig(AppState.esps[num - 1].id);
        return;
    }

    if (key === 'B') { // Subir
        AppState.focusIndex = Math.max(0, AppState.focusIndex - 1);
        updateUI();
    } else if (key === 'C') { // Descer
        AppState.focusIndex = Math.min(AppState.esps.length - 1, AppState.focusIndex + 1);
        updateUI();
    } else if (key === '0' || key === 'A') { // Selecionar
        if (AppState.esps[AppState.focusIndex]) {
            selectEspForConfig(AppState.esps[AppState.focusIndex].id);
        }
    }
}

function selectEspForConfig(espId) {
    AppState.selectedEspId = espId;
    navigateTo('esp-options');
}

// --- Opções do ESP Selecionado (Slide 7) ---
function handleKeypadEspOptions(key) {
    const maxIdx = 4; // 5 opções: Renomear (0), Intervalo (1), Unidade (2), Modo (3), Fonte de Alimentação (4)

    if (key === 'D') { // Retornar
        navigateBack();
    } else if (key === 'B') { // Subir
        AppState.focusIndex = Math.max(0, AppState.focusIndex - 1);
        updateUI();
    } else if (key === 'C') { // Descer
        AppState.focusIndex = Math.min(maxIdx, AppState.focusIndex + 1);
        updateUI();
    } else if (key === 'A' || key === '0') { // Selecionar
        const esp = AppState.esps.find(e => e.id === AppState.selectedEspId);
        if (!esp) return;

        if (AppState.focusIndex === 0) {
            openVirtualKeyboard('name', '(NOME SENDO DIGITADO)', esp.name);
        } else if (AppState.focusIndex === 1) {
            openVirtualKeyboard('interval', '(INTERVALO EM MINUTOS)', esp.interval.replace(/\D/g, ''));
        } else if (AppState.focusIndex === 2) {
            openVirtualKeyboard('unit', '(UNIDADE DE MEDIDA)', esp.unit);
        } else if (AppState.focusIndex === 3) {
            navigateTo('mode-selector');
        } else if (AppState.focusIndex === 4) {
            navigateTo('power-selector');
        }
    }
}

// --- Seletor de Modo de Energia ---
function handleKeypadModeSelector(key) {
    const modes = ['ONLINE', 'LIGHT SLEEP', 'DEEP SLEEP'];

    if (key === 'D') {
        navigateBack();
    } else if (key === 'B') {
        AppState.focusIndex = Math.max(0, AppState.focusIndex - 1);
        updateUI();
    } else if (key === 'C') {
        AppState.focusIndex = Math.min(modes.length - 1, AppState.focusIndex + 1);
        updateUI();
    } else if (key === '0' || key === 'A') {
        const selectedMode = modes[AppState.focusIndex];
        saveEspSetting('mode', selectedMode);
        navigateBack();
    }
}

// --- Seletor de Fonte de Alimentação ---
function handleKeypadPowerSelector(key) {
    const powerOptions = [
        { type: 'BATTERY', capacity: 2500, label: 'Bateria Li-ion 2500 mAh' },
        { type: 'BATTERY', capacity: 5000, label: 'Bateria Dupla 5000 mAh' },
        { type: 'BATTERY', capacity: 1200, label: 'Pilhas AA 1200 mAh' },
        { type: 'DC', capacity: null, label: 'Fonte DC (Rede Elétrica)' },
        { type: 'SOLAR', capacity: 2500, label: 'Painel Solar + Bateria' },
        { type: 'CUSTOM_MAH', capacity: null, label: 'Capacidade Personalizada' }
    ];

    if (key === 'D') {
        navigateBack();
    } else if (key === 'B') {
        AppState.focusIndex = Math.max(0, AppState.focusIndex - 1);
        updateUI();
    } else if (key === 'C') {
        AppState.focusIndex = Math.min(powerOptions.length - 1, AppState.focusIndex + 1);
        updateUI();
    } else if (key === '0' || key === 'A') {
        const selected = powerOptions[AppState.focusIndex];
        const esp = AppState.esps.find(e => e.id === AppState.selectedEspId);
        if (!esp) return;

        if (selected.type === 'CUSTOM_MAH') {
            // Abre o teclado virtual para digitar a capacidade em mAh
            openVirtualKeyboard('batteryCapacity', '(CAPACIDADE EM mAh)', String(esp.batteryCapacityMah || 2500));
        } else {
            saveEspSetting('powerSource', selected);
            navigateBack();
        }
    }
}

// --- Teclado Virtual de Digitação (Slide 8) ---
function openVirtualKeyboard(fieldKey, fieldTitle, initialText) {
    AppState.vk.fieldKey = fieldKey;
    AppState.vk.fieldTitle = fieldTitle;
    AppState.vk.text = initialText || '';
    AppState.vk.activeRow = 1; // Linha do Q
    AppState.vk.activeCol = 0; // Coluna do Q
    navigateTo('virtual-keyboard');
}

function handleKeypadVirtualKeyboard(key) {
    const rowCount = AppState.vk.rows.length;
    const curRowLength = AppState.vk.rows[AppState.vk.activeRow].length;

    // Navegação direcional rígida pelo D-Pad do Slide 8:
    // "A" = Cima, "B" = Esquerda, "C" = Direita, "D" = Baixo
    if (key === 'A') { // CIMA
        AppState.vk.activeRow = (AppState.vk.activeRow - 1 + rowCount) % rowCount;
        normalizeVkCol();
        updateUI();
    } else if (key === 'D') { // BAIXO
        AppState.vk.activeRow = (AppState.vk.activeRow + 1) % rowCount;
        normalizeVkCol();
        updateUI();
    } else if (key === 'B') { // ESQUERDA
        AppState.vk.activeCol = (AppState.vk.activeCol - 1 + curRowLength) % curRowLength;
        updateUI();
    } else if (key === 'C') { // DIREITA
        AppState.vk.activeCol = (AppState.vk.activeCol + 1) % curRowLength;
        updateUI();
    } else if (key === '0') { // SELECIONAR CARACTERE
        const selectedChar = AppState.vk.rows[AppState.vk.activeRow][AppState.vk.activeCol];
        typeKey(selectedChar);
    } else if (key === '*') { // CANCELAR
        navigateBack();
    } else if (key === '#') { // SALVAR
        saveVirtualKeyboardValue();
    }
}

function normalizeVkCol() {
    const curRowLength = AppState.vk.rows[AppState.vk.activeRow].length;
    if (AppState.vk.activeCol >= curRowLength) {
        AppState.vk.activeCol = curRowLength - 1;
    }
}

function typeKey(char) {
    if (char === 'ESPAÇO') {
        AppState.vk.text += ' ';
    } else if (char === 'APAGAR') {
        AppState.vk.text = AppState.vk.text.slice(0, -1);
    } else {
        if (AppState.vk.text.length < 24) {
            AppState.vk.text += char;
        }
    }
    updateUI();
}

function saveVirtualKeyboardValue() {
    const val = AppState.vk.text.trim();
    if (val.length === 0) return;

    saveEspSetting(AppState.vk.fieldKey, val);
    navigateBack();
}

// ============================================================================
// 8. ARQUITETURA DE FILA OFFLINE (DEEP/LIGHT SLEEP SYNC)
// ============================================================================

function saveEspSetting(field, value) {
    const esp = AppState.esps.find(e => e.id === AppState.selectedEspId);
    if (!esp) return;

    const isSleeping = esp.connectivity === 'DORMINDO';

    let displayValue = typeof value === 'object' ? value.label : value;

    if (isSleeping) {
        // Enfileira na fila de pendências
        const pendingItem = {
            id: Date.now(),
            espId: esp.id,
            espName: esp.name,
            field: field,
            value: value,
            displayValue: displayValue,
            timestamp: new Date().toLocaleTimeString('pt-BR')
        };

        // Remove item duplicado da mesma propriedade se já existia na fila
        AppState.pendingQueue = AppState.pendingQueue.filter(q => !(q.espId === esp.id && q.field === field));
        AppState.pendingQueue.push(pendingItem);

        showToastNotification(`Configuração na fila offline! O ${esp.name} está em repouso e receberá ao acordar.`);
        console.log('[OfflineSync] Comando enfileirado para nó dormindo:', pendingItem);
    } else {
        // Se estiver ONLINE, aplica imediatamente
        applySettingDirectly(esp, field, value);
        showToastNotification(`Configuração aplicada com sucesso no ${esp.name}!`);
        sendConfigToRemoteAPI(esp.id, field, value);
    }

    updateUI();
}

function applySettingDirectly(esp, field, value) {
    if (field === 'name') esp.name = value;
    else if (field === 'interval') esp.interval = `${value} min`;
    else if (field === 'unit') esp.unit = value;
    else if (field === 'mode') esp.mode = value;
    else if (field === 'powerSource') {
        esp.powerSource = value.type;
        if (value.capacity) esp.batteryCapacityMah = value.capacity;
        else if (value.type === 'DC') esp.batteryCapacityMah = null;
    }
    else if (field === 'batteryCapacity') {
        esp.powerSource = 'BATTERY';
        esp.batteryCapacityMah = Math.max(100, parseInt(value, 10) || 2500);
    }
}

// Quando o nó dormindo acorda e reconecta, esvazia a fila de pendências
function processPendingQueueForEsp(esp) {
    const items = AppState.pendingQueue.filter(q => q.espId === esp.id);
    if (items.length === 0) return;

    console.log(`[OfflineSync] Sincronizando ${items.length} alterações para o ${esp.name} que acabou de acordar!`);

    items.forEach(item => {
        applySettingDirectly(esp, item.field, item.value);
        sendConfigToRemoteAPI(esp.id, item.field, item.value);
    });

    // Remove os itens processados da fila
    AppState.pendingQueue = AppState.pendingQueue.filter(q => q.espId !== esp.id);

    showToastNotification(`⚡ Sincronização concluída: ${items.length} alteração(ões) entregue(s) ao ${esp.name}!`);
    updateUI();
}

// ============================================================================
// 9. COMUNICAÇÃO ASSÍNCRONA COM ESP32 REST API (FETCH API) E MÓDULO DE SIMULAÇÃO
// ============================================================================

function setSimulatorMode(enabled) {
    if (AppState.api.simulatorMode === enabled) return;

    AppState.api.simulatorMode = enabled;

    const simChk = document.getElementById('chk-simulator-mode');
    if (simChk) simChk.checked = enabled;

    if (enabled) {
        // Restaurar dados fictícios de demonstração
        AppState.esps = JSON.parse(JSON.stringify(MOCK_ESPS));
        AppState.notifications = [
            'Modo Simulação Ativo: exibindo exemplos de sensores agrícolas.',
            'O ESP 3 está online!'
        ];
        showToastNotification('🧪 Modo Simulação Ativado (dados de exemplo restaurados)');
        updateUI();
    } else {
        // SAIR DO MÓDULO SIMULAÇÃO:
        // DEIXA DE MOSTRAR EXEMPLOS E PASSA A MOSTRAR APENAS DADOS ENVIADOS PELO ESP CENTRAL!
        AppState.esps = []; // LIMPA TOTALMENTE OS DADOS DE EXEMPLO!
        AppState.notifications = [];
        AppState.statusRowPage = 0;
        AppState.focusIndex = 0;
        
        showToastNotification('📡 Modo Real Ativo: Buscando dados reais do ESP32 Central...');
        updateUI();
        
        // Dispara requisição ao ESP Central imediatamente
        fetchStatusFromESP32();
    }
}

async function fetchStatusFromESP32() {
    // NO MOMENTO EM QUE O ESP ESTÁ AGENDADO PARA RECEBER DADOS DOS PERIFÉRICOS,
    // A CONEXÃO DA INTERFACE COM O ESP CENTRAL CESSA IMEDIATAMENTE!
    if (AppState.receptionSchedule && AppState.receptionSchedule.inReceptionWindow) {
        console.log('[Fetch] Ignorado: Conexão web pausada durante janela agendada de recepção de periféricos.');
        return;
    }

    if (AppState.api.simulatorMode) {
        // No modo simulador, atualiza dados fictícios
        simulateSensorFluctuation();
        return;
    }

    try {
        const response = await fetch(`${AppState.api.baseUrl}/api/status`, {
            method: 'GET',
            headers: { 'Accept': 'application/json' },
            cache: 'no-store'
        });

        if (response.ok) {
            const data = await response.json();
            AppState.api.isOnline = true;
            
            // AO SAIR DO MÓDULO SIMULAÇÃO: MOSTRA APENAS DADOS ENVIADOS PELO ESP CENTRAL!
            const remoteEsps = Array.isArray(data.esps) ? data.esps : [];

            // Se algum nó acordou, entrega as alterações pendentes da fila offline
            remoteEsps.forEach(remoteEsp => {
                const localEsp = AppState.esps.find(e => e.id === remoteEsp.id);
                if (localEsp && localEsp.connectivity === 'DORMINDO' && remoteEsp.connectivity === 'ONLINE') {
                    processPendingQueueForEsp(remoteEsp);
                }
            });

            // Apenas dados reais enviados pelo Central são exibidos
            AppState.esps = remoteEsps;

            // Sincroniza agendamento da janela de recepção se informado pelo ESP Central
            if (data.receptionSchedule) {
                if (typeof data.receptionSchedule.cycleIntervalSeconds === 'number') {
                    AppState.receptionSchedule.cycleSeconds = data.receptionSchedule.cycleIntervalSeconds;
                }
                if (typeof data.receptionSchedule.windowDurationSeconds === 'number') {
                    AppState.receptionSchedule.windowDurationSeconds = data.receptionSchedule.windowDurationSeconds;
                }
                if (typeof data.receptionSchedule.nextWindowSeconds === 'number' && !AppState.receptionSchedule.inReceptionWindow) {
                    AppState.receptionSchedule.secondsUntilWindow = data.receptionSchedule.nextWindowSeconds;
                }
            }

            // NOTIFICAÇÕES: caso não haja nenhum ESP conectado, evidencia nas notificações!
            if (AppState.esps.length === 0) {
                AppState.notifications = [
                    "⚠️ Nenhum ESP conectado ao sistema no momento.",
                    "ℹ️ Aguardando dados dos microcontroladores periféricos via ESP-NOW..."
                ];
                if (Array.isArray(data.notifications)) {
                    data.notifications.forEach(n => {
                        if (!AppState.notifications.includes(n)) AppState.notifications.push(n);
                    });
                }
            } else if (Array.isArray(data.notifications)) {
                AppState.notifications = data.notifications;
            }

            updateUI();
        } else {
            AppState.api.isOnline = false;
            AppState.esps = [];
            AppState.notifications = [
                `⚠️ Erro HTTP ${response.status} ao conectar ao ESP Central.`,
                "⚠️ Nenhum ESP conectado ou acessível no momento."
            ];
            updateUI();
        }
    } catch (err) {
        console.warn('[Fetch] Falha na comunicação com o ESP32 REST server:', err.message);
        AppState.api.isOnline = false;
        AppState.esps = [];
        AppState.notifications = [
            `⚠️ Falha ao conectar ao ESP Central (${AppState.api.baseUrl}).`,
            "⚠️ Nenhum ESP conectado ou acessível no momento."
        ];
        updateUI();
    }
}

async function sendConfigToRemoteAPI(espId, field, value) {
    if (AppState.api.simulatorMode) return;

    // Se a janela de recepção estiver ativa, enfileira para não interferir no rádio
    if (AppState.receptionSchedule && AppState.receptionSchedule.inReceptionWindow) {
        const esp = AppState.esps.find(e => e.id === espId) || { name: espId };
        showToastNotification('📡 ESP Central ocupado recebendo periféricos. Comando mantido na fila offline!');
        return;
    }

    try {
        await fetch(`${AppState.api.baseUrl}/api/config`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ espId, field, value })
        });
    } catch (err) {
        console.error('[Fetch] Erro ao enviar configuração para ESP32:', err);
    }
}

function stopPollingLoop() {
    if (AppState.api.pollingTimer) {
        clearInterval(AppState.api.pollingTimer);
        AppState.api.pollingTimer = null;
    }
}

function startPollingLoop() {
    stopPollingLoop();
    if (AppState.receptionSchedule && AppState.receptionSchedule.inReceptionWindow) {
        console.log('[Polling] Conexão pausada: Janela de recepção de periféricos ativa.');
        return;
    }
    AppState.api.pollingTimer = setInterval(fetchStatusFromESP32, AppState.api.pollingInterval);
}

// ----------------------------------------------------------------------------
// AGENDAMENTO DA JANELA DE RECEPÇÃO DE DADOS DOS PERIFÉRICOS (ESP-NOW)
// ----------------------------------------------------------------------------

function initReceptionSchedule() {
    if (AppState.receptionSchedule.timer) {
        clearInterval(AppState.receptionSchedule.timer);
    }
    AppState.receptionSchedule.timer = setInterval(tickReceptionSchedule, 1000);
}

function tickReceptionSchedule() {
    const sched = AppState.receptionSchedule;
    if (!sched.enabled) return;

    if (!sched.inReceptionWindow) {
        if (sched.secondsUntilWindow > 0) {
            sched.secondsUntilWindow--;
        }
        updateReceptionBadge();

        if (sched.secondsUntilWindow <= 0) {
            enterReceptionWindow();
        }
    } else {
        if (sched.windowSecondsRemaining > 0) {
            sched.windowSecondsRemaining--;
        }
        updateReceptionBadge();

        if (sched.windowSecondsRemaining <= 0) {
            exitReceptionWindow();
        }
    }
}

function enterReceptionWindow() {
    const sched = AppState.receptionSchedule;
    sched.inReceptionWindow = true;
    sched.windowSecondsRemaining = sched.windowDurationSeconds;

    // 1. CESSA A CONEXÃO DA INTERFACE COM O ESP CENTRAL NO MOMENTO AGENDADO
    stopPollingLoop();

    // 2. Notificação e alerta destacado
    const msg = "📡 Janela agendada de recepção ativa! Conexão web pausada temporariamente para que o ESP Central receba os periféricos sem interferência no rádio Wi-Fi.";
    if (!AppState.notifications.includes(msg)) {
        AppState.notifications.unshift(msg);
        if (AppState.notifications.length > 5) AppState.notifications.pop();
    }

    showToastNotification(`📡 Conexão web pausada: ESP Central recebendo dados dos periféricos (${sched.windowDurationSeconds}s)...`);
    updateHeaderBadges();
    updateUI();
}

function exitReceptionWindow() {
    const sched = AppState.receptionSchedule;
    sched.inReceptionWindow = false;
    sched.secondsUntilWindow = sched.cycleSeconds;

    // 1. REESTABELECE A CONEXÃO DA INTERFACE COM O ESP CENTRAL
    startPollingLoop();

    // 2. Busca imediatamente os novos dados enviados pelos periféricos
    fetchStatusFromESP32();

    // 3. Notificação
    const msg = "⚡ Janela de recepção concluída! Conexão web restabelecida e dados atualizados do ESP Central.";
    AppState.notifications.unshift(msg);
    if (AppState.notifications.length > 5) AppState.notifications.pop();

    showToastNotification("⚡ Conexão restabelecida! Dados atualizados recebidos do ESP Central.");
    updateHeaderBadges();
    updateUI();
}

// Simulação dinâmica e autônoma de nós agrícolas (permite testar tudo sem ESP físico)
function simulateSensorFluctuation() {
    // Leve variação nas leituras
    AppState.esps.forEach(esp => {
        if (esp.connectivity === 'ONLINE') {
            if (esp.unit === '% Umidade') {
                const baseVal = parseInt(esp.sensorValue, 10) || 60;
                const delta = (Math.random() * 4 - 2);
                const newVal = Math.min(95, Math.max(20, Math.round(baseVal + delta)));
                esp.sensorValue = `${newVal}% (Umidade Solo)`;
            }
        }
    });
}

function simulateBackgroundEvents() {
    // Simula ciclo periódico de despertar de nós para demonstrar a fila offline
    setInterval(() => {
        if (!AppState.api.simulatorMode) return; // Se saiu da simulação, não gera eventos fictícios

        const sleepingEsps = AppState.esps.filter(e => e.connectivity === 'DORMINDO');
        if (sleepingEsps.length > 0 && Math.random() > 0.4) {
            const wokeEsp = sleepingEsps[Math.floor(Math.random() * sleepingEsps.length)];
            wokeEsp.connectivity = 'ONLINE';
            wokeEsp.lastUpdate = new Date().toLocaleTimeString('pt-BR');

            // Adiciona notificação de despertar
            AppState.notifications.unshift(`O ${wokeEsp.name} acordou e conectou-se a rede!`);
            if (AppState.notifications.length > 4) AppState.notifications.pop();

            // Sincroniza qualquer alteração pendente
            processPendingQueueForEsp(wokeEsp);

            // Volta a dormir após 12 segundos se seu modo for SLEEP
            setTimeout(() => {
                if (wokeEsp.mode.includes('SLEEP')) {
                    wokeEsp.connectivity = 'DORMINDO';
                    updateUI();
                }
            }, 12000);

            updateUI();
        }
    }, 10000);
}

// ============================================================================
// 10. NAVEGAÇÃO E COMANDOS DO COMPUTADOR (TECLADO PC & MOUSE)
// ============================================================================

function cycleStatusRowPage() {
    const perPage = AppState.statusRowsPerPage || 5;
    const totalRowPages = Math.max(1, Math.ceil(AppState.esps.length / perPage));
    AppState.statusRowPage = (AppState.statusRowPage + 1) % totalRowPages;
    showToastNotification(`Página de nós: Linhas ${AppState.statusRowPage + 1} de ${totalRowPages}`);
    updateUI();
}

function navigateUp() {
    switch (AppState.currentScreen) {
        case 'main-menu':
            AppState.focusIndex = 0;
            updateUI();
            break;
        case 'config-list':
            AppState.focusIndex = Math.max(0, AppState.focusIndex - 1);
            updateUI();
            break;
        case 'esp-options':
            AppState.focusIndex = Math.max(0, AppState.focusIndex - 1);
            updateUI();
            break;
        case 'mode-selector':
            AppState.focusIndex = Math.max(0, AppState.focusIndex - 1);
            updateUI();
            break;
        case 'power-selector':
            AppState.focusIndex = Math.max(0, AppState.focusIndex - 1);
            updateUI();
            break;
    }
}

function navigateDown() {
    switch (AppState.currentScreen) {
        case 'main-menu':
            AppState.focusIndex = 1;
            updateUI();
            break;
        case 'status-p1':
        case 'status-p2':
            cycleStatusRowPage();
            break;
        case 'config-list':
            AppState.focusIndex = Math.min(AppState.esps.length - 1, AppState.focusIndex + 1);
            updateUI();
            break;
        case 'esp-options':
            AppState.focusIndex = Math.min(4, AppState.focusIndex + 1);
            updateUI();
            break;
        case 'mode-selector':
            AppState.focusIndex = Math.min(2, AppState.focusIndex + 1);
            updateUI();
            break;
        case 'power-selector':
            AppState.focusIndex = Math.min(5, AppState.focusIndex + 1);
            updateUI();
            break;
    }
}

function confirmCurrentSelection() {
    switch (AppState.currentScreen) {
        case 'main-menu':
            if (AppState.focusIndex === 0) navigateTo('status-p1');
            else navigateTo('config-list');
            break;
        case 'status-p1':
            navigateTo('status-p2');
            break;
        case 'status-p2':
            navigateBack();
            break;
        case 'config-list':
            if (AppState.esps[AppState.focusIndex]) {
                selectEspForConfig(AppState.esps[AppState.focusIndex].id);
            }
            break;
        case 'esp-options':
            handleKeypadEspOptions('0');
            break;
        case 'mode-selector':
            handleKeypadModeSelector('0');
            break;
        case 'power-selector':
            handleKeypadPowerSelector('0');
            break;
        case 'virtual-keyboard':
            saveVirtualKeyboardValue();
            break;
    }
}

function setupEventListeners() {
    // 1. Atalhos de Teclado do Computador (PC / Web)
    window.addEventListener('keydown', (e) => {
        // Se o modal de API estiver aberto:
        const modal = document.getElementById('modal-api-config');
        if (modal && modal.classList.contains('open')) {
            if (e.key === 'Escape') {
                e.preventDefault();
                modal.classList.remove('open');
            } else if (e.key === 'Enter') {
                e.preventDefault();
                const saveBtn = document.getElementById('btn-modal-save');
                if (saveBtn) saveBtn.click();
            }
            return;
        }

        // Se estiver no teclado virtual de digitação (Slide 8):
        if (AppState.currentScreen === 'virtual-keyboard') {
            if (e.key === 'Backspace') {
                e.preventDefault();
                typeKey('APAGAR');
                return;
            } else if (e.key === 'Enter') {
                e.preventDefault();
                saveVirtualKeyboardValue();
                return;
            } else if (e.key === 'Escape') {
                e.preventDefault();
                navigateBack();
                return;
            } else if (e.key === ' ') {
                e.preventDefault();
                typeKey('ESPAÇO');
                return;
            } else if (e.key === 'ArrowUp') {
                e.preventDefault();
                handleKeypadVirtualKeyboard('A');
                return;
            } else if (e.key === 'ArrowDown') {
                e.preventDefault();
                handleKeypadVirtualKeyboard('D');
                return;
            } else if (e.key === 'ArrowLeft') {
                e.preventDefault();
                handleKeypadVirtualKeyboard('B');
                return;
            } else if (e.key === 'ArrowRight') {
                e.preventDefault();
                handleKeypadVirtualKeyboard('C');
                return;
            } else if (e.key.length === 1 && !e.ctrlKey && !e.altKey && !e.metaKey) {
                // Digitação direta no teclado virtual com qualquer tecla física do PC
                e.preventDefault();
                typeKey(e.key.toUpperCase());
                return;
            }
        }

        // Navegação universal nas demais telas
        if (e.key === 'Escape') {
            e.preventDefault();
            navigateBack();
            return;
        }

        if (e.key === 'Tab') {
            e.preventDefault();
            if (AppState.currentScreen === 'status-p1') navigateTo('status-p2');
            else if (AppState.currentScreen === 'status-p2') navigateBack();
            return;
        }

        if (e.key === 'ArrowUp' || e.key === 'w' || e.key === 'W') {
            e.preventDefault();
            navigateUp();
            return;
        }

        if (e.key === 'ArrowDown' || e.key === 's' || e.key === 'S') {
            e.preventDefault();
            navigateDown();
            return;
        }

        if (e.key === 'ArrowLeft' || e.key === 'a' || e.key === 'A') {
            if (AppState.currentScreen === 'status-p2') {
                e.preventDefault();
                navigateBack();
                return;
            }
        }

        if (e.key === 'ArrowRight' || e.key === 'd' || e.key === 'D') {
            if (AppState.currentScreen === 'status-p1') {
                e.preventDefault();
                navigateTo('status-p2');
                return;
            }
        }

        if (e.key === 'PageDown' || e.key === 'PageUp') {
            e.preventDefault();
            if (AppState.currentScreen === 'status-p1' || AppState.currentScreen === 'status-p2') {
                cycleStatusRowPage();
            }
            return;
        }

        if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault();
            confirmCurrentSelection();
            return;
        }

        // Seleção numérica rápida (1 a 9) na tela de configurações
        if (AppState.currentScreen === 'config-list') {
            const num = parseInt(e.key, 10);
            if (!isNaN(num) && num >= 1 && num <= AppState.esps.length) {
                e.preventDefault();
                selectEspForConfig(AppState.esps[num - 1].id);
                return;
            }
        }

        // Suporte opcional a comandos legados
        const legacyMap = {
            'b': 'B', 'c': 'C', '0': '0', '*': '*', '#': '#'
        };
        if (legacyMap[e.key]) {
            handleKeypadPress(legacyMap[e.key]);
        }
    });

    // 2. Modal de API REST do ESP32
    const btnToggleApiModal = document.getElementById('btn-toggle-api-modal');
    if (btnToggleApiModal) {
        btnToggleApiModal.addEventListener('click', (e) => {
            e.preventDefault();
            openApiModal();
        });
    }

    const modal = document.getElementById('modal-api-config');
    const btnCloseModal = document.getElementById('btn-close-modal');
    if (btnCloseModal && modal) {
        btnCloseModal.addEventListener('click', (e) => {
            e.preventDefault();
            modal.classList.remove('open');
        });
    }

    const btnModalCancel = document.getElementById('btn-modal-cancel');
    if (btnModalCancel && modal) {
        btnModalCancel.addEventListener('click', (e) => {
            e.preventDefault();
            modal.classList.remove('open');
        });
    }

    // Botão de alternar modo simulação na barra superior
    const btnSim = document.getElementById('btn-toggle-simulation');
    if (btnSim) {
        btnSim.addEventListener('click', (e) => {
            e.preventDefault();
            setSimulatorMode(!AppState.api.simulatorMode);
        });
    }

    const btnModalSave = document.getElementById('btn-modal-save');
    if (btnModalSave) {
        btnModalSave.addEventListener('click', (e) => {
            e.preventDefault();
            const ipInput = document.getElementById('input-esp-ip');
            const intervalInput = document.getElementById('input-sync-interval');
            const simChk = document.getElementById('chk-simulator-mode');
            const cycleInput = document.getElementById('input-reception-cycle');
            const durationInput = document.getElementById('input-reception-duration');

            if (ipInput) AppState.api.baseUrl = ipInput.value.trim();
            if (intervalInput) AppState.api.pollingInterval = parseInt(intervalInput.value, 10);
            if (cycleInput) AppState.receptionSchedule.cycleSeconds = parseInt(cycleInput.value, 10) || 60;
            if (durationInput) AppState.receptionSchedule.windowDurationSeconds = parseInt(durationInput.value, 10) || 10;

            if (simChk) setSimulatorMode(simChk.checked);

            startPollingLoop();
            if (modal) modal.classList.remove('open');
            updateUI();
            showToastNotification('Configurações da API atualizadas!');
        });
    }

    // 3. Ações de clique e foco com mouse nas opções do Menu Principal
    const menuStatus = document.getElementById('menu-opt-status');
    if (menuStatus) {
        menuStatus.addEventListener('mousemove', (e) => {
            if (mouseMoved(e) && AppState.focusIndex !== 0) {
                AppState.focusIndex = 0;
                updateUI();
            }
        });
        menuStatus.addEventListener('click', (e) => {
            e.preventDefault();
            AppState.focusIndex = 0;
            navigateTo('status-p1');
        });
    }

    const menuConfig = document.getElementById('menu-opt-config');
    if (menuConfig) {
        menuConfig.addEventListener('mousemove', (e) => {
            if (mouseMoved(e) && AppState.focusIndex !== 1) {
                AppState.focusIndex = 1;
                updateUI();
            }
        });
        menuConfig.addEventListener('click', (e) => {
            e.preventDefault();
            AppState.focusIndex = 1;
            navigateTo('config-list');
        });
    }

    // 4. Ações de navegação nos botões de rodapé das telas de Status
    const btnStatusRet = document.getElementById('btn-status-return');
    if (btnStatusRet) {
        btnStatusRet.addEventListener('click', (e) => {
            e.preventDefault();
            navigateBack();
        });
    }

    const btnStatusNext = document.getElementById('btn-status-next-page');
    if (btnStatusNext) {
        btnStatusNext.addEventListener('click', (e) => {
            e.preventDefault();
            navigateTo('status-p2');
        });
    }

    const btnStatusPrev = document.getElementById('btn-status-prev-page');
    if (btnStatusPrev) {
        btnStatusPrev.addEventListener('click', (e) => {
            e.preventDefault();
            navigateBack();
        });
    }

    const btnStatusScroll = document.getElementById('btn-status-scroll');
    if (btnStatusScroll) {
        btnStatusScroll.addEventListener('click', (e) => {
            e.preventDefault();
            cycleStatusRowPage();
        });
    }

    const btnStatusScrollP2 = document.getElementById('btn-status-scroll-p2');
    if (btnStatusScrollP2) {
        btnStatusScrollP2.addEventListener('click', (e) => {
            e.preventDefault();
            cycleStatusRowPage();
        });
    }

    // 5. Botões de retorno com mouse das telas de Configuração
    const btnConfigRet = document.getElementById('btn-config-list-return');
    if (btnConfigRet) {
        btnConfigRet.addEventListener('click', (e) => {
            e.preventDefault();
            navigateBack();
        });
    }

    const btnEspOptRet = document.getElementById('btn-esp-options-return');
    if (btnEspOptRet) {
        btnEspOptRet.addEventListener('click', (e) => {
            e.preventDefault();
            navigateBack();
        });
    }

    const btnModeRet = document.getElementById('btn-mode-return');
    if (btnModeRet) {
        btnModeRet.addEventListener('click', (e) => {
            e.preventDefault();
            navigateBack();
        });
    }

    const btnPowerRet = document.getElementById('btn-power-return');
    if (btnPowerRet) {
        btnPowerRet.addEventListener('click', (e) => {
            e.preventDefault();
            navigateBack();
        });
    }

    // 6. Botões do Teclado Virtual (Cancelar e Salvar)
    const btnVkCancel = document.getElementById('btn-vk-cancel');
    if (btnVkCancel) {
        btnVkCancel.addEventListener('click', (e) => {
            e.preventDefault();
            navigateBack();
        });
    }

    const btnVkSave = document.getElementById('btn-vk-save');
    if (btnVkSave) {
        btnVkSave.addEventListener('click', (e) => {
            e.preventDefault();
            saveVirtualKeyboardValue();
        });
    }

    // 7. Clique e foco nas subopções de configuração do ESP
    document.querySelectorAll('.esp-subopt').forEach((el, idx) => {
        el.addEventListener('mousemove', (e) => {
            if (mouseMoved(e) && AppState.focusIndex !== idx) {
                AppState.focusIndex = idx;
                updateUI();
            }
        });
        el.addEventListener('click', (e) => {
            e.preventDefault();
            AppState.focusIndex = idx;
            handleKeypadEspOptions('0');
        });
    });

    // 8. Clique e foco nos modos de energia
    document.querySelectorAll('.mode-card').forEach((el, idx) => {
        el.addEventListener('mousemove', (e) => {
            if (mouseMoved(e) && AppState.focusIndex !== idx) {
                AppState.focusIndex = idx;
                updateUI();
            }
        });
        el.addEventListener('click', (e) => {
            e.preventDefault();
            AppState.focusIndex = idx;
            handleKeypadModeSelector('0');
        });
    });

    // 9. Clique e foco nas fontes de alimentação
    document.querySelectorAll('.power-card').forEach((el, idx) => {
        el.addEventListener('mousemove', (e) => {
            if (mouseMoved(e) && AppState.focusIndex !== idx) {
                AppState.focusIndex = idx;
                updateUI();
            }
        });
        el.addEventListener('click', (e) => {
            e.preventDefault();
            AppState.focusIndex = idx;
            handleKeypadPowerSelector('0');
        });
    });
}

function openApiModal() {
    const modal = document.getElementById('modal-api-config');
    if (!modal) return;

    const ipInput = document.getElementById('input-esp-ip');
    if (ipInput) ipInput.value = AppState.api.baseUrl;

    const simChk = document.getElementById('chk-simulator-mode');
    if (simChk) simChk.checked = AppState.api.simulatorMode;

    const cycleInput = document.getElementById('input-reception-cycle');
    if (cycleInput) cycleInput.value = AppState.receptionSchedule.cycleSeconds;

    const durationInput = document.getElementById('input-reception-duration');
    if (durationInput) durationInput.value = AppState.receptionSchedule.windowDurationSeconds;

    // Renderiza lista da fila offline no modal
    const queueListEl = document.getElementById('modal-queue-items');
    if (queueListEl) {
        queueListEl.innerHTML = '';
        if (AppState.pendingQueue.length === 0) {
            queueListEl.innerHTML = '<p class="empty-queue-msg">Nenhum comando na fila no momento.</p>';
        } else {
            AppState.pendingQueue.forEach(item => {
                const div = document.createElement('div');
                div.style.padding = '4px 0';
                div.style.borderBottom = '1px solid #334155';
                const valText = item.displayValue || item.value;
                div.innerHTML = `<strong>${item.espName}:</strong> alterar <em>${item.field}</em> para "<strong>${valText}</strong>" (${item.timestamp})`;
                queueListEl.appendChild(div);
            });
        }
    }

    modal.classList.add('open');
}

function showToastNotification(text) {
    let toast = document.getElementById('app-toast');
    if (!toast) {
        toast = document.createElement('div');
        toast.id = 'app-toast';
        toast.style.position = 'fixed';
        toast.style.bottom = '30px';
        toast.style.left = '50%';
        toast.style.transform = 'translateX(-50%)';
        toast.style.background = 'rgba(15, 23, 42, 0.95)';
        toast.style.border = '1px solid #00f2fe';
        toast.style.color = '#ffffff';
        toast.style.padding = '12px 24px';
        toast.style.borderRadius = '30px';
        toast.style.boxShadow = '0 10px 30px rgba(0,0,0,0.5)';
        toast.style.zIndex = '9999';
        toast.style.fontFamily = 'var(--font-body)';
        toast.style.fontSize = '0.95rem';
        toast.style.fontWeight = '500';
        toast.style.transition = 'opacity 0.3s ease, transform 0.3s ease';
        document.body.appendChild(toast);
    }

    toast.textContent = text;
    toast.style.opacity = '1';
    toast.style.transform = 'translateX(-50%) translateY(0)';

    clearTimeout(toast._timeout);
    toast._timeout = setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transform = 'translateX(-50%) translateY(10px)';
    }, 3500);
}
