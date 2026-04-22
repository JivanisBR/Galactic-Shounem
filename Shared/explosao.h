#pragma once
#include "raylib.h"
#include <vector>

enum ExplosionType { SMALL_SHIP, LARGE_SHIP, METEOR, MONSTER };

struct Particula {
    Vector2 position;
    Vector2 velocity;
    float size;
    float lifePct;
    float decayRate;
    Color color;
    bool isAdditive; 
};

struct ExplosionObj {
    std::vector<Particula> particles;
    bool active;
    ExplosionType currentType;
};

class GerenciadorDeExplosoes {
private:
    std::vector<ExplosionObj> explosoesAtivas;
    Texture2D texParticula;

    Color GetFireColor(float lifePct);
    Color GetBloodColor(Color base, float lifePct);

public:
    GerenciadorDeExplosoes();
    ~GerenciadorDeExplosoes();

    void Inicializar(Texture2D textura);
    void AdicionarExplosao(Vector2 p, ExplosionType type);
    void Atualizar(float dt, float velocidadeMundo);
    void Desenhar();
};