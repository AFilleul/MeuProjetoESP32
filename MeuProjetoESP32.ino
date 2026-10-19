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
        json += "      \"mode\": \"" + DeviceManager::normalizeModeForDisplay(dev->mode) + "\",\n";
        json += "      \"lastUpdate\": \"" + lastUp + "\",\n";
        json += "      \"nextUpdate\": \"" + nextUp + "\",\n";
        json += "      \"sensorValue\": \"" + dev->sensorValue + "\",\n";
        json += "      \"interval\": \"" + dev->interval + "\",\n";
        json += "      \"unit\": \"" + dev->unit + "\",\n";
        json += "      \"batteryPct\": " + String(dev->batteryPct) + ",\n";
        json += "      \"powerSource\": \"" + dev->powerSource + "\",\n";
        json += "      \"batteryCapacityMah\": " + String(dev->batteryCapacityMah) + ",\n";
        json += "      \"uptimeHours\": " + String(dev->uptimeHours) + ",\n";
        json += "      \"hasPendingConfig\": " + String(dev->hasPendingConfig ? "true" : "false") + "\n";
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

// Função auxiliar para extrair campo JSON simples
static String extractJsonStringField(const String& body, const char* key) {
    String searchKey = String("\"") + key + "\":";
    int pos = body.indexOf(searchKey);
    if (pos == -1) return "";
    int startVal = pos + searchKey.length();
    while (startVal < (int)body.length() && (body[startVal] == ' ' || body[startVal] == ':')) startVal++;
    if (startVal >= (int)body.length()) return "";

    if (body[startVal] == '\"') {
        int endVal = body.indexOf("\"", startVal + 1);
        if (endVal != -1) return body.substring(startVal + 1, endVal);
    } else if (body[startVal] != '{') {
        int endVal = startVal;
        while (endVal < (int)body.length() && body[endVal] != ',' && body[endVal] != '}') endVal++;
        String val = body.substring(startVal, endVal);
        val.trim();
        return val;
    }
    return "";
}

void handlePostConfig() {
    adicionarCORS();
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        Serial.println("\n[API REST] Nova configuracao prioritaria recebida da interface web:");
        Serial.println(body);

        String espId = extractJsonStringField(body, "espId");
        String field = extractJsonStringField(body, "field");
        String valueStr = extractJsonStringField(body, "value");

        if (espId.length() > 0 && field.length() > 0) {
            Device* dev = deviceManager.getDevice(espId);
            if (!dev) {
                deviceManager.registerDevice(espId, espId);
                dev = deviceManager.getDevice(espId);
            }

            // Campos gerenciados no Central para exibição e cálculo de autonomia
            if (field == "unit") {
                deviceManager.setUnit(espId, valueStr);
                server.send(200, "application/json", "{\"status\":\"sent\",\"message\":\"Unidade atualizada e fixada no Central\"}");
                return;
            } else if (field == "powerSource") {
                String pType = extractJsonStringField(body, "type");
                if (pType.length() == 0) pType = valueStr;
                String capStr = extractJsonStringField(body, "capacity");
                uint32_t capMah = (uint32_t)capStr.toInt();
                deviceManager.setPowerConfig(espId, pType, capMah);
                server.send(200, "application/json", "{\"status\":\"sent\",\"message\":\"Fonte de energia atualizada e fixada no Central\"}");
                return;
            } else if (field == "batteryCapacity") {
                uint32_t capMah = (uint32_t)valueStr.toInt();
                deviceManager.setPowerConfig(espId, "BATTERY", capMah);
                server.send(200, "application/json", "{\"status\":\"sent\",\"message\":\"Capacidade de bateria atualizada no Central\"}");
                return;
            }

            // Campos sincronizados via rádio ESP-NOW com o periférico (interval, mode, name)
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

            // Registra e bloqueia a configuração com prioridade sobre qualquer telemetria futura
            deviceManager.queueConfig(espId, novoIntervalo, novoModo, novoNome);

            // Se o MAC já foi capturado, despacha imediatamente via ESP-NOW
            PacoteConfiguracao cfgEnvio;
            uint8_t macDest[6];
            if (dev && dev->hasMac) {
                if (deviceManager.getPendingConfig(espId, cfgEnvio, macDest)) {
                    Serial.printf("[API REST] PRIORIDADE: Despachando configuração para '%s' via ESP-NOW imediatamente...\n", espId.c_str());
                    bool enviado = espNowCentral.enviarConfiguracao(macDest, cfgEnvio);
                    dev->lastConfigSendMs = millis();
                    if (enviado) {
                        Serial.println("[API REST] Pacote ESP-NOW transmitido via rádio (aguardando ACK do periférico).");
                    }
                    server.send(200, "application/json", "{\"status\":\"sent\",\"message\":\"Configuracao despachada diretamente via ESP-NOW\"}");
                    return;
                }
            }

            Serial.printf("[API REST] Periférico '%s' em repouso/aguardando sinal. Mantida na fila prioritária do ESP Central.\n", espId.c_str());
            server.send(200, "application/json", "{\"status\":\"queued\",\"message\":\"Configuracao armazenada na fila prioritaria de sincronizacao\"}");
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

        String devId = extractJsonStringField(body, "id");
        if (devId.length() > 0) {
            bool removed = deviceManager.removeDevice(devId);
            if (removed) {
                Serial.printf("[API REST] Periférico '%s' apagado com sucesso!\n", devId.c_str());
                server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Periferico apagado com sucesso!\"}");
                return;
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
        // Fallback: se o roteador não responder, cria ponto de acesso próprio em modo AP+STA no canal do ESP-NOW
        Serial.println("\n[Wi-Fi] Roteador nao encontrado. Criando Ponto de Acesso (AP+STA)...");
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("SitioOnline_ESP32", "12345678", ESPNOW_WIFI_CHANNEL);
        Serial.println("********************************************************");
        Serial.println("  MODO ACCESS POINT ATIVADO!");
        Serial.println("  Rede Wi-Fi: SitioOnline_ESP32  |  Senha: 12345678");
        Serial.print("  >>> ENDERECO IP: http://");
        Serial.println(WiFi.softAPIP()); // 192.168.4.1
        Serial.println("********************************************************\n");
    }

    // Inicializa o ESP-NOW para recepção de telemetria e envio prioritário de configurações
    espNowCentral.iniciar(ESPNOW_WIFI_CHANNEL, false);
    
    // Callback 1: Recepção de telemetria dos sensores com PRIORIDADE ABSOLUTA para envio de configurações
    espNowCentral.definirCallbackRecepcao([](const uint8_t* mac, const PacoteDadosSensores& dados) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        Serial.printf("\n[CENTRAL] Telemetria recebida do nó: %s (%s) | Valor: %.1f | Bateria: %d%%\n", 
                      dados.nodeName, dados.nodeId, dados.sensorUmidade, dados.bateriaPct);
        
        if (!deviceManager.hasDevice(dados.nodeId)) {
            deviceManager.registerDevice(dados.nodeId, dados.nodeName, macStr);
        }
        deviceManager.setDeviceMac(dados.nodeId, mac);

        // Atualiza telemetria protegendo todas as configurações definidas pelo usuário
        // (e reativa hasPendingConfig automaticamente caso o periférico ainda esteja com configuração antiga!)
        deviceManager.updateTelemetry(dados.nodeId, dados.sensorUmidade, dados.bateriaPct, dados.modoOperacao, dados.tempoUptimeSegundos, dados.nodeName);

        // PRIORIDADE 1: Se houver configuração pendente ou divergente, despacha IMEDIATAMENTE para o periférico!
        PacoteConfiguracao cfgPendente;
        uint8_t macDest[6];
        if (deviceManager.getPendingConfig(dados.nodeId, cfgPendente, macDest)) {
            Serial.printf("[CENTRAL] PRIORIDADE: Despachando configuração para '%s' via ESP-NOW imediatamente...\n", dados.nodeId);
            espNowCentral.enviarConfiguracao(mac, cfgPendente);
            Device* dev = deviceManager.getDevice(dados.nodeId);
            if (dev) dev->lastConfigSendMs = millis();
        }
    });

    // Callback 2: Confirmação (ACK) de configuração aplicada com sucesso no periférico
    espNowCentral.definirCallbackAckConfig([](const uint8_t* mac, const PacoteAckConfig& ack) {
        Serial.printf("\n[CENTRAL] ACK de configuração confirmado pelo periférico '%s'! Status: %s\n",
                       ack.nodeId, ack.sucesso ? "SUCESSO" : "ERRO");
        if (ack.sucesso) {
            Device* dev = deviceManager.getDevice(ack.nodeId);
            if (dev) {
                if (ack.intervaloAplicadoMin > 0 && !dev->hasUserInterval) {
                    deviceManager.setInterval(ack.nodeId, String(ack.intervaloAplicadoMin) + " min");
                }
                if (strlen(ack.modoAplicado) > 0 && !dev->hasUserMode) {
                    dev->mode = DeviceManager::normalizeModeForDisplay(ack.modoAplicado);
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

    // 2. PRIORIDADE: Reenvio ativo de configurações pendentes para periféricos que estão acordados/online
    static unsigned long ultimoRetryConfigMs = 0;
    if (millis() - ultimoRetryConfigMs >= 250) {
        ultimoRetryConfigMs = millis();
        size_t count = deviceManager.getDeviceCount();
        for (size_t i = 0; i < count; i++) {
            Device* dev = deviceManager.getDeviceByIndex(i);
            if (dev && dev->hasPendingConfig && dev->hasMac) {
                // Se o periférico deu sinal nos últimos 15 segundos (está acordado/escutando)
                // e já passaram 450ms desde a última tentativa de envio, reenvia o pacote até receber o ACK!
                if ((millis() - dev->lastSeen < 15000) && (millis() - dev->lastConfigSendMs >= 450)) {
                    dev->lastConfigSendMs = millis();
                    Serial.printf("[CENTRAL] Reenviando configuração prioritária para '%s' (aguardando ACK)...\n", dev->id.c_str());
                    espNowCentral.enviarConfiguracao(dev->macAddr, dev->pendingConfig);
                }
            }
        }
    }

    // 3. Emissão contínua de Broadcast Beacon (a cada 1.5s) para descoberta rápida de periféricos ao acordarem
    if (millis() - ultimoBroadcastBeaconMs >= 1500) {
        ultimoBroadcastBeaconMs = millis();
        espNowCentral.enviarBroadcastBeacon("CENTRAL_SITIO");
    }

    // 4. Reconexão automática ao Wi-Fi se houver perda de sinal
    wifiManager.handleReconnect();

    // 5. Manutenção de keep-alive e detecção de periféricos que não enviaram sinal quando deviam
    static unsigned long ultimoCheckDevices = 0;
    if (millis() - ultimoCheckDevices > 2000) {
        ultimoCheckDevices = millis();
        deviceManager.checkScheduleTimeout(30000); // 30s de tolerância após o horário previsto
    }

    delay(10);
}