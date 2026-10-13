#include <Arduino.h>
#include "EspNowCentral.h"
#include "DeviceManager.h"

// ============================================================================
// ESP32 CENTRAL - PROTOCOLO ESP-NOW (COMUNICAÇÃO DIRETA SEM ROTEADOR)
// ============================================================================
// Este sketch exemplifica o ESP32 Central operando puramente via ESP-NOW:
// 1. Transmite periodicamente Broadcast (FF:FF:FF:FF:FF:FF) anunciando o Central
// 2. Recebe telemetria Unicast dos nós periféricos via callback C++
// 3. Atualiza os dispositivos registrados no DeviceManager
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
    Serial.println("--------------------------------------------------------");

    // Registra ou atualiza no DeviceManager
    if (!deviceManager.hasDevice(dados.nodeId)) {
        deviceManager.registerDevice(dados.nodeId, dados.nodeName, macStr);
    } else {
        deviceManager.updateLastSeen(dados.nodeId);
    }
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

    // 2. Registra o callback para captura de mensagens Unicast dos periféricos
    espNowCentral.definirCallbackRecepcao(aoReceberDadosPeriferico);

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
