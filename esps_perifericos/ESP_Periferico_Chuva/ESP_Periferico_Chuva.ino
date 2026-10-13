#include <Arduino.h>
#include "EspNowPeriferico.h"
#include "config_periferico.h"
#include "SensorChuvaMHRD.h"

// ============================================================================
// INSTÂNCIAS DE HARDWARE E PROTOCOLO
// ============================================================================

// Instância modular do protocolo ESP-NOW
EspNowPeriferico espNowNode;

// Instância do Sensor de Chuva MH-RD com chaveamento BJT NPN (Low-Side)
ConfigSensorChuva cfgChuva = {
    .pinoBjtBase           = PIN_BJT_BASE,
    .pinoAnalogicoAO       = PIN_SENSOR_AO,
    .pinoDigitalDO         = PIN_SENSOR_DO,
    .tempoEstabilizacaoMs  = 50,    // 50 ms de saturação para leitura estável
    .adcLimiarSeco         = 3200,  // ADC >= 3200 -> Seco
    .adcLimiarChuvaForte   = 1800,  // ADC < 1800 -> Chuva Forte
    .qtdAmostrasAdc        = 8      // Média de 8 amostras para filtrar ruído
};

SensorChuvaMHRD sensorChuva(cfgChuva);

// ============================================================================
// VARIÁVEIS NA MEMÓRIA RTC (PRESERVADAS DURANTE O DEEP SLEEP)
// ============================================================================
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint32_t tempoUptimeAcumuladoSegundos = 0;
RTC_DATA_ATTR uint16_t rtcIntervaloMin = READ_INTERVAL_MIN;
RTC_DATA_ATTR char rtcModoOperacao[16] = OPERATING_MODE;
RTC_DATA_ATTR char rtcNodeName[32] = NODE_NAME;

// ============================================================================
// LEITURA DOS SENSORES E BATERIA
// ============================================================================
void lerBateria(uint16_t &outMv, uint8_t &outPct) {
    if (strcmp(POWER_SOURCE_TYPE, "DC") == 0) {
        // Alimentação contínua via fonte/tomada (5V USB/regulador)
        outMv = 5000;
        outPct = 100;
    } else {
        // Estimativa para bateria Li-Ion 18650 (3.3V a 4.2V)
        outMv = 3980;
        outPct = (uint8_t)map(constrain(outMv, 3300, 4200), 3300, 4200, 0, 100);
    }
}

// Monta o pacote de dados com a leitura do sensor de chuva e status
PacoteDadosSensores montarPacoteSensores(unsigned long tempoInicioMs) {
    PacoteDadosSensores pacote = {};
    pacote.tipo = MSG_DADOS_SENSORES;
    strncpy(pacote.nodeMac, espNowNode.getLocalMacString().c_str(), sizeof(pacote.nodeMac) - 1);
    strncpy(pacote.nodeId, NODE_ID, sizeof(pacote.nodeId) - 1);
    strncpy(pacote.nodeName, rtcNodeName, sizeof(pacote.nodeName) - 1);

    // 1. Executa a leitura inteligente com ativação transitória do transistor BJT NPN (50ms)
    LeituraSensorChuva leitura = sensorChuva.realizarLeitura();
    sensorChuva.imprimirLeituraSerial(leitura);

    // Mapeia a intensidade de chuva (0.0% a 100.0%) no campo de umidade da telemetria
    pacote.sensorUmidade = leitura.porcentagemIntensidade;
    pacote.temperatura = 25.0f; // Temperatura de referência ou sensor auxiliar

    // 2. Coleta status de bateria e métricas de execução
    lerBateria(pacote.bateriaMv, pacote.bateriaPct);
    pacote.cicloWakeup = bootCount;
    pacote.tempoUptimeSegundos = tempoUptimeAcumuladoSegundos + ((millis() - tempoInicioMs) / 1000);
    strncpy(pacote.modoOperacao, rtcModoOperacao, sizeof(pacote.modoOperacao) - 1);
    pacote.pareamentoAtivo = 0;

    return pacote;
}

// Callback acionado quando uma nova configuração for enviada da Interface Web
void aplicarNovaConfiguracao(const PacoteConfiguracao& cfg) {
    Serial.println("\n--------------------------------------------------------");
    Serial.println("[CONFIGURAÇÃO] Aplicando parâmetros recebidos do Central:");

    if (cfg.novoIntervaloMin > 0) {
        rtcIntervaloMin = cfg.novoIntervaloMin;
        Serial.printf("  -> Novo Intervalo de Leitura: %d minutos\n", rtcIntervaloMin);
    }

    if (strlen(cfg.novoModo) > 0) {
        if (strcmp(cfg.novoModo, "DEEP SLEEP") == 0 || strcmp(cfg.novoModo, "DEEP_SLEEP") == 0) {
            strcpy(rtcModoOperacao, "DEEP_SLEEP");
        } else if (strcmp(cfg.novoModo, "LIGHT SLEEP") == 0 || strcmp(cfg.novoModo, "LIGHT_SLEEP") == 0) {
            strcpy(rtcModoOperacao, "LIGHT_SLEEP");
        } else {
            strncpy(rtcModoOperacao, cfg.novoModo, sizeof(rtcModoOperacao) - 1);
            rtcModoOperacao[sizeof(rtcModoOperacao) - 1] = '\0';
        }
        Serial.printf("  -> Novo Modo de Operação: %s\n", rtcModoOperacao);
    }

    if (strlen(cfg.novoNome) > 0) {
        strncpy(rtcNodeName, cfg.novoNome, sizeof(rtcNodeName) - 1);
        Serial.printf("  -> Novo Nome do Nó: %s\n", rtcNodeName);
    }
    Serial.println("--------------------------------------------------------");

    // Envia ACK de volta ao Central
    espNowNode.enviarAckConfiguracao(NODE_ID, true, rtcIntervaloMin, rtcModoOperacao);
}

// ============================================================================
// INICIALIZAÇÃO (SETUP)
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(300);

    // Inicializa o sensor de chuva (coloca o BJT NPN em CORTE imediato para proteger as trilhas)
    sensorChuva.begin();

    unsigned long tempoInicioMs = millis();
    bool isColdBoot = EspNowPeriferico::isHardReset();

    Serial.println("\n========================================================");
    Serial.printf("  SÍTIO ONLINE - PERIFÉRICO SENSOR DE CHUVA [%s]\n", NODE_ID);
    Serial.printf("  Motivo de Reset: %s\n", EspNowPeriferico::getMotivoResetString().c_str());
    Serial.println("========================================================");

    // 1. Inicializa o rádio ESP-NOW
    if (!espNowNode.iniciar(ESPNOW_WIFI_CHANNEL)) {
        Serial.println("[Chuva] Erro ao iniciar rádio ESP-NOW. Entrando em Deep Sleep de segurança...");
        espNowNode.entrarDeepSleep(rtcIntervaloMin);
        return;
    }

    // 2. Configura callback de recepção de configurações
    espNowNode.definirCallbackConfiguracao(aplicarNovaConfiguracao);

    // ========================================================================
    // CASO A: HARD RESET (LIGADO NA ENERGIA OU BOTÃO RESET PRESSIONADO)
    // -> PERMANECE ONLINE POR 1 MINUTO PARA PAREAMENTO E RECEBER CONFIGURAÇÕES
    // ========================================================================
    if (isColdBoot) {
        bootCount = 1;
        Serial.println("\n[AVISO] HARD RESET DETECTADO!");
        Serial.println("[AVISO] O nó permanecerá ONLINE por 60 segundos (1 minuto) para parear e receber configurações.");

        // Aguarda beacon do Central
        espNowNode.aguardarBeaconCentral(8000);

        // Executa janela de pareamento de 60 segundos
        espNowNode.executarJanelaPareamento(
            60000,
            [&tempoInicioMs]() { return montarPacoteSensores(tempoInicioMs); },
            aplicarNovaConfiguracao
        );

        tempoUptimeAcumuladoSegundos += ((millis() - tempoInicioMs) / 1000);

        if (strcmp(rtcModoOperacao, "ONLINE") == 0) {
            Serial.println("[Chuva] MODO ONLINE CONTÍNUO ativo. O nó permanecerá 100% ativo no loop().");
            return; // Prossegue para loop() contínuo
        } else {
            Serial.printf("[Chuva] Entrando em Deep Sleep pelo tempo configurado (%d min)...\n", rtcIntervaloMin);
            espNowNode.entrarDeepSleep(rtcIntervaloMin);
            return;
        }
    }

    // ========================================================================
    // CASO B: DESPERTAR PROGRAMADO DO DEEP SLEEP (CICLO RÁPIDO DE BATERIA)
    // -> ACORDA, MEDE EM 50ms, DESPACHA E RETORNA A DORMIR
    // ========================================================================
    bootCount++;
    Serial.printf("[CICLO WAKEUP] Retorno de Deep Sleep (Ciclo #%u).\n", bootCount);

    bool centralOk = espNowNode.aguardarBeaconCentral(4000);

    if (centralOk) {
        PacoteDadosSensores dados = montarPacoteSensores(tempoInicioMs);
        espNowNode.enviarDadosUnicast(dados);

        // Escuta breve (250 ms) para pegar configuração pendente
        unsigned long tEspera = millis();
        while (millis() - tEspera < 250) {
            delay(10);
            if (espNowNode.temConfiguracaoRecebida()) {
                Serial.println("[CICLO WAKEUP] Nova configuração pendente aplicada com sucesso!");
                break;
            }
        }
    } else {
        Serial.println("[CICLO WAKEUP] Central indisponivel neste ciclo. Retornando ao sono.");
    }

    tempoUptimeAcumuladoSegundos += ((millis() - tempoInicioMs) / 1000) + 1;

    if (strcmp(rtcModoOperacao, "ONLINE") == 0) {
        Serial.println("[CICLO WAKEUP] Transicionado para MODO ONLINE CONTÍNUO. Permanecendo no loop().");
        return;
    }

    Serial.printf("[CICLO WAKEUP] Ciclo concluído em %lu ms. Retornando ao sono por %d min...\n", 
                  millis() - tempoInicioMs, rtcIntervaloMin);

    espNowNode.entrarDeepSleep(rtcIntervaloMin);
}

// ============================================================================
// LOOP PRINCIPAL (MODO ONLINE CONTÍNUO)
// ============================================================================
void loop() {
    static unsigned long ultimoEnvioOnline = 0;
    unsigned long agora = millis();

    unsigned long intervaloMs = (unsigned long)rtcIntervaloMin * 60000UL;
    if (intervaloMs == 0) intervaloMs = 5000; // Padrão seguro de 5s para demonstração

    // Envio periódico da leitura de chuva
    if (agora - ultimoEnvioOnline >= intervaloMs || ultimoEnvioOnline == 0) {
        ultimoEnvioOnline = agora;
        PacoteDadosSensores dados = montarPacoteSensores(agora);
        espNowNode.enviarDadosUnicast(dados);
        Serial.printf("[MODO ONLINE] Chuva: %.1f%% | Bateria: %d%% | Uptime: %lu s\n", 
                      dados.sensorUmidade, dados.bateriaPct, (unsigned long)dados.tempoUptimeSegundos);
    }

    // Se a interface Web alterar o modo para DEEP_SLEEP ou LIGHT_SLEEP, executa a transição imediata
    if (strcmp(rtcModoOperacao, "DEEP_SLEEP") == 0) {
        Serial.printf("\n[MODO ONLINE -> SLEEP] Comando recebido da Web: Entrando em Deep Sleep (%d min)...\n", rtcIntervaloMin);
        espNowNode.entrarDeepSleep(rtcIntervaloMin);
    } else if (strcmp(rtcModoOperacao, "LIGHT_SLEEP") == 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)rtcIntervaloMin * 60ULL * 1000000ULL);
        esp_light_sleep_start();
    }

    delay(20);
}
