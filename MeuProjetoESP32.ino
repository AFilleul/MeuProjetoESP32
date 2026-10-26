#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "secrets.h"
#include "WifiManager.h"
#include "DeviceManager.h"
#include "EspNowCentral.h"

// Fallback caso o arquivo secrets.h local ainda nao contenha as constantes do Mini PC
#ifndef MINI_PC_CONFIGURED
const char* const MINI_PC_IP = "192.168.1.100";
const uint16_t MINI_PC_PORT = 8000;
#endif

// ============================================================================
// SÍTIO ONLINE - SISTEMA DE MONITORAMENTO AGRÍCOLA ESP32
// VERSÃO: ARQUITETURA OFFLINE COM MINI PC (CLIENTE PUSH HTTP + ESP-NOW)
//
// Fluxo desta ramificação ('com-mini-pc'):
// 1. O ESP32 Central atua exclusivamente na coordenação do rádio ESP-NOW e como
//    cliente HTTP ativo (Push), sem servidor Web embarcado.
// 2. Ao receber MSG_DADOS_SENSORES via ESP-NOW, monta o payload JSON e dispara
//    um HTTP POST ativo para http://<MINI_PC_IP>:<MINI_PC_PORT>/api/telemetria
//    com timeout estrito de no máximo 1500 ms (sem travar a escuta ESP-NOW).
// 3. Ao receber HTTP 200 do Mini PC, verifica se há comandos/configurações
//    pendentes no JSON de resposta e despacha imediatamente o pacote
//    MSG_CONFIG_PARAMETROS (PacoteConfiguracao) ao periférico via ESP-NOW.
// ============================================================================

static const uint16_t MINI_PC_HTTP_TIMEOUT_MS = 1500; // Timeout máximo de 1500 ms (conexão e leitura)
static const char* const MINI_PC_TELEMETRIA_PATH = "/api/telemetria";

WifiManager wifiManager(WIFI_SSID, WIFI_PASSWORD);
DeviceManager deviceManager(30000); // 30 KB de margem de segurança de heap
EspNowCentral espNowCentral;

unsigned long ultimoBroadcastBeaconMs = 0;

// Estrutura para transferir pacotes do callback Wi-Fi/ESP-NOW (Core 0) para a
// task de despacho HTTP (Core 1) em microssegundos, garantindo que o driver
// Wi-Fi/ESP-NOW nunca seja bloqueado por operações de socket TCP/HTTP.
struct EventoTelemetriaPush {
    uint8_t mac[6];
    char macStr[18];
    PacoteDadosSensores dados;
};

static QueueHandle_t filaPushMiniPC = nullptr;
static SemaphoreHandle_t mutexCentral = nullptr;

// ============================================================================
// UTILITÁRIOS DE FORMATAÇÃO E PARSING JSON (ZERO DEPENDÊNCIAS EXTERNAS)
// ============================================================================

static String escapeJsonString(const String& raw) {
    String out;
    out.reserve(raw.length() + 4);
    for (size_t i = 0; i < raw.length(); i++) {
        char c = raw[i];
        if (c == '\"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if ((uint8_t)c >= 0x20) out += c;
    }
    return out;
}

static String buildMiniPcTelemetryUrl() {
    String host = String(MINI_PC_IP);
    host.trim();
    if (host.startsWith("http://")) {
        host = host.substring(7);
    } else if (host.startsWith("https://")) {
        host = host.substring(8);
    }
    int slashIdx = host.indexOf('/');
    if (slashIdx != -1) {
        host = host.substring(0, slashIdx);
    }
    return String("http://") + host + ":" + String(MINI_PC_PORT) + String(MINI_PC_TELEMETRIA_PATH);
}

// Extrai o valor escalar (string, número ou booleano) de uma chave JSON dentro de um trecho
static String extractJsonValue(const String& json, const char* key) {
    String pattern = String("\"") + key + "\"";
    int keyPos = json.indexOf(pattern);
    while (keyPos != -1) {
        int colonPos = keyPos + pattern.length();
        while (colonPos < (int)json.length() && (json[colonPos] == ' ' || json[colonPos] == '\t' || json[colonPos] == '\r' || json[colonPos] == '\n')) {
            colonPos++;
        }
        if (colonPos < (int)json.length() && json[colonPos] == ':') {
            int valStart = colonPos + 1;
            while (valStart < (int)json.length() && (json[valStart] == ' ' || json[valStart] == '\t' || json[valStart] == '\r' || json[valStart] == '\n')) {
                valStart++;
            }
            if (valStart >= (int)json.length()) return "";

            if (json[valStart] == '\"') {
                String result;
                for (int i = valStart + 1; i < (int)json.length(); i++) {
                    if (json[i] == '\\' && (i + 1) < (int)json.length()) {
                        result += json[i + 1];
                        i++;
                    } else if (json[i] == '\"') {
                        return result;
                    } else {
                        result += json[i];
                    }
                }
                return "";
            } else if (json[valStart] != '{' && json[valStart] != '[') {
                int valEnd = valStart;
                while (valEnd < (int)json.length() &&
                       json[valEnd] != ',' && json[valEnd] != '}' && json[valEnd] != ']' &&
                       json[valEnd] != '\r' && json[valEnd] != '\n') {
                    valEnd++;
                }
                String rawVal = json.substring(valStart, valEnd);
                rawVal.trim();
                if (rawVal == "null") return "";
                return rawVal;
            }
            return "";
        }
        keyPos = json.indexOf(pattern, keyPos + pattern.length());
    }
    return "";
}

// Extrai um sub-objeto JSON {...} associado a uma chave (ex: "config": {...} ou "pendingConfig": {...})
static String extractJsonObject(const String& json, const char* key) {
    String pattern = String("\"") + key + "\"";
    int keyPos = json.indexOf(pattern);
    if (keyPos == -1) return "";

    int colonPos = json.indexOf(':', keyPos + pattern.length());
    if (colonPos == -1) return "";

    int braceStart = colonPos + 1;
    while (braceStart < (int)json.length() && (json[braceStart] == ' ' || json[braceStart] == '\t' || json[braceStart] == '\r' || json[braceStart] == '\n')) {
        braceStart++;
    }
    if (braceStart >= (int)json.length() || json[braceStart] != '{') return "";

    int depth = 0;
    bool inQuotes = false;
    for (int i = braceStart; i < (int)json.length(); i++) {
        char c = json[i];
        if (c == '\\' && inQuotes) {
            i++;
            continue;
        }
        if (c == '\"') {
            inQuotes = !inQuotes;
        } else if (!inQuotes) {
            if (c == '{') depth++;
            else if (c == '}') {
                depth--;
                if (depth == 0) {
                    return json.substring(braceStart, i + 1);
                }
            }
        }
    }
    return "";
}

// Extrai um array JSON [...] associado a uma chave (ex: "commands": [...])
static String extractJsonArray(const String& json, const char* key) {
    String pattern = String("\"") + key + "\"";
    int keyPos = json.indexOf(pattern);
    if (keyPos == -1) return "";

    int colonPos = json.indexOf(':', keyPos + pattern.length());
    if (colonPos == -1) return "";

    int arrStart = colonPos + 1;
    while (arrStart < (int)json.length() && (json[arrStart] == ' ' || json[arrStart] == '\t' || json[arrStart] == '\r' || json[arrStart] == '\n')) {
        arrStart++;
    }
    if (arrStart >= (int)json.length() || json[arrStart] != '[') return "";

    int depth = 0;
    bool inQuotes = false;
    for (int i = arrStart; i < (int)json.length(); i++) {
        char c = json[i];
        if (c == '\\' && inQuotes) {
            i++;
            continue;
        }
        if (c == '\"') {
            inQuotes = !inQuotes;
        } else if (!inQuotes) {
            if (c == '[') depth++;
            else if (c == ']') {
                depth--;
                if (depth == 0) {
                    return json.substring(arrStart, i + 1);
                }
            }
        }
    }
    return "";
}

static bool isValidOperationMode(const String& rawMode) {
    String m = rawMode;
    m.trim();
    m.toUpperCase();
    return (m == "ONLINE" || m == "LIGHT_SLEEP" || m == "LIGHT SLEEP" || m == "DEEP_SLEEP" || m == "DEEP SLEEP");
}

static uint16_t parsePositiveMinutes(const String& rawVal) {
    if (rawVal.length() == 0) return 0;
    String digits;
    for (size_t i = 0; i < rawVal.length(); i++) {
        if (isDigit(rawVal[i])) digits += rawVal[i];
        else if (digits.length() > 0) break;
    }
    if (digits.length() == 0) return 0;
    long v = digits.toInt();
    return (v > 0 && v <= 1440) ? (uint16_t)v : 0;
}

static void applyFieldValueCommand(const String& fieldName, const String& valStr,
                                   uint16_t& inoutIntervalo, String& inoutModo, String& inoutNome) {
    if (fieldName == "interval" || fieldName == "intervalo" || fieldName == "novoIntervaloMin" || fieldName == "intervaloMin") {
        uint16_t parsed = parsePositiveMinutes(valStr);
        if (parsed > 0) inoutIntervalo = parsed;
    } else if (fieldName == "mode" || fieldName == "modo" || fieldName == "novoModo" || fieldName == "modoOperacao") {
        if (isValidOperationMode(valStr)) {
            String upper = valStr;
            upper.trim();
            upper.toUpperCase();
            inoutModo = DeviceManager::normalizeModeForEspNow(upper);
        }
    } else if (fieldName == "name" || fieldName == "nome" || fieldName == "novoNome" || fieldName == "nodeName") {
        if (valStr.length() > 0) inoutNome = valStr;
    }
}

// ============================================================================
// MONTAGEM DO PAYLOAD JSON E PROCESSAMENTO DA RESPOSTA DO MINI PC
// ============================================================================

static String montarPayloadTelemetriaJson(const PacoteDadosSensores& dados, const char* macStr) {
    String nodeIdStr = String(dados.nodeId);
    String nomeEfetivo = String(dados.nodeName);
    String modoEfetivo = String(dados.modoOperacao);

    if (xSemaphoreTake(mutexCentral, pdMS_TO_TICKS(50)) == pdTRUE) {
        const Device* dev = deviceManager.getDevice(nodeIdStr);
        if (dev != nullptr) {
            if (dev->hasUserName && dev->targetName.length() > 0) {
                nomeEfetivo = dev->targetName;
            }
            if (modoEfetivo == "PAREAMENTO") {
                modoEfetivo = dev->hasUserMode ? dev->targetModeEspNow : "ONLINE";
            }
        }
        xSemaphoreGive(mutexCentral);
    }

    if (modoEfetivo.length() == 0) {
        modoEfetivo = "ONLINE";
    } else {
        modoEfetivo = DeviceManager::normalizeModeForEspNow(modoEfetivo);
    }

    String macFinal = (strlen(macStr) > 0) ? String(macStr) : String(dados.nodeMac);

    String json;
    json.reserve(256);
    json += "{";
    json += "\"nodeId\":\"" + escapeJsonString(nodeIdStr) + "\",";
    json += "\"nodeName\":\"" + escapeJsonString(nomeEfetivo) + "\",";
    json += "\"sensorUmidade\":" + String(dados.sensorUmidade, 2) + ",";
    json += "\"bateriaPct\":" + String(dados.bateriaPct) + ",";
    json += "\"modoOperacao\":\"" + escapeJsonString(modoEfetivo) + "\",";
    json += "\"tempoUptimeSegundos\":" + String(dados.tempoUptimeSegundos) + ",";
    json += "\"macAddr\":\"" + escapeJsonString(macFinal) + "\"";
    json += "}";
    return json;
}

// Analisa o corpo HTTP 200 retornado pelo Mini PC e, caso haja comandos pendentes
// para o periférico, despacha imediatamente MSG_CONFIG_PARAMETROS via ESP-NOW.
static bool processarRespostaMiniPC(const String& respBody, const PacoteDadosSensores& dadosOriginais, const uint8_t* macPeriferico) {
    String trimmed = respBody;
    trimmed.trim();
    if (trimmed.length() < 2 || trimmed == "{}") {
        return false;
    }

    // Verifica sub-objetos dedicados de comando ("pendingConfig", "config", "command", "comando")
    String subObj = extractJsonObject(trimmed, "pendingConfig");
    bool hasExplicitSubObj = (subObj.length() > 0);
    if (!hasExplicitSubObj) {
        subObj = extractJsonObject(trimmed, "config");
        hasExplicitSubObj = (subObj.length() > 0);
    }
    if (!hasExplicitSubObj) {
        subObj = extractJsonObject(trimmed, "command");
        hasExplicitSubObj = (subObj.length() > 0);
    }
    if (!hasExplicitSubObj) {
        subObj = extractJsonObject(trimmed, "comando");
        hasExplicitSubObj = (subObj.length() > 0);
    }

    String commandsArr = extractJsonArray(trimmed, "commands");
    if (commandsArr.length() == 0) {
        commandsArr = extractJsonArray(trimmed, "comandos");
    }

    // Se o servidor informou explicitamente que não há pendências (ex: "hasPendingConfig": false)
    // e não enviou objeto/lista de comandos, ignora imediatamente.
    String explicitFlag = extractJsonValue(trimmed, "hasPendingConfig");
    if (explicitFlag.length() == 0) explicitFlag = extractJsonValue(trimmed, "hasConfig");
    if (explicitFlag.length() == 0) explicitFlag = extractJsonValue(trimmed, "hasCommand");
    explicitFlag.toLowerCase();

    if (explicitFlag == "false" && !hasExplicitSubObj && commandsArr.length() == 0) {
        return false;
    }

    const String& escopo = hasExplicitSubObj ? subObj : trimmed;
    bool hasExplicitTrueFlag = (explicitFlag == "true" || hasExplicitSubObj || commandsArr.length() > 0);

    // Valida se o comando é direcionado a este nodeId (ou curinga "*")
    String targetId = extractJsonValue(escopo, "targetNodeId");
    if (targetId.length() == 0) targetId = extractJsonValue(escopo, "nodeId");
    if (targetId.length() == 0) targetId = extractJsonValue(escopo, "espId");

    String currentNodeId = String(dadosOriginais.nodeId);
    if (targetId.length() > 0 && targetId != "*" && targetId != currentNodeId) {
        // Caso o Mini PC tenha enviado configuração para outro nó já conhecido no Central, enfileira para ele!
        Serial.printf("[MINI PC] Comando recebido destinado a outro nó ('%s'). Enfileirando no Central.\n", targetId.c_str());
    } else {
        targetId = currentNodeId;
    }

    uint16_t novoIntervaloMin = 0;
    String novoModo = "";
    String novoNome = "";

    // 1. Formato lista de comandos: "commands": [{"field": "interval", "value": 15}, ...]
    if (commandsArr.length() > 2) {
        int pos = 0;
        while (pos < (int)commandsArr.length()) {
            int openBrace = commandsArr.indexOf('{', pos);
            if (openBrace == -1) break;
            int closeBrace = commandsArr.indexOf('}', openBrace);
            if (closeBrace == -1) break;
            String itemObj = commandsArr.substring(openBrace, closeBrace + 1);
            String fName = extractJsonValue(itemObj, "field");
            String fVal  = extractJsonValue(itemObj, "value");
            if (fName.length() > 0) {
                applyFieldValueCommand(fName, fVal, novoIntervaloMin, novoModo, novoNome);
            }
            pos = closeBrace + 1;
        }
    }

    // 2. Formato par {"field": "...", "value": "..."} no escopo
    String singleField = extractJsonValue(escopo, "field");
    if (singleField.length() > 0) {
        String singleVal = extractJsonValue(escopo, "value");
        applyFieldValueCommand(singleField, singleVal, novoIntervaloMin, novoModo, novoNome);
        hasExplicitTrueFlag = true;
    }

    // 3. Formato direto por chaves de configuração (intervalo, modo e nome)
    String strIntervalo = extractJsonValue(escopo, "novoIntervaloMin");
    if (strIntervalo.length() == 0) strIntervalo = extractJsonValue(escopo, "intervaloMin");
    if (strIntervalo.length() == 0) strIntervalo = extractJsonValue(escopo, "interval");
    if (strIntervalo.length() == 0) strIntervalo = extractJsonValue(escopo, "intervalo");
    if (strIntervalo.length() > 0) {
        uint16_t parsedInt = parsePositiveMinutes(strIntervalo);
        if (parsedInt > 0) novoIntervaloMin = parsedInt;
    }

    String strModo = extractJsonValue(escopo, "novoModo");
    if (strModo.length() == 0) strModo = extractJsonValue(escopo, "mode");
    if (strModo.length() == 0) strModo = extractJsonValue(escopo, "modo");
    if (strModo.length() == 0 && hasExplicitTrueFlag) strModo = extractJsonValue(escopo, "modoOperacao");
    if (strModo.length() > 0 && isValidOperationMode(strModo)) {
        strModo.trim();
        strModo.toUpperCase();
        novoModo = DeviceManager::normalizeModeForEspNow(strModo);
    }

    String strNome = extractJsonValue(escopo, "novoNome");
    if (strNome.length() == 0 && hasExplicitTrueFlag) {
        strNome = extractJsonValue(escopo, "name");
        if (strNome.length() == 0) strNome = extractJsonValue(escopo, "nome");
        if (strNome.length() == 0) strNome = extractJsonValue(escopo, "nodeName");
    }
    if (strNome.length() > 0) {
        novoNome = strNome;
    }

    // Se nenhum parâmetro de configuração foi enviado na resposta HTTP 200, encerra
    if (novoIntervaloMin == 0 && novoModo.length() == 0 && novoNome.length() == 0) {
        return false;
    }

    // Verifica se há mudança real ou pendência para evitar loop caso o Mini PC retorne estado fixo
    String forceApply = extractJsonValue(escopo, "forceApply");
    forceApply.toLowerCase();
    bool precisaDespachar = (forceApply == "true" || dadosOriginais.pareamentoAtivo == 1);

    PacoteConfiguracao cfgParaEnvio = {};
    uint8_t macDestino[6] = {0};
    bool temMacDestino = false;

    if (xSemaphoreTake(mutexCentral, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!deviceManager.hasDevice(targetId)) {
            deviceManager.registerDevice(targetId, targetId);
        }
        Device* dev = deviceManager.getDevice(targetId);
        if (dev != nullptr) {
            if (novoIntervaloMin > 0 && (!dev->hasUserInterval || dev->targetIntervalMin != novoIntervaloMin)) {
                precisaDespachar = true;
            }
            if (novoModo.length() > 0) {
                String modoReportado = DeviceManager::normalizeModeForEspNow(String(dadosOriginais.modoOperacao));
                if (!dev->hasUserMode || dev->targetModeEspNow != novoModo ||
                    (targetId == currentNodeId && modoReportado != "PAREAMENTO" && modoReportado != novoModo)) {
                    precisaDespachar = true;
                }
            }
            if (novoNome.length() > 0) {
                if (!dev->hasUserName || dev->targetName != novoNome ||
                    (targetId == currentNodeId && String(dadosOriginais.nodeName) != novoNome)) {
                    precisaDespachar = true;
                }
            }
            if (dev->hasPendingConfig) {
                precisaDespachar = true;
            }

            if (precisaDespachar) {
                deviceManager.queueConfig(targetId, novoIntervaloMin, novoModo, novoNome);
                if (targetId == currentNodeId && macPeriferico != nullptr) {
                    memcpy(macDestino, macPeriferico, 6);
                    temMacDestino = true;
                } else if (dev->hasMac) {
                    memcpy(macDestino, dev->macAddr, 6);
                    temMacDestino = true;
                }
                deviceManager.getPendingConfig(targetId, cfgParaEnvio, nullptr);
                dev->lastConfigSendMs = millis();
            }
        }
        xSemaphoreGive(mutexCentral);
    }

    if (!precisaDespachar) {
        return false;
    }

    Serial.printf("[MINI PC -> ESP-NOW] Comando pendente recebido para '%s'! (Intervalo: %u min | Modo: %s | Nome: %s)\n",
                  targetId.c_str(), cfgParaEnvio.novoIntervaloMin, cfgParaEnvio.novoModo, cfgParaEnvio.novoNome);

    if (temMacDestino) {
        Serial.printf("[MINI PC -> ESP-NOW] Despachando MSG_CONFIG_PARAMETROS imediatamente para '%s'...\n", targetId.c_str());
        return espNowCentral.enviarConfiguracao(macDestino, cfgParaEnvio);
    } else {
        Serial.printf("[MINI PC -> ESP-NOW] MAC de '%s' ainda não capturado. Configuração aguardando na fila prioritária.\n", targetId.c_str());
        return false;
    }
}

// Dispara o HTTP POST ativo para o Mini PC com timeout curto (máximo 1500 ms)
static void despacharTelemetriaParaMiniPC(const EventoTelemetriaPush& evento) {
    if (!wifiManager.isConnected()) {
        Serial.println("[PUSH HTTP] Wi-Fi desconectado no momento. Escuta ESP-NOW segue ativa; WifiManager reconectará em background.");
        return;
    }

    String url = buildMiniPcTelemetryUrl();
    String jsonPayload = montarPayloadTelemetriaJson(evento.dados, evento.macStr);

    WiFiClient client;
    HTTPClient http;

    // Timeout estrito de no máximo 1500 ms (conexão TCP e leitura HTTP) para nunca travar o ESP32
    http.setConnectTimeout(MINI_PC_HTTP_TIMEOUT_MS);
    http.setTimeout(MINI_PC_HTTP_TIMEOUT_MS);
    http.setReuse(false);

    unsigned long inicioMs = millis();
    if (!http.begin(client, url)) {
        Serial.printf("[PUSH HTTP] Falha ao inicializar HTTPClient para URL: %s\n", url.c_str());
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");

    Serial.printf("[PUSH HTTP] POST -> %s | Payload: %s\n", url.c_str(), jsonPayload.c_str());
    int httpCode = http.POST(jsonPayload);
    unsigned long duracaoMs = millis() - inicioMs;

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        String responseBody = http.getString();
        Serial.printf("[PUSH HTTP] Resposta HTTP %d em %lu ms: %s\n", httpCode, duracaoMs, responseBody.c_str());

        // Verifica se há comandos/configurações na resposta do Mini PC e despacha via ESP-NOW imediatamente
        processarRespostaMiniPC(responseBody, evento.dados, evento.mac);
    } else if (httpCode > 0) {
        Serial.printf("[PUSH HTTP] Mini PC respondeu com código HTTP %d (%lu ms).\n", httpCode, duracaoMs);
    } else {
        Serial.printf("[PUSH HTTP] Mini PC inacessível/timeout em %lu ms (%s). Rádio ESP-NOW permanece livre!\n",
                      duracaoMs, http.errorToString(httpCode).c_str());
    }

    http.end();
}

// Task dedicada no Core 1 para executar o despacho HTTP Push sem bloquear a task Wi-Fi/ESP-NOW (Core 0)
static void taskDespachoMiniPC(void* pvParameters) {
    (void)pvParameters;
    EventoTelemetriaPush evento;

    for (;;) {
        if (xQueueReceive(filaPushMiniPC, &evento, portMAX_DELAY) == pdTRUE) {
            despacharTelemetriaParaMiniPC(evento);
        }
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
    Serial.println("  MODO: CLIENTE PUSH ATIVO (ESP-NOW <-> MINI PC OFFLINE)");
    Serial.println("========================================================");

    mutexCentral = xSemaphoreCreateMutex();
    filaPushMiniPC = xQueueCreate(20, sizeof(EventoTelemetriaPush));

    // Inicia task dedicada de despacho HTTP (Core 1, stack de 8 KB)
    xTaskCreatePinnedToCore(
        taskDespachoMiniPC,
        "TaskPushMiniPC",
        8192,
        nullptr,
        2,
        nullptr,
        1
    );

    // 1. Conexão Wi-Fi em modo Station usando WifiManager (reconexão automática habilitada)
    Serial.println("[Wi-Fi] Conectando a rede local da fazenda...");
    bool wifiOk = wifiManager.connect(15000); // 15 segundos de tentativa inicial

    if (wifiOk) {
        Serial.println("\n********************************************************");
        Serial.println("  SUCESSO: ESP32 CENTRAL CONECTADO AO WI-FI!");
        Serial.print("  >>> IP DO ESP32 CENTRAL: ");
        Serial.println(wifiManager.getLocalIP());
        Serial.print("  >>> DESTINO MINI PC (PUSH): ");
        Serial.println(buildMiniPcTelemetryUrl());
        Serial.printf("  >>> TIMEOUT MAXIMO HTTP: %u ms\n", MINI_PC_HTTP_TIMEOUT_MS);
        Serial.println("********************************************************\n");
    } else {
        // Mantém modo WIFI_STA no canal padrão para operar o ESP-NOW imediatamente
        // enquanto o WifiManager tenta reconectar ao roteador automaticamente no loop()
        WiFi.mode(WIFI_STA);
        Serial.println("\n[Wi-Fi] Roteador ainda indisponivel. Rádio ESP-NOW iniciará normalmente");
        Serial.println("        e o WifiManager reconectará automaticamente em background!\n");
    }

    // 2. Inicializa o ESP-NOW compartilhando o rádio Wi-Fi atual
    espNowCentral.iniciar(ESPNOW_WIFI_CHANNEL, false);

    // 3. Callback de recepção de telemetria (MSG_DADOS_SENSORES) dos nós periféricos
    espNowCentral.definirCallbackRecepcao([](const uint8_t* mac, const PacoteDadosSensores& dados) {
        EventoTelemetriaPush evento = {};
        memcpy(evento.mac, mac, 6);
        snprintf(evento.macStr, sizeof(evento.macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        memcpy(&evento.dados, &dados, sizeof(PacoteDadosSensores));

        Serial.printf("\n[CENTRAL] MSG_DADOS_SENSORES recebido de %s (%s) | Umidade: %.1f%% | Bateria: %d%%\n",
                      dados.nodeName, dados.nodeId, dados.sensorUmidade, dados.bateriaPct);

        // Atualiza registro local e verifica se já existe configuração pendente aguardando ACK
        PacoteConfiguracao cfgPendente = {};
        bool despacharPendenteLocal = false;

        if (xSemaphoreTake(mutexCentral, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (!deviceManager.hasDevice(dados.nodeId)) {
                deviceManager.registerDevice(dados.nodeId, dados.nodeName, evento.macStr);
            }
            deviceManager.setDeviceMac(dados.nodeId, mac);
            deviceManager.updateTelemetry(
                dados.nodeId,
                dados.sensorUmidade,
                dados.bateriaPct,
                dados.modoOperacao,
                dados.tempoUptimeSegundos,
                dados.nodeName
            );

            uint8_t macDest[6];
            if (deviceManager.getPendingConfig(dados.nodeId, cfgPendente, macDest)) {
                despacharPendenteLocal = true;
                Device* dev = deviceManager.getDevice(dados.nodeId);
                if (dev) dev->lastConfigSendMs = millis();
            }
            xSemaphoreGive(mutexCentral);
        }

        if (despacharPendenteLocal) {
            Serial.printf("[CENTRAL] PRIORIDADE: Reenviando configuração pendente para '%s' via ESP-NOW...\n", dados.nodeId);
            espNowCentral.enviarConfiguracao(mac, cfgPendente);
        }

        // Aciona imediatamente o envio do HTTP POST ativo para o Mini PC (sem travar a task Wi-Fi/ESP-NOW)
        if (filaPushMiniPC != nullptr) {
            if (xQueueSend(filaPushMiniPC, &evento, 0) != pdTRUE) {
                // Se a fila estiver cheia (Mini PC offline sob rajada intensa), descarta o item mais antigo
                // e insere a leitura mais recente para manter dados frescos sem bloquear o rádio
                EventoTelemetriaPush descartado;
                xQueueReceive(filaPushMiniPC, &descartado, 0);
                xQueueSend(filaPushMiniPC, &evento, 0);
            }
        }
    });

    // 4. Callback de confirmação (MSG_ACK_CONFIG) vinda do periférico após aplicar configuração
    espNowCentral.definirCallbackAckConfig([](const uint8_t* mac, const PacoteAckConfig& ack) {
        (void)mac;
        Serial.printf("\n[CENTRAL] ACK de configuração confirmado pelo periférico '%s'! Status: %s\n",
                      ack.nodeId, ack.sucesso ? "SUCESSO" : "ERRO");
        if (ack.sucesso) {
            if (xSemaphoreTake(mutexCentral, pdMS_TO_TICKS(50)) == pdTRUE) {
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
                xSemaphoreGive(mutexCentral);
            }
        }
    });

    Serial.println("[ESP32 Central] Operando exclusivamente com rádio ESP-NOW + Cliente Push HTTP para Mini PC!\n");
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
    // 1. Reenvio ativo de configurações pendentes para periféricos acordados até confirmação (MSG_ACK_CONFIG)
    static unsigned long ultimoRetryConfigMs = 0;
    if (millis() - ultimoRetryConfigMs >= 250) {
        ultimoRetryConfigMs = millis();

        if (xSemaphoreTake(mutexCentral, pdMS_TO_TICKS(30)) == pdTRUE) {
            size_t count = deviceManager.getDeviceCount();
            for (size_t i = 0; i < count; i++) {
                Device* dev = deviceManager.getDeviceByIndex(i);
                if (dev && dev->hasPendingConfig && dev->hasMac) {
                    if ((millis() - dev->lastSeen < 15000) && (millis() - dev->lastConfigSendMs >= 450)) {
                        dev->lastConfigSendMs = millis();
                        uint8_t macCopia[6];
                        memcpy(macCopia, dev->macAddr, 6);
                        PacoteConfiguracao cfgCopia = dev->pendingConfig;
                        String idCopia = dev->id;
                        xSemaphoreGive(mutexCentral);

                        Serial.printf("[CENTRAL] Reenviando MSG_CONFIG_PARAMETROS para '%s' (aguardando ACK)...\n", idCopia.c_str());
                        espNowCentral.enviarConfiguracao(macCopia, cfgCopia);

                        if (xSemaphoreTake(mutexCentral, pdMS_TO_TICKS(30)) != pdTRUE) {
                            break;
                        }
                    }
                }
            }
            xSemaphoreGive(mutexCentral);
        }
    }

    // 2. Emissão contínua de Broadcast Beacon (a cada 1.5s) para auto-discovery dos periféricos
    if (millis() - ultimoBroadcastBeaconMs >= 1500) {
        ultimoBroadcastBeaconMs = millis();
        espNowCentral.enviarBroadcastBeacon("CENTRAL_SITIO");
    }

    // 3. Reconexão automática do WifiManager caso o sinal Wi-Fi oscile ou caia
    wifiManager.handleReconnect();

    // 4. Monitoramento local de presença dos periféricos (detecção de OFFLINE no Serial)
    static unsigned long ultimoCheckDevices = 0;
    if (millis() - ultimoCheckDevices > 2000) {
        ultimoCheckDevices = millis();
        if (xSemaphoreTake(mutexCentral, pdMS_TO_TICKS(30)) == pdTRUE) {
            deviceManager.checkScheduleTimeout(30000);
            xSemaphoreGive(mutexCentral);
        }
    }

    delay(10);
}