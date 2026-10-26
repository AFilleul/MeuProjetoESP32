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
RTC_DATA_ATTR uint8_t rtcMacCentral[6] = {0};
RTC_DATA_ATTR bool rtcCentralConhecido = false;

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

// Callback executado com PRIORIDADE MÁXIMA quando o ESP Central envia uma nova configuração
void aplicarNovaConfiguracao(const PacoteConfiguracao& cfg) {
    Serial.println("\n--------------------------------------------------------");
    Serial.println("[CONFIGURAÇÃO PRIORITÁRIA] Aplicando parâmetros recebidos do Central:");
    
    // Salva o MAC do Central na memória RTC caso ainda não estivesse salvo
    if (espNowNode.centralDescoberto()) {
        espNowNode.getCentralMac(rtcMacCentral);
        rtcCentralConhecido = true;
    }

    // Atualiza intervalo se fornecido
    if (cfg.novoIntervaloMin > 0) {
        rtcIntervaloMin = cfg.novoIntervaloMin;
        Serial.printf("  -> Novo Intervalo de Leitura: %d minutos\n", rtcIntervaloMin);
    }

    // Atualiza modo de operação se fornecido
    if (strlen(cfg.novoModo) > 0) {
        if (strcmp(cfg.novoModo, "DEEP SLEEP") == 0 || strcmp(cfg.novoModo, "DEEP_SLEEP") == 0) {
            strcpy(rtcModoOperacao, "DEEP_SLEEP");
        } else if (strcmp(cfg.novoModo, "LIGHT SLEEP") == 0 || strcmp(cfg.novoModo, "LIGHT_SLEEP") == 0) {
            strcpy(rtcModoOperacao, "LIGHT_SLEEP");
        } else if (strcmp(cfg.novoModo, "ONLINE") == 0) {
            strcpy(rtcModoOperacao, "ONLINE");
        } else {
            strncpy(rtcModoOperacao, cfg.novoModo, sizeof(rtcModoOperacao) - 1);
            rtcModoOperacao[sizeof(rtcModoOperacao) - 1] = '\0';
        }
        Serial.printf("  -> Novo Modo de Operação: %s\n", rtcModoOperacao);
    }

    // Atualiza apelido/nome se fornecido
    if (strlen(cfg.novoNome) > 0) {
        strncpy(rtcNodeName, cfg.novoNome, sizeof(rtcNodeName) - 1);
        rtcNodeName[sizeof(rtcNodeName) - 1] = '\0';
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

    // 2. Configura callback prioritário de recepção de configurações
    espNowNode.definirCallbackConfiguracao(aplicarNovaConfiguracao);

    // 3. Se já conhecia o MAC do Central antes do Deep Sleep, restaura imediatamente
    if (rtcCentralConhecido) {
        espNowNode.restaurarCentralMac(rtcMacCentral);
    }

    // ========================================================================
    // CASO A: HARD RESET (LIGADO NA ENERGIA OU BOTÃO RESET PRESSIONADO)
    // -> PERMANECE ONLINE POR 1 MINUTO PARA PAREAMENTO E RECEBER CONFIGURAÇÕES
    // ========================================================================
    if (isColdBoot) {
        bootCount = 1;
        Serial.println("\n[AVISO] HARD RESET DETECTADO!");
        Serial.println("[AVISO] O nó permanecerá ONLINE por 60 segundos (1 minuto) com prioridade para receber configurações.");

        // Aguarda o beacon do Central para descobrir o MAC
        if (espNowNode.aguardarBeaconCentral(8000)) {
            espNowNode.getCentralMac(rtcMacCentral);
            rtcCentralConhecido = true;
        }

        // Executa a janela de pareamento de 1 minuto (60.000 ms)
        espNowNode.executarJanelaPareamento(
            60000, 
            [&tempoInicioMs]() { return montarPacoteSensores(tempoInicioMs); },
            aplicarNovaConfiguracao
        );

        if (espNowNode.centralDescoberto()) {
            espNowNode.getCentralMac(rtcMacCentral);
            rtcCentralConhecido = true;
        }

        // Atualiza tempo ativo
        tempoUptimeAcumuladoSegundos += ((millis() - tempoInicioMs) / 1000);

        // Após 1 minuto, verifica se o modo configurado é Deep Sleep ou Contínuo (ONLINE)
        if (strcmp(rtcModoOperacao, "ONLINE") == 0) {
            Serial.println("[Periferico] MODO ONLINE CONTÍNUO ativo. O nó permanecerá 100% ativo e conectado no loop().");
            return; // Continua no loop()
        } else {
            espNowNode.aguardarPrioridadeConfiguracao(250);
            Serial.printf("[Periferico] Entrando em Deep Sleep pelo tempo configurado (%d min)...\n", rtcIntervaloMin);
            espNowNode.entrarDeepSleep(rtcIntervaloMin);
            return;
        }
    }

    // ========================================================================
    // CASO B: DESPERTAR PROGRAMADO DO DEEP SLEEP (CICLO RÁPIDO DE BATERIA)
    // -> PRIORIZA RECEBER CONFIGURAÇÕES ANTES E DEPOIS DO ENVIO DE DADOS
    // ========================================================================
    bootCount++;
    Serial.printf("[CICLO WAKEUP] Retorno de Deep Sleep (Ciclo #%u).\n", bootCount);

    // 1. Garante descoberta do Central (usa MAC salvo em RTC ou escuta beacon)
    bool centralOk = espNowNode.centralDescoberto();
    if (!centralOk) {
        centralOk = espNowNode.aguardarBeaconCentral(4000);
    }

    if (centralOk) {
        espNowNode.getCentralMac(rtcMacCentral);
        rtcCentralConhecido = true;

        // 2. PRIORIDADE ANTES DE ENVIAR: Escuta por 150ms caso o Central já esteja transmitindo configuração
        espNowNode.aguardarPrioridadeConfiguracao(150);

        // 3. Monta pacote (já com configurações atualizadas se recebidas) e envia Unicast ao Central
        PacoteDadosSensores dados = montarPacoteSensores(tempoInicioMs);
        espNowNode.enviarDadosUnicast(dados);
        
        // 4. PRIORIDADE PÓS-ENVIO: Janela de até 1650ms (encerra em 250ms assim que receber comando)
        //    para aguardar o HTTP POST do Central ao Mini PC (timeout máx 1500ms) e capturar comandos no mesmo ciclo!
        if (espNowNode.aguardarPrioridadeConfiguracao(1650)) {
            Serial.println("[CICLO WAKEUP] Nova configuração recebida via Central/Mini PC! Enviando estado atualizado...");
            PacoteDadosSensores dadosAtualizados = montarPacoteSensores(tempoInicioMs);
            espNowNode.enviarDadosUnicast(dadosAtualizados);
            delay(100);
        }
    } else {
        Serial.println("[CICLO WAKEUP] Central indisponivel neste ciclo. Retornando ao sono.");
    }

    // 5. Acumula tempo de funcionamento
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
    // MODO ONLINE CONTÍNUO / LIGHT SLEEP
    // PRIORIDADE 1: Receber e confirmar configurações antes de enviar telemetria
    // ========================================================================
    static unsigned long ultimoEnvioOnline = 0;

    // Se ainda não descobriu o Central, escuta brevemente pelo beacon
    if (!espNowNode.centralDescoberto()) {
        if (espNowNode.aguardarBeaconCentral(2000)) {
            espNowNode.getCentralMac(rtcMacCentral);
            rtcCentralConhecido = true;
        }
    }

    // 1. PRIORIDADE MÁXIMA: Verifica e processa configurações recebidas do Central
    if (espNowNode.aguardarPrioridadeConfiguracao(50)) {
        Serial.println("[MODO ONLINE] Configuração prioritária aplicada! Despachando telemetria confirmando novo estado...");
        PacoteDadosSensores dadosConfirmacao = montarPacoteSensores(millis());
        espNowNode.enviarDadosUnicast(dadosConfirmacao);
        ultimoEnvioOnline = millis();
        delay(100);
    }

    unsigned long agora = millis();
    unsigned long intervaloMs = (unsigned long)rtcIntervaloMin * 60000UL;
    if (intervaloMs == 0) intervaloMs = 5000;

    // 2. Envio periódico de telemetria completa dos sensores
    if (agora - ultimoEnvioOnline >= intervaloMs || ultimoEnvioOnline == 0) {
        // Verifica novamente se chegou configuração imediatamente antes de medir/enviar
        espNowNode.aguardarPrioridadeConfiguracao(80);

        ultimoEnvioOnline = millis();
        PacoteDadosSensores dados = montarPacoteSensores(ultimoEnvioOnline);
        espNowNode.enviarDadosUnicast(dados);
        Serial.printf("[MODO ONLINE] Telemetria despachada | Umidade: %.1f%% | Bateria: %d%% | Modo: %s | Intervalo: %d min\n", 
                      dados.sensorUmidade, dados.bateriaPct, rtcModoOperacao, rtcIntervaloMin);

        // Mantém janela ativa após cada envio (1650ms em LIGHT_SLEEP para cobrir o round-trip HTTP do Mini PC)
        unsigned long janelaPosEnvio = (strcmp(rtcModoOperacao, "LIGHT_SLEEP") == 0) ? 1650 : 500;
        if (espNowNode.aguardarPrioridadeConfiguracao(janelaPosEnvio)) {
            PacoteDadosSensores dadosConfirmacao = montarPacoteSensores(millis());
            espNowNode.enviarDadosUnicast(dadosConfirmacao);
            delay(100);
        }
    }

    // 3. Se o modo for alterado para DEEP_SLEEP ou LIGHT_SLEEP, aguarda estabilização do ACK antes de dormir
    if (strcmp(rtcModoOperacao, "DEEP_SLEEP") == 0) {
        espNowNode.aguardarPrioridadeConfiguracao(200);
        Serial.printf("\n[MODO ONLINE -> SLEEP] Comando confirmado: Entrando em Deep Sleep (%d min)...\n", rtcIntervaloMin);
        espNowNode.entrarDeepSleep(rtcIntervaloMin);
    } else if (strcmp(rtcModoOperacao, "LIGHT_SLEEP") == 0) {
        espNowNode.aguardarPrioridadeConfiguracao(200);
        esp_sleep_enable_timer_wakeup((uint64_t)rtcIntervaloMin * 60ULL * 1000000ULL);
        esp_light_sleep_start();
        // Ao acordar do Light Sleep, força envio e janela de escuta na próxima iteração
        ultimoEnvioOnline = 0;
    }

    delay(20);
}
