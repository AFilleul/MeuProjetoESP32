/**
 * SÍTIO ONLINE - SISTEMA DE MONITORAMENTO AGRÍCOLA ESP32
 * Lógica Completa da Aplicação Web, Navegação Matricial 4x4 e Fila de Sincronização Offline
 */

// ============================================================================
// 1. ESTADO GLOBAL DA APLICAÇÃO
// ============================================================================

const AppState = {
    currentScreen: 'main-menu', // 'main-menu', 'status-p1', 'status-p2', 'config-list', 'esp-options', 'virtual-keyboard', 'mode-selector'
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

    // Fundo Dinâmico
    bgSlideTimer: null,
    activeBgIndex: 1,

    // Teclado Virtual (Slide 8)
    vk: {
        fieldKey: 'name', // 'name', 'interval', 'unit'
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

    // Base de Dados dos Nós ESP32 Conectados (10 nós para teste de paginação vertical)
    esps: [
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
            batteryPct: 45,
            uptimeHours: 65,
            batteryCapacityMah: 2500
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
            batteryCapacityMah: 2500
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
            batteryPct: 62,
            uptimeHours: 210,
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
            batteryPct: 82,
            uptimeHours: 90,
            batteryCapacityMah: 2500
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
            batteryCapacityMah: 2500
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
            batteryCapacityMah: 2500
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
            batteryPct: 73,
            uptimeHours: 160,
            batteryCapacityMah: 2500
        }
    ],

    // Notificações
    notifications: [
        'O ESP 3 está online!',
        'O ESP 9 está online!'
    ]
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
        simulateBackgroundEvents();
    } catch (e) {
        console.error('[Init] Erro na simulação de eventos:', e);
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
// 5. CÁLCULO E ESTIMATIVA DE BATERIA DO ESP32
// ============================================================================

/**
 * Calcula a autonomia estimada e consumo médio do ESP32 com base:
 * - Capacidade nominal da bateria (padrão 18650 Li-ion 2500 mAh)
 * - Porcentagem de carga atual (batteryPct)
 * - Modo operacional (ONLINE, LIGHT SLEEP, DEEP SLEEP)
 * - Ciclo de atividade e intervalo de medição/envio REST
 * - Tempo total acumulado em operação (uptimeHours)
 */
function calculateBatteryEstimate(esp) {
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

    const uptimeH = esp.uptimeHours || 120;
    let uptimeFormatted = '';
    if (uptimeH < 24) {
        uptimeFormatted = `${uptimeH}h`;
    } else {
        const d = Math.floor(uptimeH / 24);
        const h = uptimeH % 24;
        uptimeFormatted = `${d}d ${h}h`;
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
        badgeClass
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
        'mode-selector': 'screen-mode-selector'
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
        if (AppState.api.simulatorMode) {
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
    if (notifContainer) {
        notifContainer.innerHTML = '';
        if (AppState.notifications.length === 0) {
            notifContainer.innerHTML = '<p class="notif-item empty">Nenhuma notificação</p>';
        } else {
            AppState.notifications.forEach(msg => {
                const p = document.createElement('p');
                p.className = 'notif-item';
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

        tr.innerHTML = `
            <td><strong>${esp.name}:</strong></td>
            <td><span class="badge-status ${badgeClass}">${esp.connectivity}</span></td>
            <td>${esp.mode}</td>
            <td>
                <div class="battery-cell">
                    <span class="battery-badge ${est.badgeClass}">🔋 ${est.pct}% (${est.remainingFormatted})</span>
                    <span class="battery-subtext">${est.avgMa} mA méd. • ${est.uptimeFormatted} ativo</span>
                </div>
            </td>
            <td>${esp.lastUpdate}</td>
            <td><strong>${esp.sensorValue}</strong></td>
        `;
        tbody.appendChild(tr);
    });
}

// ----------------------------------------------------------------------------
// Renderização do Status - Página 2 (Slide 4)
// ----------------------------------------------------------------------------
function renderStatusP2() {
    const tbody = document.getElementById('tbody-status-p2');
    const rowIndicatorP2 = document.getElementById('status-row-indicator-p2');
    if (!tbody) return;

    tbody.innerHTML = '';

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
            <td><strong>🕒 ${est.uptimeFormatted}</strong> <span style="font-size:0.75rem; color:#94a3b8;">(${est.uptimeHours}h totais)</span></td>
            <td>${queueHtml}</td>
        `;
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

        card.addEventListener('click', () => {
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
    console.log(`[Keypad] Tecla acionada: "${key}" | Tela: ${AppState.currentScreen}`);

    // Feedback visual na tecla correspondente do painel 4x4
    const btn = document.querySelector(`.kpad-key[data-key="${key}"]`);
    if (btn) {
        btn.classList.add('pressed');
        setTimeout(() => btn.classList.remove('pressed'), 140);
    }

    const lastKeyDisplay = document.getElementById('last-key-display');
    if (lastKeyDisplay) lastKeyDisplay.textContent = key;

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
    const maxIdx = 3; // 4 opções: Renomear, Intervalo, Unidade, Modo

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

    if (isSleeping) {
        // Enfileira na fila de pendências
        const pendingItem = {
            id: Date.now(),
            espId: esp.id,
            espName: esp.name,
            field: field,
            value: value,
            timestamp: new Date().toLocaleTimeString('pt-BR')
        };

        // Remove item duplicado da mesma propriedade se já existia na fila
        AppState.pendingQueue = AppState.pendingQueue.filter(q => !(q.espId === esp.id && q.field === field));
        AppState.pendingQueue.push(pendingItem);

        showToastNotification(`Configuração salva na fila offline! O ${esp.name} está em repouso e receberá assim que acordar.`);
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
// 9. COMUNICAÇÃO ASSÍNCRONA COM ESP32 REST API (FETCH API)
// ============================================================================

async function fetchStatusFromESP32() {
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
            
            if (Array.isArray(data.esps)) {
                // Atualiza ou insere dados recebidos
                data.esps.forEach(remoteEsp => {
                    const localEsp = AppState.esps.find(e => e.id === remoteEsp.id);
                    if (localEsp) {
                        const wasSleeping = localEsp.connectivity === 'DORMINDO';
                        Object.assign(localEsp, remoteEsp);
                        
                        // Se o ESP acordou, entrega as pendências
                        if (wasSleeping && localEsp.connectivity === 'ONLINE') {
                            processPendingQueueForEsp(localEsp);
                        }
                    } else {
                        AppState.esps.push(remoteEsp);
                    }
                });
            }

            if (Array.isArray(data.notifications)) {
                AppState.notifications = data.notifications;
            }

            updateUI();
        } else {
            AppState.api.isOnline = false;
        }
    } catch (err) {
        console.warn('[Fetch] Falha na comunicação com o ESP32 REST server:', err.message);
        AppState.api.isOnline = false;
        updateUI();
    }
}

async function sendConfigToRemoteAPI(espId, field, value) {
    if (AppState.api.simulatorMode) return;

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

function startPollingLoop() {
    if (AppState.api.pollingTimer) clearInterval(AppState.api.pollingTimer);
    AppState.api.pollingTimer = setInterval(fetchStatusFromESP32, AppState.api.pollingInterval);
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
// 10. CONFIGURAÇÃO DE EVENTOS DO TECLADO DO PC E BOTÕES DA INTERFACE (SEGURO)
// ============================================================================

function setupEventListeners() {
    // 1. Teclado Físico do Computador mapeado para o Teclado Matricial 4x4
    window.addEventListener('keydown', (e) => {
        // Se estiver no teclado virtual de digitação (Slide 8):
        if (AppState.currentScreen === 'virtual-keyboard') {
            if (e.key === 'Backspace') {
                e.preventDefault();
                typeKey('APAGAR');
                return;
            } else if (e.key === 'Enter') {
                e.preventDefault();
                const selectedChar = AppState.vk.rows[AppState.vk.activeRow][AppState.vk.activeCol];
                typeKey(selectedChar);
                return;
            } else if (e.key === 'Escape') {
                e.preventDefault();
                navigateBack();
                return;
            } else if (e.key === ' ') {
                e.preventDefault();
                typeKey('ESPAÇO');
                return;
            }
        }

        const keyMap = {
            'ArrowUp': 'A',
            'ArrowLeft': 'B',
            'ArrowRight': 'C',
            'ArrowDown': 'D',
            'Enter': '0',
            ' ': '0',
            'Escape': '*',
            'Backspace': 'APAGAR',
            'a': 'A', 'A': 'A',
            'b': 'B', 'B': 'B',
            'c': 'C', 'C': 'C',
            'd': 'D', 'D': 'D',
            '0': '0', '1': '1', '2': '2', '3': '3', '4': '4',
            '5': '5', '6': '6', '7': '7', '8': '8', '9': '9',
            '*': '*', '#': '#'
        };

        if (keyMap[e.key]) {
            handleKeypadPress(keyMap[e.key]);
        } else if (AppState.currentScreen === 'virtual-keyboard' && e.key.length === 1 && !e.ctrlKey && !e.altKey && !e.metaKey) {
            // Digitação direta no teclado virtual se for caractere imprimível
            typeKey(e.key.toUpperCase());
        }
    });

    // 2. Teclas do Widget Lateral 4x4 (Clique com o Mouse)
    document.querySelectorAll('.kpad-key').forEach(btn => {
        btn.addEventListener('click', (e) => {
            e.preventDefault();
            e.stopPropagation();
            const key = btn.dataset.key;
            if (key) handleKeypadPress(key);
        });
    });

    // 3. Controles de Cabeçalho e Painéis
    const btnToggleKeypad = document.getElementById('btn-toggle-keypad');
    if (btnToggleKeypad) {
        btnToggleKeypad.addEventListener('click', (e) => {
            e.preventDefault();
            const panel = document.getElementById('keypad-sim-panel');
            if (panel) panel.classList.toggle('hidden');
        });
    }

    const btnCloseKeypad = document.getElementById('btn-close-keypad');
    if (btnCloseKeypad) {
        btnCloseKeypad.addEventListener('click', (e) => {
            e.preventDefault();
            const panel = document.getElementById('keypad-sim-panel');
            if (panel) panel.classList.add('hidden');
        });
    }

    // 4. Modal de API
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

    const btnModalSave = document.getElementById('btn-modal-save');
    if (btnModalSave) {
        btnModalSave.addEventListener('click', (e) => {
            e.preventDefault();
            const ipInput = document.getElementById('input-esp-ip');
            const intervalInput = document.getElementById('input-sync-interval');
            const simChk = document.getElementById('chk-simulator-mode');

            if (ipInput) AppState.api.baseUrl = ipInput.value.trim();
            if (intervalInput) AppState.api.pollingInterval = parseInt(intervalInput.value, 10);
            if (simChk) AppState.api.simulatorMode = simChk.checked;

            startPollingLoop();
            if (modal) modal.classList.remove('open');
            updateUI();
            showToastNotification('Configurações da API atualizadas!');
        });
    }

    // 5. Ações de clique nas opções do menu principal
    const menuStatus = document.getElementById('menu-opt-status');
    if (menuStatus) {
        menuStatus.addEventListener('click', (e) => {
            e.preventDefault();
            AppState.focusIndex = 0;
            navigateTo('status-p1');
        });
    }

    const menuConfig = document.getElementById('menu-opt-config');
    if (menuConfig) {
        menuConfig.addEventListener('click', (e) => {
            e.preventDefault();
            AppState.focusIndex = 1;
            navigateTo('config-list');
        });
    }

    // 6. Ações de navegação do rodapé de Status
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
            handleKeypadPress('C');
        });
    }

    const btnStatusScrollP2 = document.getElementById('btn-status-scroll-p2');
    if (btnStatusScrollP2) {
        btnStatusScrollP2.addEventListener('click', (e) => {
            e.preventDefault();
            handleKeypadPress('C');
        });
    }

    // 7. Retornos das demais telas
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

    // 8. Teclado virtual rodapé
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

    // 9. Clique nas subopções de configuração
    document.querySelectorAll('.esp-subopt').forEach((el, idx) => {
        el.addEventListener('click', (e) => {
            e.preventDefault();
            AppState.focusIndex = idx;
            handleKeypadEspOptions('0');
        });
    });

    // 10. Clique nos modos de energia
    document.querySelectorAll('.mode-card').forEach((el, idx) => {
        el.addEventListener('click', (e) => {
            e.preventDefault();
            AppState.focusIndex = idx;
            handleKeypadModeSelector('0');
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
                div.innerHTML = `<strong>${item.espName}:</strong> alterar <em>${item.field}</em> para "<strong>${item.value}</strong>" (${item.timestamp})`;
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
