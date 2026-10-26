#include "WifiManager.h"

WifiManager::WifiManager() 
    : ssid(nullptr), password(nullptr), lastReconnectAttempt(0), reconnectInterval(10000) {}

WifiManager::WifiManager(const char* ssid, const char* password) 
    : ssid(ssid), password(password), lastReconnectAttempt(0), reconnectInterval(10000) {}

void WifiManager::setCredentials(const char* ssid, const char* password) {
    this->ssid = ssid;
    this->password = password;
}

bool WifiManager::connect(unsigned long timeoutMs) {
    if (this->ssid == nullptr) {
        Serial.println("[WifiManager] ERRO: SSID nao definido!");
        return false;
    }

    Serial.println();
    Serial.print("[WifiManager] Conectando a rede: ");
    Serial.println(this->ssid);

    // Configura modo Station e ativa reconexão automática nativa do SDK
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    if (this->password != nullptr && strlen(this->password) > 0) {
        WiFi.begin(this->ssid, this->password);
    } else {
        WiFi.begin(this->ssid);
    }

    unsigned long startAttemptTime = millis();

    // Aguarda a conexão exibindo pontos no Monitor Serial
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < timeoutMs) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (isConnected()) {
        Serial.println("[WifiManager] Conexao estabelecida com sucesso!");
        Serial.print("[WifiManager] Endereco IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("[WifiManager] Sinal (RSSI): ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        return true;
    } else {
        Serial.println("[WifiManager] Falha ao conectar (tempo limite esgotado).");
        return false;
    }
}

bool WifiManager::connect(const char* ssid, const char* password, unsigned long timeoutMs) {
    setCredentials(ssid, password);
    return connect(timeoutMs);
}

bool WifiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

void WifiManager::handleReconnect(unsigned long intervalMs) {
    this->reconnectInterval = intervalMs;

    // Se estiver conectado, reseta o temporizador de reconexão
    if (isConnected()) {
        lastReconnectAttempt = millis();
        return;
    }

    // Se desconectado e o intervalo expirou, tenta reconectar sem bloquear o loop
    unsigned long currentMillis = millis();
    if (currentMillis - lastReconnectAttempt >= reconnectInterval) {
        lastReconnectAttempt = currentMillis;
        Serial.println("\n[WifiManager] Alerta: Wi-Fi desconectado! Tentando reconectar em background...");

        WiFi.mode(WIFI_STA);
        if (this->ssid != nullptr) {
            if (this->password != nullptr && strlen(this->password) > 0) {
                WiFi.begin(this->ssid, this->password);
            } else {
                WiFi.begin(this->ssid);
            }
        } else {
            WiFi.reconnect();
        }
    }
}

IPAddress WifiManager::getLocalIP() const {
    return WiFi.localIP();
}

int8_t WifiManager::getRSSI() const {
    return WiFi.RSSI();
}

void WifiManager::disconnect() {
    WiFi.disconnect(true);
    Serial.println("[WifiManager] Wi-Fi desconectado.");
}
