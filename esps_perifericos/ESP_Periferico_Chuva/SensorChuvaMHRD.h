#ifndef SENSOR_CHUVA_MHRD_H
#define SENSOR_CHUVA_MHRD_H

#include <Arduino.h>

// ============================================================================
// CONFIGURAÇÃO DO SENSOR DE CHUVA (MH-RD) COM CHAVEAMENTO MOSFET CANAL N
// ============================================================================

// Classificação qualitativa da condição climática
enum EstadoChuva : uint8_t {
    CHUVA_SECO = 0,             // Sem presença de água nas trilhas
    CHUVA_LEVE = 1,             // Gotículas, garoa ou início de precipitação
    CHUVA_MODERADA_FORTE = 2    // Precipitação intensa / lâmina d'água acumulada
};

// Estrutura de configuração de pinos e calibração
struct ConfigSensorChuva {
    uint8_t pinoMosfetGate;       // GPIO que aciona o Gate do MOSFET Canal N (Low-Side Switch)
    uint8_t pinoAnalogicoAO;      // GPIO conectado à saída analógica (AO) do sensor (ADC1 seguro)
    uint8_t pinoDigitalDO;        // GPIO conectado à saída digital (DO) do comparador LM393
    uint16_t tempoEstabilizacaoMs;// Tempo (ms) para estabilizar a tensão antes da leitura (ex: 50 ms)
    uint16_t adcLimiarSeco;       // Acima deste valor ADC considera-se "Tempo Seco" (ex: 3200)
    uint16_t adcLimiarChuvaForte; // Abaixo deste valor ADC considera-se "Chuva Forte" (ex: 1800)
    uint8_t qtdAmostrasAdc;       // Quantidade de leituras para média e redução de ruído (ex: 8)
};

// Estrutura de dados contendo os resultados processados da leitura
struct LeituraSensorChuva {
    uint16_t valorAdcBruto;        // Valor analógico lido (0 a 4095 na escala de 12 bits)
    float tensaoVolts;             // Tensão equivalente convertida (0.00 V a 3.30 V)
    float porcentagemIntensidade;  // Intensidade estimada de chuva (0.0% a 100.0%)
    bool limiarDigitalDisparado;   // true se o comparador físico disparou (DO em nível ativo)
    EstadoChuva estado;            // Enum de estado (CHUVA_SECO, CHUVA_LEVE, CHUVA_MODERADA_FORTE)
    const char* descricaoEstado;   // Descrição textual formatada para exibição
    uint32_t tempoSensorLigadoMs;  // Duração em milissegundos que o sensor permaneceu energizado
};

// ============================================================================
// CLASSE: SensorChuvaMHRD
// ============================================================================
class SensorChuvaMHRD {
public:
    // Construtor simplificado com pinos padrão
    SensorChuvaMHRD(uint8_t pinoMosfetGate = 23, 
                    uint8_t pinoAnalogicoAO = 34, 
                    uint8_t pinoDigitalDO = 35);

    // Construtor detalhado via estrutura de configuração
    SensorChuvaMHRD(const ConfigSensorChuva& config);

    // Inicializa os pinos e garante que o MOSFET inicie em corte (sensor desligado)
    void begin();

    // Executa o ciclo completo de chaveamento:
    // 1. Liga MOSFET (HIGH)
    // 2. Aguarda estabilização (50 ms)
    // 3. Amostra e lê AO e DO
    // 4. Desliga MOSFET imediatamente (LOW)
    // 5. Processa e classifica os dados
    LeituraSensorChuva realizarLeitura();

    // Imprime um relatório detalhado e formatado no Monitor Serial
    void imprimirLeituraSerial(const LeituraSensorChuva& leitura);

    // Métodos para calibração em tempo de execução
    void setLimiares(uint16_t limiarSeco, uint16_t limiarForte);
    void setTempoEstabilizacao(uint16_t tempoMs);

    // Retorna a configuração atual
    const ConfigSensorChuva& getConfig() const { return _config; }

private:
    ConfigSensorChuva _config;

    void ligarSensor();
    void desligarSensor();
    uint16_t lerAdcFiltrado();
    EstadoChuva classificarEstado(uint16_t adc, const char* &descricao);
    float calcularPorcentagem(uint16_t adc);
};

#endif // SENSOR_CHUVA_MHRD_H
