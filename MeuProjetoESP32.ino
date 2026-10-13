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
        std::vector<String> dynamicNotifs = deviceManager.getAndClearNotifications();
        for (const auto& dn : dynamicNotifs) {
            json += "    \"" + dn + "\",\n";
        }
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

        // Parsing do payload JSON enviado pela interface web: {"espId": "esp-2", "field": "interval", "value": 30}
        String espId = "";
        String field = "";
        String valueStr = "";

        int idPos = body.indexOf("\"espId\":");
        if (idPos != -1) {
            int q1 = body.indexOf("\"", idPos + 8);
            int q2 = body.indexOf("\"", q1 + 1);
            if (q1 != -1 && q2 != -1) espId = body.substring(q1 + 1, q2);
        }

        int fieldPos = body.indexOf("\"field\":");
        if (fieldPos != -1) {
            int q1 = body.indexOf("\"", fieldPos + 8);
            int q2 = body.indexOf("\"", q1 + 1);
            if (q1 != -1 && q2 != -1) field = body.substring(q1 + 1, q2);
        }

        int valPos = body.indexOf("\"value\":");
        if (valPos != -1) {
            int startVal = valPos + 8;
            while (startVal < body.length() && (body[startVal] == ' ' || body[startVal] == ':')) startVal++;
            if (body[startVal] == '\"') {
                int endVal = body.indexOf("\"", startVal + 1);
                if (endVal != -1) valueStr = body.substring(startVal + 1, endVal);
            } else {
                int endVal = startVal;
                while (endVal < body.length() && body[endVal] != ',' && body[endVal] != '}') endVal++;
                valueStr = body.substring(startVal, endVal);
                valueStr.trim();
            }
        }

        if (espId.length() > 0 && field.length() > 0) {
            Device* dev = deviceManager.getDevice(espId);
            if (!dev) {
                deviceManager.registerDevice(espId, espId);
                dev = deviceManager.getDevice(espId);
            }

            uint16_t novoIntervalo = 0;
            String novoModo = "";
            String novoNome = "";

            if (field == "interval") {
                novoIntervalo = (uint16_t)valueStr.toInt();
            } else if (field == "mode") {
                novoModo = valueStr;
            } else if (field == "name") {
                novoNome = valueStr;
            }

            // Enfileira a configuração no registro do DeviceManager
            deviceManager.queueConfig(espId, novoIntervalo, novoModo, novoNome);

            // Se o MAC já foi capturado, despacha imediatamente via ESP-NOW
            PacoteConfiguracao cfgEnvio;
            uint8_t macDest[6];
            if (dev && dev->hasMac) {
                if (deviceManager.getPendingConfig(espId, cfgEnvio, macDest)) {
                    Serial.printf("[API REST] Despachando configuração para '%s' via ESP-NOW imediatamente...\n", espId.c_str());
                    bool enviado = espNowCentral.enviarConfiguracao(macDest, cfgEnvio);
                    if (enviado) {
                        Serial.println("[API REST] Pacote ESP-NOW transmitido via rádio.");
                    }
                    server.send(200, "application/json", "{\"status\":\"sent\",\"message\":\"Configuracao despachada diretamente via ESP-NOW\"}");
                    return;
                }
            }

            Serial.printf("[API REST] Periférico '%s' aguardando sinal. Mantida na fila do ESP Central para envio quando o nó acordar.\n", espId.c_str());
            server.send(200, "application/json", "{\"status\":\"queued\",\"message\":\"Configuracao armazenada na fila de sincronizacao\"}");
            return;
        }

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
    
    // Callback 1: Recepção de telemetria dos sensores e despacho imediato de filas pendentes
    espNowCentral.definirCallbackRecepcao([](const uint8_t* mac, const PacoteDadosSensores& dados) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        Serial.printf("\n[CENTRAL] Telemetria recebida do nó: %s (%s) | Umidade: %.1f%% | Bateria: %d%%\n", 
                      dados.nodeName, dados.nodeId, dados.sensorUmidade, dados.bateriaPct);
        
        if (!deviceManager.hasDevice(dados.nodeId)) {
            deviceManager.registerDevice(dados.nodeId, dados.nodeName, macStr);
        }
        deviceManager.setDeviceMac(dados.nodeId, mac);
        deviceManager.updateTelemetry(dados.nodeId, dados.sensorUmidade, dados.bateriaPct, dados.modoOperacao, dados.tempoUptimeSegundos);

        // FILA DE SINCRONIZAÇÃO: Se houver alteração pendente enviada pela Interface Web, despacha agora!
        PacoteConfiguracao cfgPendente;
        uint8_t macDest[6];
        if (deviceManager.getPendingConfig(dados.nodeId, cfgPendente, macDest)) {
            Serial.printf("[CENTRAL] Nó '%s' acordou! Despachando configuração da fila de sincronização via ESP-NOW...\n", dados.nodeId);
            espNowCentral.enviarConfiguracao(mac, cfgPendente);
        }
    });

    // Callback 2: Confirmação (ACK) de configuração aplicada com sucesso no periférico
    espNowCentral.definirCallbackAckConfig([](const uint8_t* mac, const PacoteAckConfig& ack) {
        Serial.printf("\n[CENTRAL] ACK de configuração confirmado pelo periférico '%s'! Status: %s\n",
                      ack.nodeId, ack.sucesso ? "SUCESSO" : "ERRO");
        if (ack.sucesso) {
            Device* dev = deviceManager.getDevice(ack.nodeId);
            if (dev) {
                if (ack.intervaloAplicadoMin > 0) {
                    deviceManager.setInterval(ack.nodeId, String(ack.intervaloAplicadoMin) + " min");
                }
                if (strlen(ack.modoAplicado) > 0) {
                    dev->mode = ack.modoAplicado;
                }
            }
            deviceManager.clearPendingConfig(ack.nodeId);
            deviceManager.addNotification(String("✓ Configuração sincronizada com sucesso no nó '") + ack.nodeId + "'!");
        }
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