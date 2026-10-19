#include "DeviceManager.h"

DeviceManager::DeviceManager(size_t minHeapSafety)
    : minFreeHeapSafety(minHeapSafety) {}

bool DeviceManager::registerDevice(const String& id, const String& name, const String& ip, uint16_t port) {
    if (id.length() == 0) {
        Serial.println("[DeviceManager] ERRO: ID do dispositivo nao pode ser vazio!");
        return false;
    }

    // Se o dispositivo já estiver registrado, apenas atualiza suas informações sem sobrescrever configurações do usuário
    Device* existing = getDevice(id);
    if (existing != nullptr) {
        if (name.length() > 0 && !existing->hasUserName && !existing->hasPendingConfig) {
            existing->name = name;
        }
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

unsigned long DeviceManager::parseIntervalToMs(const String& intervalStr) {
    int val = intervalStr.toInt();
    if (val <= 0) val = 60;
    if (intervalStr.indexOf("s") != -1 && intervalStr.indexOf("min") == -1) {
        return (unsigned long)val * 1000UL;
    }
    return (unsigned long)val * 60UL * 1000UL;
}

String DeviceManager::normalizeModeForEspNow(const String& rawMode) {
    if (rawMode == "DEEP SLEEP" || rawMode == "DEEP_SLEEP") return "DEEP_SLEEP";
    if (rawMode == "LIGHT SLEEP" || rawMode == "LIGHT_SLEEP") return "LIGHT_SLEEP";
    if (rawMode == "ONLINE") return "ONLINE";
    return rawMode;
}

String DeviceManager::normalizeModeForDisplay(const String& rawMode) {
    if (rawMode == "DEEP_SLEEP" || rawMode == "DEEP SLEEP") return "DEEP SLEEP";
    if (rawMode == "LIGHT_SLEEP" || rawMode == "LIGHT SLEEP") return "LIGHT SLEEP";
    if (rawMode == "ONLINE" || rawMode == "PAREAMENTO") return "ONLINE";
    return rawMode;
}

void DeviceManager::setInterval(const String& id, const String& intervalStr) {
    Device* dev = getDevice(id);
    if (dev != nullptr) {
        dev->interval = intervalStr;
        dev->intervalMs = parseIntervalToMs(intervalStr);
    }
}

void DeviceManager::setUnit(const String& id, const String& unitStr) {
    Device* dev = getDevice(id);
    if (dev != nullptr && unitStr.length() > 0) {
        dev->unit = unitStr;
        dev->hasUserUnit = true;
    }
}

void DeviceManager::setPowerConfig(const String& id, const String& powerType, uint32_t capacityMah) {
    Device* dev = getDevice(id);
    if (dev != nullptr) {
        if (powerType.length() > 0) {
            dev->powerSource = powerType;
        }
        if (capacityMah > 0) {
            dev->batteryCapacityMah = capacityMah;
        }
        dev->hasUserPower = true;
    }
}

void DeviceManager::updateLastSeen(const String& id) {
    Device* dev = getDevice(id);
    if (dev != nullptr) {
        dev->lastSeen = millis();
        dev->isOnline = true;
        dev->connectivity = "ONLINE";
    }
}

void DeviceManager::updateTelemetry(const String& id, float umidade, uint8_t batPct, const String& modo, uint32_t uptimeSec, const String& reportedName) {
    Device* dev = getDevice(id);
    if (dev != nullptr) {
        dev->lastSeen = millis();
        dev->isOnline = true;
        dev->connectivity = "ONLINE";
        dev->sensorValue = String(umidade, 1) + " (" + dev->unit + ")";
        dev->batteryPct = batPct;
        if (uptimeSec > 0) dev->uptimeHours = uptimeSec / 3600;

        // PRIORIDADE DE CONFIGURAÇÃO SOBRE TELEMETRIA:
        // 1. Se o usuário já configurou um nome para este periférico:
        //    - NUNCA deixa o nome vindo da telemetria sobrescrever dev->name.
        //    - Se o periférico reportou um nome diferente do configurado, reativa o envio da configuração!
        if (dev->hasUserName) {
            dev->name = dev->targetName;
            if (reportedName.length() > 0 && reportedName != dev->targetName) {
                dev->hasPendingConfig = true;
            }
        } else if (!dev->hasPendingConfig && reportedName.length() > 0) {
            dev->name = reportedName;
        }

        // 2. Se o usuário já configurou um modo de operação para este periférico:
        //    - NUNCA deixa o modo antigo vindo da telemetria sobrescrever dev->mode.
        //    - Se o periférico reportou um modo operacional diferente do configurado (ignorando flag transitória de PAREAMENTO),
        //      reativa o envio da configuração para forçar o periférico a assumir o modo configurado!
        if (dev->hasUserMode) {
            dev->mode = normalizeModeForDisplay(dev->targetModeEspNow);
            if (modo.length() > 0 && modo != "PAREAMENTO") {
                String reportedNorm = normalizeModeForEspNow(modo);
                if (reportedNorm != dev->targetModeEspNow) {
                    dev->hasPendingConfig = true;
                }
            }
        } else if (!dev->hasPendingConfig && modo.length() > 0 && modo != "PAREAMENTO") {
            dev->mode = normalizeModeForDisplay(modo);
        }
    }
}

void DeviceManager::setDeviceStatus(const String& id, bool online) {
    Device* dev = getDevice(id);
    if (dev != nullptr) {
        dev->isOnline = online;
        dev->connectivity = online ? "ONLINE" : "OFFLINE";
        if (online) {
            dev->lastSeen = millis();
        }
    }
}

void DeviceManager::checkScheduleTimeout(unsigned long toleranciaMs) {
    unsigned long now = millis();
    for (auto& dev : devices) {
        unsigned long tempoDesdeUltimoSinal = now - dev.lastSeen;

        if (dev.mode == "ONLINE") {
            // Em modo ONLINE contínuo, espera sinal a cada 60s ou conforme intervalo
            unsigned long limiteEsperado = (dev.intervalMs > 0 ? dev.intervalMs : 60000UL) + toleranciaMs;
            if (tempoDesdeUltimoSinal > limiteEsperado) {
                if (dev.connectivity != "OFFLINE") {
                    Serial.print("\n[DeviceManager] ⚠️ ALERTA: Periférico '");
                    Serial.print(dev.name);
                    Serial.print("' (");
                    Serial.print(dev.id);
                    Serial.println(") NÃO enviou sinal quando devia! Alterando status para OFFLINE (Vermelho).");
                }
                dev.isOnline = false;
                dev.connectivity = "OFFLINE";
            } else {
                dev.isOnline = true;
                dev.connectivity = "ONLINE";
            }
        } else {
            // Em modo SLEEP (DEEP SLEEP ou LIGHT SLEEP)
            unsigned long limiteEsperado = dev.intervalMs + toleranciaMs;
            if (tempoDesdeUltimoSinal > limiteEsperado) {
                // Passou do horário previsto para acordar e transmitir: NÃO RECEBEU SINAL QUANDO DEVIA!
                if (dev.connectivity != "OFFLINE") {
                    Serial.print("\n[DeviceManager] ⚠️ ALERTA: Periférico em repouso '");
                    Serial.print(dev.name);
                    Serial.print("' (");
                    Serial.print(dev.id);
                    Serial.println(") NÃO acordou/enviou dados no horário previsto! Alterando status para OFFLINE (Vermelho).");
                }
                dev.isOnline = false;
                dev.connectivity = "OFFLINE";
            } else if (tempoDesdeUltimoSinal > 12000) {
                // Dentro do prazo normal de sono: está em repouso planejado
                dev.isOnline = true;
                dev.connectivity = "DORMINDO";
            } else {
                // Acabou de enviar pacote (últimos 12 segundos)
                dev.isOnline = true;
                dev.connectivity = "ONLINE";
            }
        }
    }
}

void DeviceManager::checkKeepAlive(unsigned long timeoutMs) {
    checkScheduleTimeout(timeoutMs);
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
            Serial.print(dev.connectivity);
            Serial.print(" | Ultimo contato: ");
            Serial.print((millis() - dev.lastSeen) / 1000);
            Serial.println("s atras");
        }
    }
    Serial.println("============================================================\n");
}

void DeviceManager::setDeviceMac(const String& id, const uint8_t* mac) {
    Device* dev = getDevice(id);
    if (dev != nullptr && mac != nullptr) {
        memcpy(dev->macAddr, mac, 6);
        dev->hasMac = true;
    }
}

bool DeviceManager::getDeviceMac(const String& id, uint8_t* outMac) const {
    const Device* dev = getDevice(id);
    if (dev != nullptr && dev->hasMac && outMac != nullptr) {
        memcpy(outMac, dev->macAddr, 6);
        return true;
    }
    return false;
}

void DeviceManager::queueConfig(const String& id, uint16_t novoIntervaloMin, const String& novoModo, const String& novoNome) {
    Device* dev = getDevice(id);
    if (dev == nullptr) return;

    dev->hasPendingConfig = true;
    dev->lastConfigSendMs = 0; // Força envio imediato no próximo ciclo
    dev->pendingConfig.tipo = MSG_CONFIG_PARAMETROS;
    strncpy(dev->pendingConfig.targetNodeId, id.c_str(), sizeof(dev->pendingConfig.targetNodeId) - 1);
    dev->pendingConfig.targetNodeId[sizeof(dev->pendingConfig.targetNodeId) - 1] = '\0';
    
    if (novoIntervaloMin > 0) {
        dev->hasUserInterval = true;
        dev->targetIntervalMin = novoIntervaloMin;
        dev->pendingConfig.novoIntervaloMin = novoIntervaloMin;
        setInterval(id, String(novoIntervaloMin) + " min");
    } else if (dev->hasUserInterval) {
        dev->pendingConfig.novoIntervaloMin = dev->targetIntervalMin;
    }

    if (novoModo.length() > 0) {
        String modoEspNow = normalizeModeForEspNow(novoModo);
        String modoDisplay = normalizeModeForDisplay(novoModo);

        dev->hasUserMode = true;
        dev->targetModeEspNow = modoEspNow;
        dev->mode = modoDisplay;

        strncpy(dev->pendingConfig.novoModo, modoEspNow.c_str(), sizeof(dev->pendingConfig.novoModo) - 1);
        dev->pendingConfig.novoModo[sizeof(dev->pendingConfig.novoModo) - 1] = '\0';
    } else if (dev->hasUserMode) {
        strncpy(dev->pendingConfig.novoModo, dev->targetModeEspNow.c_str(), sizeof(dev->pendingConfig.novoModo) - 1);
        dev->pendingConfig.novoModo[sizeof(dev->pendingConfig.novoModo) - 1] = '\0';
    }

    if (novoNome.length() > 0) {
        dev->hasUserName = true;
        dev->targetName = novoNome;
        dev->name = novoNome;

        strncpy(dev->pendingConfig.novoNome, novoNome.c_str(), sizeof(dev->pendingConfig.novoNome) - 1);
        dev->pendingConfig.novoNome[sizeof(dev->pendingConfig.novoNome) - 1] = '\0';
    } else if (dev->hasUserName) {
        strncpy(dev->pendingConfig.novoNome, dev->targetName.c_str(), sizeof(dev->pendingConfig.novoNome) - 1);
        dev->pendingConfig.novoNome[sizeof(dev->pendingConfig.novoNome) - 1] = '\0';
    }

    dev->pendingConfig.timestampMillis = millis();

    Serial.printf("[DeviceManager] Configuracao prioritaria registrada para '%s' (Intervalo: %d min, Modo: %s, Nome: %s)\n",
                  id.c_str(), dev->pendingConfig.novoIntervaloMin, dev->pendingConfig.novoModo, dev->pendingConfig.novoNome);
}

bool DeviceManager::hasPendingConfig(const String& id) const {
    const Device* dev = getDevice(id);
    return (dev != nullptr && dev->hasPendingConfig);
}

bool DeviceManager::getPendingConfig(const String& id, PacoteConfiguracao& outCfg, uint8_t* outMac) const {
    const Device* dev = getDevice(id);
    if (dev != nullptr && dev->hasPendingConfig) {
        memcpy(&outCfg, &dev->pendingConfig, sizeof(PacoteConfiguracao));
        if (outMac != nullptr && dev->hasMac) {
            memcpy(outMac, dev->macAddr, 6);
        }
        return true;
    }
    return false;
}

void DeviceManager::clearPendingConfig(const String& id) {
    Device* dev = getDevice(id);
    if (dev != nullptr) {
        dev->hasPendingConfig = false;
        // Mantemos os campos de dev->pendingConfig intactos caso seja necessário reenviar futuramente
        Serial.printf("[DeviceManager] Sincronizacao confirmada (ACK recebido) para o periferico '%s'.\n", id.c_str());
    }
}

void DeviceManager::addNotification(const String& msg) {
    pendingNotifications.push_back(msg);
    if (pendingNotifications.size() > 10) {
        pendingNotifications.erase(pendingNotifications.begin());
    }
}

std::vector<String> DeviceManager::getAndClearNotifications() {
    std::vector<String> notifs = pendingNotifications;
    pendingNotifications.clear();
    return notifs;
}
