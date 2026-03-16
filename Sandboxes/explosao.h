#ifndef EXPLOSAO_H
#define EXPLOSAO_H

#include "raylib.h"
#include <vector>

// Categorias de explosão
enum ExplosionType { SMALL_SHIP, LARGE_SHIP, METEOR, MONSTER };

struct Particle {
    Vector2 position;
    Vector2 velocity;
    float size;
    float lifePct;
    float decayRate;
    Color color;
    bool isAdditive;
};

struct Explosion {
    std::vector<Particle> particles;
    ExplosionType type;
    bool active;
};

// Funções que o seu jogo vai usar
void AdicionarExplosao(Vector2 p, ExplosionType type);
void UpdateExplosoes(float deltaTime);
void DrawExplosoes(Texture2D particleTex);

#endif