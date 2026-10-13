#include <Arduino.h>
#include "SensorChuvaMHRD.h"

// ============================================================================
// DEFINIÇÃO DE PINOS DO ESP32
// ============================================================================

// Pino que aciona a Base do Transistor BJT NPN (Low-Side Switch)
#define PIN_BJT_BASE       23   // GPIO 23: Saída digital para controle de saturação/corte do BJT

// Pinos de leitura do sensor MH-RD (módulo comparador LM393)
#define PIN_SENSOR_AO      34   // GPIO 34: ADC1_CH6 (Entrada analógica segura, imune a conflitos de rádio)
#define PIN_SENSOR_DO      35   // GPIO 35: Entrada digital (saída do comparador LM393)

// Intervalo de leitura no loop de demonstração (em milissegundos)
#define INTERVALO_LEITURA_MS 5000 

// ============================================================================
// INSTÂNCIA DO SENSOR DE CHUVA
// ============================================================================

// Instância com parametrização customizada
ConfigSensorChuva cfgChuva = {
    .pinoBjtBase           = PIN_BJT_BASE,
    .pinoAnalogicoAO       = PIN_SENSOR_AO,
    .pinoDigitalDO         = PIN_SENSOR_DO,
    .tempoEstabilizacaoMs  = 50,    // 50 ms de estabilização
    .adcLimiarSeco         = 3200,  // ADC >= 3200 -> Seco
    .adcLimiarChuvaForte   = 1800,  // ADC < 1800 -> Chuva Forte
    .qtdAmostrasAdc        = 8      // Média de 8 amostras
};

SensorChuvaMHRD sensorChuva(cfgChuva);

// Variáveis de controle de tempo e estatísticas
unsigned long ultimaLeituraMs = 0;
uint32_t contadorCiclos = 0;
unsigned long tempoTotalAtivoSensorMs = 0;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=============================================================");
    Serial.println("  SISTEMA IOT ESP32 - MONITORAMENTO DE CHUVA INTELIGENTE");
    Serial.println("  Otimizacao de Energia & Chaveamento com Transistor BJT NPN");
    Serial.println("=============================================================");
    Serial.printf("  -> BJT NPN Base (GND Switch): GPIO %d\n", PIN_BJT_BASE);
    Serial.printf("  -> Entrada Analogica (AO)   : GPIO %d (ADC1 - 12 bits)\n", PIN_SENSOR_AO);
    Serial.printf("  -> Entrada Digital (DO)     : GPIO %d\n", PIN_SENSOR_DO);
    Serial.printf("  -> Tempo de Estabilizacao   : %d ms\n", cfgChuva.tempoEstabilizacaoMs);
    Serial.printf("  -> Intervalo entre Ciclos   : %d s\n", INTERVALO_LEITURA_MS / 1000);
    Serial.println("=============================================================");

    // Inicializa o sensor (configura pinos e garante que o BJT inicie em CORTE)
    sensorChuva.begin();

    Serial.println("[OK] Sensor inicializado com sucesso. Transistor BJT NPN em CORTE (0V na Base).");
    Serial.println("[OK] Aguardando o primeiro ciclo de leitura...\n");
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
    unsigned long agora = millis();

    // Executa a amostragem respeitando o intervalo definido
    if (agora - ultimaLeituraMs >= INTERVALO_LEITURA_MS || contadorCiclos == 0) {
        ultimaLeituraMs = agora;
        contadorCiclos++;

        // 1. Executa a leitura inteligente com ativação transitória do transistor BJT NPN
        LeituraSensorChuva leitura = sensorChuva.realizarLeitura();
        tempoTotalAtivoSensorMs += leitura.tempoSensorLigadoMs;

        // 2. Exibe o relatório formatado no Monitor Serial
        sensorChuva.imprimirLeituraSerial(leitura);

        // 3. Exibe estatísticas de conservação de energia e trilhas de cobre
        float tempoTotalDecorridoSegundos = (float)agora / 1000.0f;
        float percentualTempoLigado = (tempoTotalDecorridoSegundos > 0) 
            ? ((float)tempoTotalAtivoSensorMs / (float)agora) * 100.0f 
            : 0.0f;
        float economiaEnergia = 100.0f - percentualTempoLigado;

        Serial.printf("| Ciclo de Amostragem   : #%-4u                                 |\n", contadorCiclos);
        Serial.printf("| Duty Cycle do Sensor  : %4.2f %% (Economia: %4.2f %% de energia)   |\n", 
                      percentualTempoLigado, economiaEnergia);
        Serial.printf("| Fator Vida Util Trilha: Aumento de aproximadamente %.0fx!         |\n", 
                      (percentualTempoLigado > 0) ? (100.0f / percentualTempoLigado) : 100.0f);
        Serial.println("+-------------------------------------------------------------+\n");
    }

    // O ESP32 pode realizar outras tarefas ou entrar em Light/Deep Sleep aqui
    delay(10);
}
