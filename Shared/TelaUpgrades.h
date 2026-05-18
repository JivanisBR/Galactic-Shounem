#pragma once
#include "raylib.h"
#include "Nave.h"
#include <string>
#include <vector>

struct BotaoUpgrade {
    Rectangle rect;
    Color corBase;
    Color corHover;
    std::string nome;
    int* levelAtual;
    void (*AcaoUpgrade)(Nave&);
};

class TelaUpgrades {
private:
    int screenWidth;
    int screenHeight;
    float msgTimer;
    std::string msgTexto;
    std::vector<BotaoUpgrade> botoes;
    Rectangle btnReset;

public:
    TelaUpgrades(int sw, int sh, Nave* naveRef);
    void AtualizarEDesenhar(Nave* naveRef);
};