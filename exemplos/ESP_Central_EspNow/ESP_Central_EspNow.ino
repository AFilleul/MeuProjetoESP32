#include <Arduino.h>
#include "EspNowCentral.h"
#include "DeviceManager.h"

// ============================================================================
// ESP32 CENTRAL - PROTOCOLO ESP-NOW (COMUNICAÇÃO DIRETA SEM ROTEADOR)
// ============================================================================
// Este sketch gerencia o ESP32 Central operando puramente via ESP-NOW:
// 1. Transmite periodicamente Broadcast (FF:FF:FF:FF:FF:FF) anunciando o Central
// 2. Recebe telemetria Unicast dos nós periféricos via callback C++
// 3. Detecta nós em "Modo Pareamento" (1 minuto online pós-hard reset) e despacha configurações
// 4. Recebe confirmação (ACK) das configurações aplicadas pelos nós
// 5. Atualiza os dispositivos registrados no DeviceManager
// ============================================================================

EspNowCentral espNowCentral;
DeviceManager deviceManager(30000); // 30 KB margem de segurança de RAM

unsigned long ultimoBroadcastMs = 0;
const unsigned long INTERVALO_BROADCAST_MS = 3000; // Anuncia a cada 3 segundos

// Callback disparado automaticamente quando um nó periférico envia dados Unicast
void aoReceberDadosPeriferico(const uint8_t* mac, const PacoteDadosSensores& dados) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    Serial.println("\n--------------------------------------------------------");
    Serial.printf("[CENTRAL EVENTO] Telemetria recebida do nó: %s (%s)\n", dados.nodeName, dados.nodeId);
    Serial.printf("  MAC Remoto: %s\n", macStr);
    Serial.printf("  Umidade Solo: %.1f%%\n", dados.sensorUmidade);
    Serial.printf("  Bateria: %d%% (%u mV)\n", dados.bateriaPct, dados.bateriaMv);
    Serial.printf("  Modo de Energia: %s | Ciclo Wakeup: #%u\n", dados.modoOperacao, dados.cicloWakeup);
    
    // Se o periférico realizou Hard Reset e está em modo pareamento ativo (janela de 1 min)
    if (dados.pareamentoAtivo) {
        Serial.printf("  >>> NÓ EM PAREAMENTO ATIVO! (Online pós hard-reset)\n");
        Serial.printf("  >>> Enviando parâmetros de configuração para '%s'...\n", dados.nodeId);

        PacoteConfiguracao cfg = {};
        cfg.tipo = MSG_CONFIG_PARAMETROS;
        strncpy(cfg.targetNodeId, dados.nodeId, sizeof(cfg.targetNodeId) - 1);
        cfg.novoIntervaloMin = 15; // Exemplo: define intervalo de 15 minutos
        strncpy(cfg.novoModo, "DEEP_SLEEP", sizeof(cfg.novoModo) - 1);
        strncpy(cfg.novoNome, "Solo Setor A (Configurado)", sizeof(cfg.novoNome) - 1);
        cfg.timestampMillis = millis();

        espNowCentral.enviarConfiguracao(mac, cfg);
    }
    Serial.println("--------------------------------------------------------");

    // Registra ou atualiza no DeviceManager
    if (!deviceManager.hasDevice(dados.nodeId)) {
        deviceManager.registerDevice(dados.nodeId, dados.nodeName, macStr);
    } else {
        deviceManager.updateLastSeen(dados.nodeId);
    }
}

// Callback disparado quando o periférico confirma que aplicou a configuração
void aoReceberAckConfig(const uint8_t* mac, const PacoteAckConfig& ack) {
    Serial.println("\n========================================================");
    Serial.printf("[CENTRAL SUCESSO] Nó '%s' aplicou e confirmou a configuracao!\n", ack.nodeId);
    Serial.printf("  Intervalo Salvo: %d minutos | Modo Salvo: %s\n", ack.intervaloAplicadoMin, ack.modoAplicado);
    Serial.println("========================================================");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================================");
    Serial.println("  SITIO ONLINE - ESP32 CENTRAL (ESP-NOW HUB)");
    Serial.println("========================================================");

    // 1. Inicializa o rádio ESP-NOW no canal definido
    if (!espNowCentral.iniciar(ESPNOW_WIFI_CHANNEL)) {
        Serial.println("[CENTRAL] ERRO FATAL ao iniciar ESP-NOW Central!");
        while (true) delay(1000);
    }

    // 2. Registra os callbacks para mensagens Unicast dos periféricos
    espNowCentral.definirCallbackRecepcao(aoReceberDadosPeriferico);
    espNowCentral.definirCallbackAckConfig(aoReceberAckConfig);

    Serial.printf("[CENTRAL] MAC do Central: %s\n", espNowCentral.getMacAddressString().c_str());
    Serial.println("[CENTRAL] Enviando Broadcast a cada 3s para descoberta de nós...\n");
}

void loop() {
    // Transmissão periódica em Broadcast para acordar/anunciar presença aos periféricos
    if (millis() - ultimoBroadcastMs >= INTERVALO_BROADCAST_MS) {
        ultimoBroadcastMs = millis();
        espNowCentral.enviarBroadcastBeacon("CENTRAL_SITIO");
    }

    // Monitora keep-alive de dispositivos registrados
    static unsigned long ultimoCheckDevices = 0;
    if (millis() - ultimoCheckDevices > 10000) {
        ultimoCheckDevices = millis();
        deviceManager.checkKeepAlive(60000);
    }

    delay(20);
}
