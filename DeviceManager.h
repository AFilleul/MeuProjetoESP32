#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>
#include <vector>

// Estrutura que representa cada dispositivo/nó registrado
struct Device {
    String id;                  // Identificador único (MAC, UUID, etc.)
    String name;                // Nome amigável / apelido
    String ip;                  // Endereço IP / Host
    uint16_t port;              // Porta de comunicação (opcional)
    bool isOnline;              // Estado atual de conexão
    unsigned long lastSeen;     // Timestamp do último contato em millis()

    Device(const String& devId, const String& devName = "", const String& devIp = "", uint16_t devPort = 0)
        : id(devId), 
          name(devName.length() > 0 ? devName : devId), 
          ip(devIp), 
          port(devPort), 
          isOnline(true), 
          lastSeen(millis()) {}
};

class DeviceManager {
private:
    std::vector<Device> devices;        // Lista dinâmica de dispositivos
    size_t minFreeHeapSafety;           // Margem de segurança de heap livre (em bytes)
    static const size_t ESTIMATED_BYTES_PER_DEVICE = 128; // Estimativa média de RAM por dispositivo

public:
    // Construtor: define margem de segurança de heap (padrão: 30 KB livres)
    explicit DeviceManager(size_t minHeapSafety = 30000);

    // Registro e Gerenciamento
    bool registerDevice(const String& id, const String& name = "", const String& ip = "", uint16_t port = 0);
    bool removeDevice(const String& id);
    bool hasDevice(const String& id) const;
    void clear();

    // Acesso e Consultas
    Device* getDevice(const String& id);
    const Device* getDevice(const String& id) const;
    Device* getDeviceByIndex(size_t index);
    const Device* getDeviceByIndex(size_t index) const;

    size_t getDeviceCount() const;
    size_t getOnlineCount() const;

    // Monitoramento e Estado
    void updateLastSeen(const String& id);
    void setDeviceStatus(const String& id, bool online);
    void checkKeepAlive(unsigned long timeoutMs = 60000);

    // Estimativas de Memória (Heap do ESP32)
    size_t getFreeHeap() const;
    size_t getMinHeapSafety() const;
    size_t getEstimatedCapacity() const; // Quantos novos dispositivos ainda cabem na RAM com segurança

    // Relatório no Monitor Serial
    void printDevices() const;
};

#endif // DEVICE_MANAGER_H
