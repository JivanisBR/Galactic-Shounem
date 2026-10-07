#pragma once
#include "raylib.h"
#include "../Shared/Player.h"
#include "GameState.h"
#include <vector>

class GameManager {
private:
    GameState* estadoAtual;
    GameState* proximoEstado;

public:
    Player* jogador;
    // std::vector<Estrela> galaxia; <-- Adicionaremos isso na Fase 3!

    GameManager();
    ~GameManager();

    void MudarEstado(GameState* novoEstado);
    void Executar(); // O verdadeiro loop do jogo
};