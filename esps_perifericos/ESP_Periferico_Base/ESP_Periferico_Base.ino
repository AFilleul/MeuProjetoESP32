#include <Arduino.h>
#include "EspNowPeriferico.h"
#include "config_periferico.h"

// Instância modular do protocolo ESP-NOW para o periférico
EspNowPeriferico espNowNode;

// ============================================================================
// VARIÁVEIS NA MEMÓRIA RTC (PRESERVADAS DURANTE O DEEP SLEEP)
// ============================================================================
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint32_t tempoUptimeAcumuladoSegundos = 0;
RTC_DATA_ATTR uint16_t rtcIntervaloMin = READ_INTERVAL_MIN;
RTC_DATA_ATTR char rtcModoOperacao[16] = OPERATING_MODE;
RTC_DATA_ATTR char rtcNodeName[32] = NODE_NAME;

// ============================================================================
// LEITURA DOS SENSORES
// ============================================================================
float lerUmidadeSolo() {
    #ifdef PIN_SENSOR_VCC
    pinMode(PIN_SENSOR_VCC, OUTPUT);
    digitalWrite(PIN_SENSOR_VCC, HIGH); // Alimenta o sensor apenas durante a medição
    delay(30);
    #endif

    int leituraAdc = analogRead(PIN_SENSOR_ANALOG);

    #ifdef PIN_SENSOR_VCC
    digitalWrite(PIN_SENSOR_VCC, LOW);  // Desliga o sensor para poupar bateria
    #endif

    // Calibração analógica: 4095 (Seco) -> 1200 (Encharcado)
    float umidade = map(leituraAdc, 4095, 1200, 0, 100);
    return constrain(umidade, 0.0, 100.0);
}

void lerBateria(uint16_t &outMv, uint8_t &outPct) {
    // Leitura típica de bateria Li-ion 18650 (3.3V a 4.2V)
    outMv = 3980; // Milivolts medidos
    outPct = (uint8_t)map(constrain(outMv, 3300, 4200), 3300, 4200, 0, 100);
}

// Monta o pacote de dados atual para envio ao Central
PacoteDadosSensores montarPacoteSensores(unsigned long tempoInicioMs) {
    PacoteDadosSensores pacote = {};
    pacote.tipo = MSG_DADOS_SENSORES;
    strncpy(pacote.nodeMac, espNowNode.getLocalMacString().c_str(), sizeof(pacote.nodeMac) - 1);
    strncpy(pacote.nodeId, NODE_ID, sizeof(pacote.nodeId) - 1);
    strncpy(pacote.nodeName, rtcNodeName, sizeof(pacote.nodeName) - 1);
    
    pacote.sensorUmidade = lerUmidadeSolo();
    pacote.temperatura = 24.8; // Leitura de temperatura ambiente opcional
    
    lerBateria(pacote.bateriaMv, pacote.bateriaPct);
    pacote.cicloWakeup = bootCount;
    pacote.tempoUptimeSegundos = tempoUptimeAcumuladoSegundos + ((millis() - tempoInicioMs) / 1000);
    strncpy(pacote.modoOperacao, rtcModoOperacao, sizeof(pacote.modoOperacao) - 1);
    pacote.pareamentoAtivo = 0;

    return pacote;
}

// Callback executado quando o ESP Central envia uma nova configuração
void aplicarNovaConfiguracao(const PacoteConfiguracao& cfg) {
    Serial.println("\n--------------------------------------------------------");
    Serial.println("[CONFIGURAÇÃO] Aplicando parâmetros recebidos do Central:");
    
    // Atualiza intervalo se fornecido
    if (cfg.novoIntervaloMin > 0) {
        rtcIntervaloMin = cfg.novoIntervaloMin;
        Serial.printf("  -> Novo Intervalo de Leitura: %d minutos\n", rtcIntervaloMin);
    }

    // Atualiza modo de operação se fornecido
    if (strlen(cfg.novoModo) > 0) {
        strncpy(rtcModoOperacao, cfg.novoModo, sizeof(rtcModoOperacao) - 1);
        Serial.printf("  -> Novo Modo de Operação: %s\n", rtcModoOperacao);
    }

    // Atualiza apelido/nome se fornecido
    if (strlen(cfg.novoNome) > 0) {
        strncpy(rtcNodeName, cfg.novoNome, sizeof(rtcNodeName) - 1);
        Serial.printf("  -> Novo Nome do Nó: %s\n", rtcNodeName);
    }
    Serial.println("--------------------------------------------------------");

    // Envia confirmação (ACK) imediata de volta ao Central
    espNowNode.enviarAckConfiguracao(NODE_ID, true, rtcIntervaloMin, rtcModoOperacao);
}

// ============================================================================
// INICIALIZAÇÃO (SETUP)
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(300);

    unsigned long tempoInicioMs = millis();
    bool isColdBoot = EspNowPeriferico::isHardReset();

    Serial.println("\n========================================================");
    Serial.printf("  SÍTIO ONLINE - ESP32 PERIFÉRICO [%s]\n", NODE_ID);
    Serial.printf("  Motivo de Reset: %s\n", EspNowPeriferico::getMotivoResetString().c_str());
    Serial.println("========================================================");

    // 1. Inicializa o rádio ESP-NOW
    if (!espNowNode.iniciar(ESPNOW_WIFI_CHANNEL)) {
        Serial.println("[Periferico] Erro ao iniciar rádio ESP-NOW. Entrando em Deep Sleep de segurança...");
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

        // Aguarda o beacon do Central para descobrir o MAC
        espNowNode.aguardarBeaconCentral(8000);

        // Executa a janela de pareamento de 1 minuto (60.000 ms)
        // Durante este tempo, o nó envia pacotes de presença a cada 5s e processa configs do Central
        espNowNode.executarJanelaPareamento(
            60000, 
            [&tempoInicioMs]() { return montarPacoteSensores(tempoInicioMs); },
            aplicarNovaConfiguracao
        );

        // Atualiza tempo ativo
        tempoUptimeAcumuladoSegundos += ((millis() - tempoInicioMs) / 1000);

        // Após 1 minuto, verifica se o modo configurado é Deep Sleep ou Contínuo (ONLINE)
        if (strcmp(rtcModoOperacao, "ONLINE") == 0) {
            Serial.println("[Periferico] MODO ONLINE CONTÍNUO ativo. O nó permanecerá 100% ativo e conectado no loop().");
            return; // Continua no loop()
        } else {
            Serial.printf("[Periferico] Entrando em Deep Sleep pelo tempo configurado (%d min)...\n", rtcIntervaloMin);
            espNowNode.entrarDeepSleep(rtcIntervaloMin);
            return;
        }
    }

    // ========================================================================
    // CASO B: DESPERTAR PROGRAMADO DO DEEP SLEEP (CICLO RÁPIDO DE BATERIA)
    // -> ACORDA, MEDE, DESPACHA EM POUCOS MILISSEGUNDOS E RETORNA A DORMIR
    // ========================================================================
    bootCount++;
    Serial.printf("[CICLO WAKEUP] Retorno de Deep Sleep (Ciclo #%u).\n", bootCount);

    // 1. Escuta brevemente pelo beacon do Central
    bool centralOk = espNowNode.aguardarBeaconCentral(4000);

    if (centralOk) {
        // 2. Monta pacote e envia Unicast direto
        PacoteDadosSensores dados = montarPacoteSensores(tempoInicioMs);
        espNowNode.enviarDadosUnicast(dados);
        
        // Breve janela de escuta (250 ms) para capturar eventual configuração pendente enviada pelo Central
        unsigned long tEspera = millis();
        while (millis() - tEspera < 250) {
            delay(10);
            if (espNowNode.temConfiguracaoRecebida()) {
                Serial.println("[CICLO WAKEUP] Nova configuração pendente recebida e aplicada com sucesso!");
                break;
            }
        }
    } else {
        Serial.println("[CICLO WAKEUP] Central indisponivel neste ciclo. Retornando ao sono.");
    }

    // 3. Acumula tempo de funcionamento
    tempoUptimeAcumuladoSegundos += ((millis() - tempoInicioMs) / 1000) + 1;

    // Se o periférico foi reconfigurado para o modo ONLINE durante o ciclo de despertar:
    if (strcmp(rtcModoOperacao, "ONLINE") == 0) {
        Serial.println("[CICLO WAKEUP] Nó transicionado para MODO ONLINE CONTÍNUO. Permanecendo ativo no loop().");
        return; // Não entra em Deep Sleep, passa a executar o loop() continuamente!
    }

    Serial.printf("[CICLO WAKEUP] Ciclo concluído em %lu ms. Retornando ao sono por %d min...\n", 
                  millis() - tempoInicioMs, rtcIntervaloMin);

    espNowNode.entrarDeepSleep(rtcIntervaloMin);
}

void loop() {
    // ========================================================================
    // MODO ONLINE CONTÍNUO (NÓ LIGADO PERMANENTEMENTE NA TOMADA / FONTE DC)
    // O microcontrolador não desliga o rádio nem a CPU, respondendo instantaneamente
    // ========================================================================
    static unsigned long ultimoEnvioOnline = 0;
    unsigned long agora = millis();

    // 1. Envio periódico de telemetria completa dos sensores
    unsigned long intervaloMs = (unsigned long)rtcIntervaloMin * 60000UL;
    if (intervaloMs == 0) intervaloMs = 5000; // Padrão de segurança: 5 segundos

    if (agora - ultimoEnvioOnline >= intervaloMs || ultimoEnvioOnline == 0) {
        ultimoEnvioOnline = agora;
        PacoteDadosSensores dados = montarPacoteSensores(agora);
        espNowNode.enviarDadosUnicast(dados);
        Serial.printf("[MODO ONLINE] Telemetria despachada | Umidade: %.1f%% | Bateria: %d%% | Uptime: %lu s\n", 
                      dados.sensorUmidade, dados.bateriaPct, (unsigned long)dados.tempoUptimeSegundos);
    }

    // 2. Se a qualquer momento o modo for alterado pela Web para "DEEP_SLEEP", o nó entra em repouso
    if (strcmp(rtcModoOperacao, "DEEP_SLEEP") == 0) {
        Serial.printf("\n[MODO ONLINE -> SLEEP] Comando recebido da Web: Entrando em Deep Sleep (%d min)...\n", rtcIntervaloMin);
        espNowNode.entrarDeepSleep(rtcIntervaloMin);
    } else if (strcmp(rtcModoOperacao, "LIGHT_SLEEP") == 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)rtcIntervaloMin * 60ULL * 1000000ULL);
        esp_light_sleep_start();
    }

    delay(20);
}
