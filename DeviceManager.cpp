#include "DeviceManager.h"

DeviceManager::DeviceManager(size_t minHeapSafety)
    : minFreeHeapSafety(minHeapSafety) {}

bool DeviceManager::registerDevice(const String& id, const String& name, const String& ip, uint16_t port) {
    if (id.length() == 0) {
        Serial.println("[DeviceManager] ERRO: ID do dispositivo nao pode ser vazio!");
        return false;
    }

    // Se o dispositivo já estiver registrado, apenas atualiza suas informações
    Device* existing = getDevice(id);
    if (existing != nullptr) {
        if (name.length() > 0) existing->name = name;
        if (ip.length() > 0) existing->ip = ip;
        if (port > 0) existing->port = port;
        existing->isOnline = true;
        existing->lastSeen = millis();
        Serial.print("[DeviceManager] Dispositivo atualizado: ");
        Serial.println(id);
        return true;
    }

    // Verificação de segurança de memória RAM (Heap do ESP32)
    size_t currentFreeHeap = ESP.getFreeHeap();
    if (currentFreeHeap < (minFreeHeapSafety + ESTIMATED_BYTES_PER_DEVICE)) {
        Serial.println("\n[DeviceManager] ALERTA DE MEMORIA:");
        Serial.print("  Nao ha memoria RAM livre suficiente para cadastrar o dispositivo '");
        Serial.print(id);
        Serial.println("' com seguranca.");
        Serial.print("  Heap livre atual: ");
        Serial.print(currentFreeHeap);
        Serial.print(" bytes | Margem de seguranca: ");
        Serial.print(minFreeHeapSafety);
        Serial.println(" bytes.");
        return false;
    }

    // Adiciona o novo dispositivo à lista dinâmica
    devices.emplace_back(id, name, ip, port);

    Serial.print("[DeviceManager] Dispositivo registrado com sucesso: '");
    Serial.print(id);
    Serial.print("' | Total registrado: ");
    Serial.print(devices.size());
    Serial.print(" | Heap livre restante: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");

    return true;
}

bool DeviceManager::removeDevice(const String& id) {
    for (auto it = devices.begin(); it != devices.end(); ++it) {
        if (it->id == id) {
            devices.erase(it);
            devices.shrink_to_fit();
            Serial.print("[DeviceManager] Dispositivo removido: ");
            Serial.println(id);
            return true;
        }
    }
    Serial.print("[DeviceManager] Dispositivo nao encontrado para remocao: ");
    Serial.println(id);
    return false;
}

bool DeviceManager::hasDevice(const String& id) const {
    return getDevice(id) != nullptr;
}

void DeviceManager::clear() {
    devices.clear();
    devices.shrink_to_fit();
    Serial.println("[DeviceManager] Todos os dispositivos foram desregistrados.");
}

Device* DeviceManager::getDevice(const String& id) {
    for (auto& dev : devices) {
        if (dev.id == id) {
            return &dev;
        }
    }
    return nullptr;
}

const Device* DeviceManager::getDevice(const String& id) const {
    for (const auto& dev : devices) {
        if (dev.id == id) {
            return &dev;
        }
    }
    return nullptr;
}

Device* DeviceManager::getDeviceByIndex(size_t index) {
    if (index < devices.size()) {
        return &devices[index];
    }
    return nullptr;
}

const Device* DeviceManager::getDeviceByIndex(size_t index) const {
    if (index < devices.size()) {
        return &devices[index];
    }
    return nullptr;
}

size_t DeviceManager::getDeviceCount() const {
    return devices.size();
}

size_t DeviceManager::getOnlineCount() const {
    size_t count = 0;
    for (const auto& dev : devices) {
        if (dev.isOnline) {
            count++;
        }
    }
    return count;
}

void DeviceManager::updateLastSeen(const String& id) {
    Device* dev = getDevice(id);
    if (dev != nullptr) {
        dev->lastSeen = millis();
        dev->isOnline = true;
    }
}

void DeviceManager::setDeviceStatus(const String& id, bool online) {
    Device* dev = getDevice(id);
    if (dev != nullptr) {
        dev->isOnline = online;
        if (online) {
            dev->lastSeen = millis();
        }
    }
}

void DeviceManager::checkKeepAlive(unsigned long timeoutMs) {
    unsigned long now = millis();
    for (auto& dev : devices) {
        if (dev.isOnline && (now - dev.lastSeen > timeoutMs)) {
            dev.isOnline = false;
            Serial.print("[DeviceManager] Dispositivo inativo (timeout): ");
            Serial.println(dev.id);
        }
    }
}

size_t DeviceManager::getFreeHeap() const {
    return ESP.getFreeHeap();
}

size_t DeviceManager::getMinHeapSafety() const {
    return minFreeHeapSafety;
}

size_t DeviceManager::getEstimatedCapacity() const {
    size_t current = ESP.getFreeHeap();
    if (current <= minFreeHeapSafety) {
        return 0;
    }
    return (current - minFreeHeapSafety) / ESTIMATED_BYTES_PER_DEVICE;
}

void DeviceManager::printDevices() const {
    Serial.println("\n=================[ TABELA DE DISPOSITIVOS ]=================");
    Serial.print(" Total cadastrado: ");
    Serial.print(getDeviceCount());
    Serial.print(" | Online: ");
    Serial.print(getOnlineCount());
    Serial.print(" | RAM livre: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" bytes (~");
    Serial.print(getEstimatedCapacity());
    Serial.println(" vagas livres)");
    Serial.println("------------------------------------------------------------");

    if (devices.empty()) {
        Serial.println(" Nenhum dispositivo registrado no momento.");
    } else {
        for (size_t i = 0; i < devices.size(); ++i) {
            const auto& dev = devices[i];
            Serial.print(" [");
            Serial.print(i);
            Serial.print("] ID: ");
            Serial.print(dev.id);
            Serial.print(" | Nome: ");
            Serial.print(dev.name);
            Serial.print(" | IP: ");
            Serial.print(dev.ip.length() > 0 ? dev.ip : "N/A");
            if (dev.port > 0) {
                Serial.print(":");
                Serial.print(dev.port);
            }
            Serial.print(" | Status: ");
            Serial.print(dev.isOnline ? "ONLINE" : "OFFLINE");
            Serial.print(" | Ultimo contato: ");
            Serial.print((millis() - dev.lastSeen) / 1000);
            Serial.println("s atras");
        }
    }
    Serial.println("============================================================\n");
}
