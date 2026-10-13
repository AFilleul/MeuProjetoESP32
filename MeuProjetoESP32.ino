#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Keypad.h>

#include "secrets.h"
#include "WifiManager.h"
#include "DeviceManager.h"
#include "TecladoMatriz.h"

// ============================================================================
// CONFIGURAÇÃO DOS PINOS DO TECLADO MATRICIAL (ESP32 Wemos Lolin)
// ============================================================================
const byte pinosLinhas[4]  = {16, 17, 5, 18};
const byte pinosColunas[4] = {27, 26, 25, 32};

TecladoMatriz teclado(pinosLinhas, pinosColunas);
WifiManager wifiManager(WIFI_SSID, WIFI_PASSWORD);
DeviceManager deviceManager(30000); // 30 KB de margem de segurança de heap
WebServer server(80);

// Última tecla registrada para sincronização com a web
char ultimaTecla = '\0';
unsigned long ultimaLeituraSensor = 0;
float umidadeSoloSimulada = 62.0;

// ============================================================================
// FUNÇÃO DE AUTO-DIAGNÓSTICO ELÉTRICO DO TECLADO
// ============================================================================
void autoDiagnosticoPinos() {
    Serial.println("\n--- [AUTO-DIAGNOSTICO ELETRICO DO TECLADO] ---");

    bool curtoGnd = false;
    for (int i = 0; i < 4; i++) {
        pinMode(pinosLinhas[i], INPUT_PULLUP);
        delayMicroseconds(50);
        if (digitalRead(pinosLinhas[i]) == LOW) {
            Serial.print("  [GND] Linha "); Serial.print(i);
            Serial.print(" (GPIO "); Serial.print(pinosLinhas[i]);
            Serial.println("): Curto com GND!");
            curtoGnd = true;
        }
    }
    for (int j = 0; j < 4; j++) {
        pinMode(pinosColunas[j], INPUT_PULLUP);
        delayMicroseconds(50);
        if (digitalRead(pinosColunas[j]) == LOW) {
            Serial.print("  [GND] Coluna "); Serial.print(j);
            Serial.print(" (GPIO "); Serial.print(pinosColunas[j]);
            Serial.println("): Curto com GND!");
            curtoGnd = true;
        }
    }
    if (!curtoGnd) {
        Serial.println("  [Fase 1] OK: Nenhum pino isolado esta preso em GND.");
    }

    Serial.println("  [Fase 2] Testando cruzamentos (Linhas x Colunas)...");
    bool contatoEncontrado = false;
    const char mapaTeclas[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    for (int r = 0; r < 4; r++) {
        pinMode(pinosLinhas[r], INPUT_PULLUP);
    }

    for (int c = 0; c < 4; c++) {
        for (int other = 0; other < 4; other++) {
            pinMode(pinosColunas[other], INPUT_PULLUP);
        }

        pinMode(pinosColunas[c], OUTPUT);
        digitalWrite(pinosColunas[c], LOW);
        delayMicroseconds(100);

        for (int r = 0; r < 4; r++) {
            if (digitalRead(pinosLinhas[r]) == LOW) {
                Serial.print("  >>> CONTATO FECHADO: Linha ");
                Serial.print(r);
                Serial.print(" (GPIO ");
                Serial.print(pinosLinhas[r]);
                Serial.print(") <---> Coluna ");
                Serial.print(c);
                Serial.print(" (GPIO ");
                Serial.print(pinosColunas[c]);
                Serial.print(") = Tecla '");
                Serial.print(mapaTeclas[r][c]);
                Serial.println("' travada!");
                contatoEncontrado = true;
            }
        }
    }

    for (int c = 0; c < 4; c++) {
        pinMode(pinosColunas[c], INPUT_PULLUP);
    }

    if (contatoEncontrado) {
        Serial.println("----------------------------------------------------------------");
        Serial.println("ALERTA: Tecla fisica travada ou curto na protoboard detectado!");
        Serial.println("----------------------------------------------------------------\n");
    } else {
        Serial.println("  [Fase 2] OK: Nenhum cruzamento/tecla travada detectada.\n");
    }
}

// ============================================================================
// ROTAS DA REST API (PARA INTERFACE WEB)
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

    // Variação leve na umidade para demonstrar tempo real
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
    if (ultimaTecla != '\0') {
        json += "    \"Ultima tecla pressionada no teclado fisico: '" + String(ultimaTecla) + "'\",\n";
    }
    json += "    \"ESP32 Central operando normalmente na porta 80!\"\n";
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
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================================");
    Serial.println("  SITIO ONLINE - MONITORAMENTO AGRICOLA ESP32");
    Serial.println("========================================================");

    // 1. Diagnóstico elétrico do teclado de membrana
    autoDiagnosticoPinos();
    teclado.iniciar();

    // 2. Conexão Wi-Fi
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

    // 3. Registra rotas da API REST
    server.on("/api/status", HTTP_OPTIONS, handleOptions);
    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.on("/api/config", HTTP_OPTIONS, handleOptions);
    server.on("/api/config", HTTP_POST, handlePostConfig);

    server.begin();
    Serial.println("[Servidor HTTP] API REST ativa na porta 80.");
    Serial.println("Aguardando conexoes da interface web ou teclado 4x4...\n");
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
    // 1. Atende requisições HTTP da interface web
    server.handleClient();

    // 2. Garante reconexão automática ao Wi-Fi se o sinal cair
    wifiManager.handleReconnect();

    // 3. Lê teclas do teclado de membrana 4x4
    char tecla = teclado.lerTecla();
    if (tecla != '\0') {
        ultimaTecla = tecla;
        Serial.print("[Teclado 4x4] Tecla pressionada: ");
        Serial.println(tecla);
    }

    delay(10);
}