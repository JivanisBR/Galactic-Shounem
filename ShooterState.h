#pragma once
#include "../Core/GameState.h"
#include "boss.h"
#include "../Shared/Player.h"
#include "../Shared/explosao.h"

class ShooterState : public GameState {
public:
    void Entrar(GameManager* gm) override;
    void Atualizar(float dt) override;
    void Desenhar() override;
    void Sair() override;
};