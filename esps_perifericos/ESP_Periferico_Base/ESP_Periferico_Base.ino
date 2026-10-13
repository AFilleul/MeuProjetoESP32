#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "config_periferico.h"

// Credenciais Wi-Fi (da rede local ou do AP do ESP Central)
const char* ssid = "SitioOnline_ESP32"; // ou sua rede Wi-Fi convencional
const char* password = "12345678";

// RTC Memory: variáveis mantidas mesmo durante o Deep Sleep
RTC_DATA_ATTR int contagemLeituras = 0;
RTC_DATA_ATTR float ultimaLeitura = 0.0;

// Leitura analógica calibrada (0 a 4095 convertido para 0 a 100%)
float lerSensorUmidade() {
    #ifdef PIN_SENSOR_VCC
    pinMode(PIN_SENSOR_VCC, OUTPUT);
    digitalWrite(PIN_SENSOR_VCC, HIGH); // Liga o sensor apenas para leitura (economiza bateria)
    delay(50);
    #endif

    int leituraRaw = analogRead(PIN_SENSOR_ANALOG);

    #ifdef PIN_SENSOR_VCC
    digitalWrite(PIN_SENSOR_VCC, LOW); // Desliga alimentação do sensor
    #endif

    // Exemplo de calibração: 4095 (totalmente seco) -> 1200 (totalmente úmido)
    float porcentagem = map(leituraRaw, 4095, 1200, 0, 100);
    porcentagem = constrain(porcentagem, 0.0, 100.0);
    return porcentagem;
}

// Envia os dados para o ESP32 Central via REST HTTP POST
bool enviarDadosAoCentral(float valorSensor) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = "http://" + String(CENTRAL_SERVER_HOST) + ":" + String(CENTRAL_SERVER_PORT) + "/api/data";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String json = "{\n";
    json += "  \"id\": \"" + String(NODE_ID) + "\",\n";
    json += "  \"name\": \"" + String(NODE_NAME) + "\",\n";
    json += "  \"sensorType\": \"" + String(SENSOR_TYPE) + "\",\n";
    json += "  \"sensorValue\": \"" + String(valorSensor, 1) + " " + String(SENSOR_UNIT) + "\",\n";
    json += "  \"mode\": \"" + String(OPERATING_MODE) + "\",\n";
    json += "  \"powerSource\": \"" + String(POWER_SOURCE_TYPE) + "\",\n";
    json += "  \"batteryCapacityMah\": " + String(BATTERY_CAPACITY_MAH) + ",\n";
    json += "  \"uptimeCycle\": " + String(contagemLeituras) + "\n";
    json += "}";

    Serial.println("[Periferico] Enviando payload ao ESP Central:");
    Serial.println(json);

    int httpCode = http.POST(json);
    bool sucesso = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);

    if (sucesso) {
        Serial.println("[Periferico] Dados enviados e aceitos pelo Central!");
    } else {
        Serial.printf("[Periferico] Falha no envio HTTP: %d\n", httpCode);
    }

    http.end();
    return sucesso;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    contagemLeituras++;
    Serial.printf("\n--- Nó Periférico: %s (Ciclo #%d) ---\n", NODE_ID, contagemLeituras);

    // 1. Faz a leitura do sensor
    float valorUmidade = lerSensorUmidade();
    ultimaLeitura = valorUmidade;
    Serial.printf("[Sensor] Leitura: %.1f %s\n", valorUmidade, SENSOR_UNIT);

    // 2. Conecta ao Wi-Fi rapidamente
    Serial.print("[Wi-Fi] Conectando a rede...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < 10000) {
        delay(200);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" Conectado!");
        Serial.print("[Wi-Fi] IP local: ");
        Serial.println(WiFi.localIP());

        // 3. Envia os dados ao central
        enviarDadosAoCentral(valorUmidade);
    } else {
        Serial.println("\n[Wi-Fi] Timeout ao conectar. Prosseguindo...");
    }

    // 4. Entra em modo de economia de energia
    String modo = OPERATING_MODE;
    if (modo == "DEEP_SLEEP") {
        uint64_t tempoSonoMicros = (uint64_t)READ_INTERVAL_MIN * 60ULL * 1000000ULL;
        Serial.printf("[Energia] Entrando em Deep Sleep por %d minutos...\n", READ_INTERVAL_MIN);
        esp_sleep_enable_timer_wakeup(tempoSonoMicros);
        esp_deep_sleep_start();
    } else {
        Serial.println("[Energia] Modo Online/Contínuo ativo.");
    }
}

void loop() {
    // Se o modo for ONLINE, executa periodicamente sem Deep Sleep
    static unsigned long ultimoEnvio = 0;
    if (millis() - ultimoEnvio > (unsigned long)READ_INTERVAL_MIN * 60000UL) {
        ultimoEnvio = millis();
        float valor = lerSensorUmidade();
        enviarDadosAoCentral(valor);
    }
    delay(100);
}
