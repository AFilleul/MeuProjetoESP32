#include "TecladoMatriz.h"

TecladoMatriz::TecladoMatriz(const byte rPins[], const byte cPins[]) : keypadCustom(nullptr) {
    // Mapeamento padrão do teclado matricial 4x4
    const char teclasDefinidas[LINHAS][COLUNAS] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    // Copia os valores das teclas e pinos para as variáveis da classe
    for (int i = 0; i < LINHAS; i++) {
        this->pinosLinhas[i] = rPins[i];
        for (int j = 0; j < COLUNAS; j++) {
            this->teclasMatriz[i][j] = teclasDefinidas[i][j];
        }
    }

    for (int j = 0; j < COLUNAS; j++) {
        this->pinosColunas[j] = cPins[j];
    }
}

TecladoMatriz::~TecladoMatriz() {
    if (keypadCustom != nullptr) {
        delete keypadCustom;
        keypadCustom = nullptr;
    }
}

void TecladoMatriz::iniciar() {
    // Evita vazamento de memória caso iniciar() seja chamado mais de uma vez
    if (keypadCustom != nullptr) {
        delete keypadCustom;
        keypadCustom = nullptr;
    }

    // Aloca o objeto da biblioteca Keypad com o mapa e os pinos configurados
    keypadCustom = new Keypad(
        makeKeymap(teclasMatriz),
        pinosLinhas,
        pinosColunas,
        LINHAS,
        COLUNAS
    );
}

char TecladoMatriz::lerTecla() {
if (keypadCustom != nullptr) {
// Retorna a tecla se pressionada, ou '\0' se nada for acionado
return keypadCustom->getKey();
}
return '\0';
}