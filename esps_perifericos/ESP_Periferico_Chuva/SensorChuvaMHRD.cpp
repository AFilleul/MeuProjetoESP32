#include "SensorChuvaMHRD.h"

// ============================================================================
// CONSTRUTORES
// ============================================================================

SensorChuvaMHRD::SensorChuvaMHRD(uint8_t pinoMosfetGate, uint8_t pinoAnalogicoAO, uint8_t pinoDigitalDO) {
    _config.pinoMosfetGate        = pinoMosfetGate;
    _config.pinoAnalogicoAO       = pinoAnalogicoAO;
    _config.pinoDigitalDO         = pinoDigitalDO;
    _config.tempoEstabilizacaoMs  = 50;   // 50 ms para estabilização de tensão e carga capacitiva
    _config.adcLimiarSeco         = 3200; // Acima de 3200 = Placa totalmente seca
    _config.adcLimiarChuvaForte   = 1800; // Abaixo de 1800 = Lâmina contínua de água / chuva torrencial
    _config.qtdAmostrasAdc        = 8;    // Média de 8 amostras para filtrar ruído elétrico
}

SensorChuvaMHRD::SensorChuvaMHRD(const ConfigSensorChuva& config) 
    : _config(config) {
}

// ============================================================================
// INICIALIZAÇÃO DE HARDWARE
// ============================================================================

void SensorChuvaMHRD::begin() {
    // Configura o pino de controle do MOSFET Canal N
    pinMode(_config.pinoMosfetGate, OUTPUT);

    // GARANTIA DE CORTE: Inicia com o MOSFET desligado (Gate = LOW).
    // Assim, o caminho do GND do sensor fica aberto e a placa de chuva
    // permanece totalmente desenergizada, eliminando eletrólise e consumo de corrente.
    digitalWrite(_config.pinoMosfetGate, LOW);

    // Configura pinos de leitura do ESP32
    pinMode(_config.pinoAnalogicoAO, INPUT);
    pinMode(_config.pinoDigitalDO, INPUT);

    // Ajusta a resolução e atenuação do ADC do ESP32 (0 a ~3.3V)
    #if defined(ESP32)
    analogReadResolution(12);               // Escala de 12 bits: 0 a 4095
    analogSetAttenuation(ADC_11db);          // Atenuação de 11dB para ler faixa cheia até ~3.3V
    #endif
}

// ============================================================================
// CONTROLE DE ENERGIA (MOSFET LOW-SIDE SWITCH)
// ============================================================================

void SensorChuvaMHRD::ligarSensor() {
    // Aplica nível alto (3.3V) no Gate do MOSFET Canal N.
    // O transistor entra em saturação (condução), conectando o pino GND do sensor ao GND comum do ESP32.
    digitalWrite(_config.pinoMosfetGate, HIGH);
}

void SensorChuvaMHRD::desligarSensor() {
    // Aplica nível baixo (0V) no Gate do MOSFET.
    // O transistor entra em corte, interrompendo o fluxo de elétrons pelo polo negativo do sensor.
    digitalWrite(_config.pinoMosfetGate, LOW);
}

// ============================================================================
// LEITURA COM FILTRO DE MÉDIA MÓVEL
// ============================================================================

uint16_t SensorChuvaMHRD::lerAdcFiltrado() {
    uint32_t soma = 0;
    
    // Descarta a primeira leitura para estabilização do capacitor de amostragem (Sample & Hold)
    analogRead(_config.pinoAnalogicoAO);
    delayMicroseconds(500);

    for (uint8_t i = 0; i < _config.qtdAmostrasAdc; i++) {
        soma += analogRead(_config.pinoAnalogicoAO);
        delayMicroseconds(800);
    }

    return (uint16_t)(soma / _config.qtdAmostrasAdc);
}

// ============================================================================
// CLASSIFICAÇÃO E CÁLCULO DE INTENSIDADE
// ============================================================================

EstadoChuva SensorChuvaMHRD::classificarEstado(uint16_t adc, const char* &descricao) {
    if (adc >= _config.adcLimiarSeco) {
        descricao = "Tempo Seco (Sem Chuva)";
        return CHUVA_SECO;
    } else if (adc > _config.adcLimiarChuvaForte) {
        descricao = "Chuva Leve / Garoa";
        return CHUVA_LEVE;
    } else {
        descricao = "Chuva Moderada / Forte";
        return CHUVA_MODERADA_FORTE;
    }
}

float SensorChuvaMHRD::calcularPorcentagem(uint16_t adc) {
    // No sensor MH-RD:
    // Placa Seca: Resistência infinita -> Tensão máxima (ADC ~ 4095 -> 0% de chuva)
    // Placa Encharcada: Resistência baixa -> Tensão mínima (ADC ~ 800 ou menos -> 100% de chuva)
    const float adcSeco = (float)_config.adcLimiarSeco;
    const float adcMolhadoMax = 800.0f; // Ponto típico de saturação de água sobre as trilhas

    if (adc >= adcSeco) {
        return 0.0f;
    }
    if (adc <= adcMolhadoMax) {
        return 100.0f;
    }

    // Interpolação linear invertida: menor ADC = maior intensidade
    float intensidade = ((adcSeco - (float)adc) / (adcSeco - adcMolhadoMax)) * 100.0f;
    return constrain(intensidade, 0.0f, 100.0f);
}

// ============================================================================
// CICLO PRINCIPAL DE LEITURA EFICIENTE
// ============================================================================

LeituraSensorChuva SensorChuvaMHRD::realizarLeitura() {
    LeituraSensorChuva resultado = {};
    unsigned long tInicio = millis();

    // 1. ENERGIZAÇÃO: Ativa o Gate do MOSFET Canal N
    ligarSensor();

    // 2. ESTABILIZAÇÃO: Aguarda estabilizar a alimentação do comparador LM393 e a tensão nas trilhas
    delay(_config.tempoEstabilizacaoMs);

    // 3. AMOSTRAGEM: Leitura analógica (AO) filtrada e digital (DO) simultânea
    resultado.valorAdcBruto = lerAdcFiltrado();
    
    // No comparador LM393 com potenciômetro, o pino DO vai para nível LOW quando detecta chuva (acima do limiar calibrado)
    int valorDigital = digitalRead(_config.pinoDigitalDO);
    resultado.limiarDigitalDisparado = (valorDigital == LOW);

    // 4. DESENERGIZAÇÃO IMEDIATA: Corta a energia desligando o MOSFET
    desligarSensor();

    resultado.tempoSensorLigadoMs = millis() - tInicio;

    // 5. PROCESSAMENTO DOS DADOS COLETADOS
    resultado.tensaoVolts = ((float)resultado.valorAdcBruto * 3.3f) / 4095.0f;
    resultado.porcentagemIntensidade = calcularPorcentagem(resultado.valorAdcBruto);
    resultado.estado = classificarEstado(resultado.valorAdcBruto, resultado.descricaoEstado);

    return resultado;
}

// ============================================================================
// EXIBIÇÃO ORGANIZADA NO MONITOR SERIAL
// ============================================================================

void SensorChuvaMHRD::imprimirLeituraSerial(const LeituraSensorChuva& leitura) {
    Serial.println("\n+-------------------------------------------------------------+");
    Serial.println("|          RELATORIO DE LEITURA - SENSOR DE CHUVA MH-RD       |");
    Serial.println("+-------------------------------------------------------------+");
    
    // Status do Chaveamento
    Serial.printf("| Controle MOSFET (Gate): DESLIGADO (Energizado por apenas %lu ms)\n", (unsigned long)leitura.tempoSensorLigadoMs);
    Serial.println("| Protecao Eletroquimica: Trilhas isoladas (0 uA de corrosao) |");
    Serial.println("+-------------------------------------------------------------+");

    // Medições Físicas
    Serial.printf("| ADC Bruto (12 bits)   : %4u / 4095                         |\n", leitura.valorAdcBruto);
    Serial.printf("| Tensao no Pino AO     : %4.2f V                             |\n", leitura.tensaoVolts);
    Serial.printf("| Intensidade de Chuva  : %5.1f %%                             |\n", leitura.porcentagemIntensidade);
    
    // Saída Digital do Comparador (DO)
    Serial.printf("| Limiar Digital (DO)   : %-36s|\n", 
                  leitura.limiarDigitalDisparado ? "[ATIVADO] Agua acima do limiar" : "[DESATIVADO] Abaixo do limiar");

    // Diagnóstico Qualitativo
    const char* tagEstado = "[ ??? ]";
    if (leitura.estado == CHUVA_SECO) {
        tagEstado = "[ SECO ]";
    } else if (leitura.estado == CHUVA_LEVE) {
        tagEstado = "[ CHUVA LEVE / GAROA ]";
    } else if (leitura.estado == CHUVA_MODERADA_FORTE) {
        tagEstado = "[ CHUVA MODERADA A FORTE ]";
    }

    Serial.printf("| Classificacao Climatica: %-12s %-22s|\n", tagEstado, leitura.descricaoEstado);
    Serial.println("+-------------------------------------------------------------+");
}

// ============================================================================
// CALIBRAÇÃO EM TEMPO DE EXECUÇÃO
// ============================================================================

void SensorChuvaMHRD::setLimiares(uint16_t limiarSeco, uint16_t limiarForte) {
    _config.adcLimiarSeco = limiarSeco;
    _config.adcLimiarChuvaForte = limiarForte;
}

void SensorChuvaMHRD::setTempoEstabilizacao(uint16_t tempoMs) {
    _config.tempoEstabilizacaoMs = tempoMs;
}
