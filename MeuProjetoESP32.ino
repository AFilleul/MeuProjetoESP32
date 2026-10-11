#include <Keypad.h>
#include "TecladoMatriz.h"

// Definição dos pinos seguros para o ESP32 Wemos Lolin
// Linhas: GPIO 13, 14, 25, 26
const byte pinosLinhas[4] = {16, 17, 5, 18};

// Colunas: GPIO 27, 32, 33, 4
const byte pinosColunas[4] = {27, 26, 25, 33};

// Instancia o objeto do teclado modularizado
TecladoMatriz teclado(pinosLinhas, pinosColunas);

void autoDiagnosticoPinos() {
    Serial.println("\n--- [AUTO-DIAGNOSTICO ELETRICO DO TECLADO] ---");

    // FASE 1: Testa se algum pino individual esta em curto com GND
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

    // FASE 2: Testa cruzamento entre Linhas e Colunas (detecta teclas travadas ou curto entre fios)
    Serial.println("  [Fase 2] Testando cruzamentos (Linhas x Colunas)...");
    bool contatoEncontrado = false;

    const char mapaTeclas[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    // Coloca todas as linhas em INPUT_PULLUP
    for (int r = 0; r < 4; r++) {
        pinMode(pinosLinhas[r], INPUT_PULLUP);
    }

    // Coloca uma coluna de cada vez em LOW e verifica se alguma linha vai para LOW
    for (int c = 0; c < 4; c++) {
        // Desativa as outras colunas
        for (int other = 0; other < 4; other++) {
            pinMode(pinosColunas[other], INPUT_PULLUP);
        }

        // Puxa a coluna atual para LOW
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

    // Restaura todas as colunas para INPUT_PULLUP
    for (int c = 0; c < 4; c++) {
        pinMode(pinosColunas[c], INPUT_PULLUP);
    }

    if (contatoEncontrado) {
        Serial.println("----------------------------------------------------------------");
        Serial.println("RESULTADO DO DIAGNOSTICO:");
        Serial.println("O ESP32 detectou contato eletrico direto entre a Linha e Coluna acima.");
        Serial.println("Como nenhuma tecla foi pressionada, as causas sao:");
        Serial.println("  1) Curto entre fios vizinhos na protoboard (ex: GPIO 14 e GPIO 27);");
        Serial.println("  2) A tecla fisica da membrana esta emperrada/danificada;");
        Serial.println("  3) Desconecte o teclado: se o contato sumir, o defeito e no teclado.");
        Serial.println("----------------------------------------------------------------\n");
    } else {
        Serial.println("  [Fase 2] OK: Nenhum cruzamento/tecla travada detectada.\n");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n--- Sistema Inicializado ---");

    // Executa o diagnóstico elétrico dos pinos antes de ativar a biblioteca Keypad
    autoDiagnosticoPinos();

    // Inicializa o teclado modularizado
    teclado.iniciar();

    Serial.println("Pressione qualquer tecla no teclado 4x4...");
}

void loop() {
// Lê a tecla pressionada de forma não bloqueante
char teclaPressionada = teclado.lerTecla();

if (teclaPressionada != '\0') {
Serial.print("Tecla pressionada: ");
Serial.println(teclaPressionada);
}

// Pequena pausa para estabilidade do loop
delay(20);
}