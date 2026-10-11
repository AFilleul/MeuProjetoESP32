#ifndef TECLADO_MATRIZ_H
#define TECLADO_MATRIZ_H

#include <Arduino.h>
#include <Keypad.h>

class TecladoMatriz {
private:
static const byte LINHAS = 4;
static const byte COLUNAS = 4;

char teclasMatriz[LINHAS][COLUNAS];
byte pinosLinhas[LINHAS];
byte pinosColunas[COLUNAS];

Keypad* keypadCustom;


public:
    TecladoMatriz(const byte rPins[], const byte cPins[]);
    ~TecladoMatriz();

    // Proíbe cópia para evitar desalocação indevida de ponteiros internos
    TecladoMatriz(const TecladoMatriz&) = delete;
    TecladoMatriz& operator=(const TecladoMatriz&) = delete;

    void iniciar();
    char lerTecla();
};

#endif