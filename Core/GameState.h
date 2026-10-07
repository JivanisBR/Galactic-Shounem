#pragma once

class GameManager; // Aviso prévio para evitar dependência circular

class GameState {
protected:
    GameManager* game; // Permite que o estado acesse o Player e a Galáxia
public:
    virtual ~GameState() {}
    
    // Funções que todo modo de jogo terá que ter
    virtual void Entrar(GameManager* gm) = 0;
    virtual void Atualizar(float dt) = 0;
    virtual void Desenhar() = 0;
    virtual void Sair() = 0;
};