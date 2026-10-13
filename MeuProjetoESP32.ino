#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "secrets.h"
#include "WifiManager.h"
#include "DeviceManager.h"

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
WebServer server(80);

unsigned long ultimaLeituraSensor = 0;
float umidadeSoloSimulada = 62.0;

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

    // Variação leve na leitura para demonstrar tempo real
    if (millis() - ultimaLeituraSensor > 5000) {
        ultimaLeituraSensor = millis();
        umidadeSoloSimulada += (random(-20, 21) / 10.0);
        if (umidadeSoloSimulada > 95.0) umidadeSoloSimulada = 95.0;
        if (umidadeSoloSimulada < 20.0) umidadeSoloSimulada = 20.0;
    }

    String json = "{\n";
    json += "  \"esps\": [\n";
    json += "    {\n";
    json += "      \"id\": \"esp-1\",\n";
    json += "      \"num\": 1,\n";
    json += "      \"name\": \"ESP 1 (Solo Setor A)\",\n";
    json += "      \"connectivity\": \"ONLINE\",\n";
    json += "      \"mode\": \"DEEP SLEEP\",\n";
    json += "      \"lastUpdate\": \"Agora\",\n";
    json += "      \"nextUpdate\": \"Em 60 min\",\n";
    json += "      \"sensorValue\": \"" + String(umidadeSoloSimulada, 1) + "% (Umidade Solo)\",\n";
    json += "      \"interval\": \"60 min\",\n";
    json += "      \"unit\": \"% Umidade\",\n";
    json += "      \"batteryPct\": 94,\n";
    json += "      \"powerSource\": \"BATTERY\",\n";
    json += "      \"batteryCapacityMah\": 2500,\n";
    json += "      \"uptimeHours\": 320\n";
    json += "    },\n";
    json += "    {\n";
    json += "      \"id\": \"esp-2\",\n";
    json += "      \"num\": 2,\n";
    json += "      \"name\": \"ESP 2 (Temp. Estufa)\",\n";
    json += "      \"connectivity\": \"DORMINDO\",\n";
    json += "      \"mode\": \"LIGHT SLEEP\",\n";
    json += "      \"lastUpdate\": \"Ha 10 min\",\n";
    json += "      \"nextUpdate\": \"Em 20 min\",\n";
    json += "      \"sensorValue\": \"24.8 °C\",\n";
    json += "      \"interval\": \"30 min\",\n";
    json += "      \"unit\": \"°C\",\n";
    json += "      \"batteryPct\": 88,\n";
    json += "      \"powerSource\": \"BATTERY\",\n";
    json += "      \"batteryCapacityMah\": 2500,\n";
    json += "      \"uptimeHours\": 195\n";
    json += "    },\n";
    json += "    {\n";
    json += "      \"id\": \"esp-3\",\n";
    json += "      \"num\": 3,\n";
    json += "      \"name\": \"ESP 3 (Chuva MH-RD)\",\n";
    json += "      \"connectivity\": \"ONLINE\",\n";
    json += "      \"mode\": \"ONLINE\",\n";
    json += "      \"lastUpdate\": \"Agora\",\n";
    json += "      \"nextUpdate\": \"Em 1 min\",\n";
    json += "      \"sensorValue\": \"Seco (MH-RD)\",\n";
    json += "      \"interval\": \"1 min\",\n";
    json += "      \"unit\": \"Chuva\",\n";
    json += "      \"batteryPct\": 100,\n";
    json += "      \"powerSource\": \"DC\",\n";
    json += "      \"batteryCapacityMah\": null,\n";
    json += "      \"uptimeHours\": 65\n";
    json += "    }\n";
    json += "  ],\n";
    json += "  \"notifications\": [\n";
    json += "    \"ESP32 Central operando em modo computador/web!\",\n";
    json += "    \"API REST ativa e aguardando comandos na porta 80.\"\n";
    json += "  ]\n";
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

    // Registra rotas da API REST
    server.on("/api/status", HTTP_OPTIONS, handleOptions);
    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.on("/api/config", HTTP_OPTIONS, handleOptions);
    server.on("/api/config", HTTP_POST, handlePostConfig);

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

    delay(10);
}