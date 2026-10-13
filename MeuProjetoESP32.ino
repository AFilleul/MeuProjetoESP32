#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "secrets.h"
#include "WifiManager.h"
#include "DeviceManager.h"
#include "EspNowCentral.h"

// ============================================================================
// SÍTIO ONLINE - SISTEMA DE MONITORAMENTO AGRÍCOLA ESP32
// VERSÃO: CONTROLE VIA COMPUTADOR / INTERFACE WEB (SEM TECLADO 4X4)
//
// NOTA: A versão com teclado matricial 4x4 físico está preservada na branch:
//       'com-teclado-4x4' no repositório GitHub.
// Nesta branch principal ('main'), os pinos GPIO estão liberados para sensores
// e toda a operação é realizada via comandos do computador (Interface Web REST).
// ============================================================================

WifiManager wifiManager(WIFI_SSID, WIFI_PASSWORD);
DeviceManager deviceManager(30000); // 30 KB de margem de segurança de heap
EspNowCentral espNowCentral;
WebServer server(80);

const unsigned long CICLO_RECEPCAO_MS = 60000; // Ciclo de 60 segundos
const unsigned long DURACAO_JANELA_MS = 10000; // 10 segundos reservados para recepção de periféricos
unsigned long ultimoBroadcastBeaconMs = 0;

// ============================================================================
// ROTAS DA REST API (PARA INTERFACE WEB NO COMPUTADOR)
// ============================================================================

void adicionarCORS() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleOptions() {
    adicionarCORS();
    server.send(204);
}

void handleGetStatus() {
    adicionarCORS();

    // Cronograma da janela agendada de recepção de periféricos
    unsigned long cicloAtualMs = millis() % CICLO_RECEPCAO_MS;
    bool emJanelaRecepcao = (cicloAtualMs < DURACAO_JANELA_MS);
    unsigned long segundosAteJanela = emJanelaRecepcao ? 0 : ((CICLO_RECEPCAO_MS - cicloAtualMs) / 1000);

    String json = "{\n";
    json += "  \"esps\": [\n";

    size_t count = deviceManager.getDeviceCount();
    std::vector<String> offlineDevs;

    for (size_t i = 0; i < count; i++) {
        const Device* dev = deviceManager.getDeviceByIndex(i);
        if (!dev) continue;

        if (dev->connectivity == "OFFLINE") {
            offlineDevs.push_back(dev->name);
        }

        String nextUp = (dev->connectivity == "OFFLINE") ? "⚠️ Atrasado (OFFLINE)" : ("Em " + dev->interval);
        unsigned long segAtras = (millis() - dev->lastSeen) / 1000;
        String lastUp = (segAtras < 5) ? "Agora" : (String(segAtras) + "s atrás");

        json += "    {\n";
        json += "      \"id\": \"" + dev->id + "\",\n";
        json += "      \"num\": " + String(i + 1) + ",\n";
        json += "      \"name\": \"" + dev->name + "\",\n";
        json += "      \"connectivity\": \"" + dev->connectivity + "\",\n";
        json += "      \"mode\": \"" + dev->mode + "\",\n";
        json += "      \"lastUpdate\": \"" + lastUp + "\",\n";
        json += "      \"nextUpdate\": \"" + nextUp + "\",\n";
        json += "      \"sensorValue\": \"" + dev->sensorValue + "\",\n";
        json += "      \"interval\": \"" + dev->interval + "\",\n";
        json += "      \"unit\": \"" + dev->unit + "\",\n";
        json += "      \"batteryPct\": " + String(dev->batteryPct) + ",\n";
        json += "      \"powerSource\": \"" + dev->powerSource + "\",\n";
        json += "      \"batteryCapacityMah\": " + String(dev->batteryCapacityMah) + ",\n";
        json += "      \"uptimeHours\": " + String(dev->uptimeHours) + "\n";
        json += (i < count - 1) ? "    },\n" : "    }\n";
    }

    json += "  ],\n";
    json += "  \"notifications\": [\n";
    if (count == 0) {
        json += "    \"⚠️ Nenhum ESP periférico conectado ao Central no momento.\",\n";
        json += "    \"ℹ️ Aguardando comunicação dos microcontroladores periféricos via ESP-NOW...\"\n";
    } else {
        bool hasOffline = !offlineDevs.empty();
        if (hasOffline) {
            for (const auto& devName : offlineDevs) {
                json += "    \"⚠️ ALERTA: " + devName + " não enviou sinal no horário previsto e está OFFLINE!\",\n";
            }
        }
        json += "    \"ESP32 Central operando em modo computador/web!\",\n";
        json += "    \"" + String(count) + " dispositivo(s) cadastrado(s) no sistema.\"\n";
    }
    json += "  ],\n";
    json += "  \"receptionSchedule\": {\n";
    json += "    \"inReceptionWindow\": " + String(emJanelaRecepcao ? "true" : "false") + ",\n";
    json += "    \"nextWindowSeconds\": " + String(segundosAteJanela) + ",\n";
    json += "    \"windowDurationSeconds\": " + String(DURACAO_JANELA_MS / 1000) + ",\n";
    json += "    \"cycleIntervalSeconds\": " + String(CICLO_RECEPCAO_MS / 1000) + ",\n";
    json += "    \"statusMessage\": \"" + String(emJanelaRecepcao ? "Janela de recepção ativa: Central recebendo pacotes ESP-NOW" : "Aguardando próxima janela agendada") + "\"\n";
    json += "  }\n";
    json += "}";

    server.send(200, "application/json", json);
}

void handlePostConfig() {
    adicionarCORS();
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        Serial.println("\n[API REST] Nova configuracao recebida da interface web:");
        Serial.println(body);
        server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuracao gravada com sucesso!\"}");
    } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Requisicao sem conteudo\"}");
    }
}

void handleDeleteDevice() {
    adicionarCORS();
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        Serial.println("\n[API REST] Requisicao para apagar periferico:");
        Serial.println(body);

        int idPos = body.indexOf("\"id\":");
        if (idPos != -1) {
            int startQuote = body.indexOf("\"", idPos + 5);
            int endQuote = body.indexOf("\"", startQuote + 1);
            if (startQuote != -1 && endQuote != -1) {
                String devId = body.substring(startQuote + 1, endQuote);
                bool removed = deviceManager.removeDevice(devId);
                if (removed) {
                    Serial.printf("[API REST] Periférico '%s' apagado com sucesso!\n", devId.c_str());
                    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Periferico apagado com sucesso!\"}");
                    return;
                }
            }
        }
        server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Comando processado\"}");
    } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Requisicao sem conteudo\"}");
    }
}

// ============================================================================
// INICIALIZAÇÃO (SETUP)
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================================");
    Serial.println("  SITIO ONLINE - MONITORAMENTO AGRICOLA ESP32");
    Serial.println("  MODO: CONTROLE EXCLUSIVO VIA COMPUTADOR (REST API)");
    Serial.println("========================================================");

    // Conexão Wi-Fi usando WifiManager
    Serial.println("[Wi-Fi] Iniciando conexao com a rede...");
    bool wifiOk = wifiManager.connect(15000); // 15 segundos de timeout

    if (wifiOk) {
        Serial.println("\n********************************************************");
        Serial.println("  SUCESSO: ESP32 CONECTADO AO WI-FI!");
        Serial.print("  >>> SEU ENDERECO IP: ");
        Serial.println(wifiManager.getLocalIP());
        Serial.print("  >>> COPIE ESTE IP E COLE NO BOTAO '⚙️ Conexao API': ");
        Serial.print("http://");
        Serial.println(wifiManager.getLocalIP());
        Serial.println("********************************************************\n");
    } else {
        // Fallback: se o roteador não responder, cria ponto de acesso próprio
        Serial.println("\n[Wi-Fi] Roteador nao encontrado. Criando Ponto de Acesso (AP)...");
        WiFi.softAP("SitioOnline_ESP32", "12345678");
        Serial.println("********************************************************");
        Serial.println("  MODO ACCESS POINT ATIVADO!");
        Serial.println("  Rede Wi-Fi: SitioOnline_ESP32  |  Senha: 12345678");
        Serial.print("  >>> ENDERECO IP: http://");
        Serial.println(WiFi.softAPIP()); // 192.168.4.1
        Serial.println("********************************************************\n");
    }

    // Inicializa o ESP-NOW para recepção de telemetria dos nós periféricos
    espNowCentral.iniciar(ESPNOW_WIFI_CHANNEL, false);
    espNowCentral.definirCallbackRecepcao([](const uint8_t* mac, const PacoteDadosSensores& dados) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        Serial.printf("[CENTRAL] Dados recebidos do nó periférico: %s (%s) | Umidade: %.1f%% | Bateria: %d%%\n", 
                      dados.nodeName, dados.nodeId, dados.sensorUmidade, dados.bateriaPct);
        if (!deviceManager.hasDevice(dados.nodeId)) {
            deviceManager.registerDevice(dados.nodeId, dados.nodeName, macStr);
        }
        deviceManager.updateTelemetry(dados.nodeId, dados.sensorUmidade, dados.bateriaPct, dados.modoOperacao, dados.tempoUptimeSegundos);
    });

    // Registra rotas da API REST
    server.on("/api/status", HTTP_OPTIONS, handleOptions);
    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.on("/api/config", HTTP_OPTIONS, handleOptions);
    server.on("/api/config", HTTP_POST, handlePostConfig);
    server.on("/api/device/delete", HTTP_OPTIONS, handleOptions);
    server.on("/api/device/delete", HTTP_POST, handleDeleteDevice);

    server.begin();
    Serial.println("[Servidor HTTP] API REST pronta na porta 80.");
    Serial.println("Todos os pinos estao liberados para sensores!\n");
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
    // 1. Processa requisicoes HTTP vindas da interface web no computador
    server.handleClient();

    // 2. Reconexão automática ao Wi-Fi se houver perda de sinal
    wifiManager.handleReconnect();

    // 3. Janela agendada de recepção de periféricos (Beacon a cada 2s durante a janela)
    unsigned long cicloAtualMs = millis() % CICLO_RECEPCAO_MS;
    bool emJanelaRecepcao = (cicloAtualMs < DURACAO_JANELA_MS);
    if (emJanelaRecepcao) {
        if (millis() - ultimoBroadcastBeaconMs >= 2000) {
            ultimoBroadcastBeaconMs = millis();
            espNowCentral.enviarBroadcastBeacon("CENTRAL_SITIO");
        }
    }

    // 4. Manutenção de keep-alive e detecção de periféricos que não enviaram sinal quando deviam
    static unsigned long ultimoCheckDevices = 0;
    if (millis() - ultimoCheckDevices > 2000) {
        ultimoCheckDevices = millis();
        deviceManager.checkScheduleTimeout(30000); // 30s de tolerância após o horário previsto
    }

    delay(10);
}