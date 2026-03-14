#include "raylib.h"
#include <vector>
#include <cmath>

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
    bool active;
    ExplosionType currentType;
};

Color GetFireColor(float lifePct) {
    if (lifePct > 0.7f) return ColorLerp(YELLOW, WHITE, (lifePct - 0.7f) * 3.3f);
    if (lifePct > 0.4f) return ColorLerp(ORANGE, YELLOW, (lifePct - 0.4f) * 3.3f);
    return ColorLerp(DARKGRAY, ORANGE, lifePct * 2.5f);
}

Color GetBloodColor(Color base, float lifePct) {
    // Mantém a cor forte por mais tempo
    float alphaBoost = (lifePct > 0.2f) ? 1.0f : lifePct * 5.0f;
    return ColorAlpha(base, alphaBoost);
}

void SpawnExplosion(Explosion &ex, Vector2 p, ExplosionType type) {
    Explosion novaEx = {};
    ex.active = true;
    ex.currentType = type;
    novaEx.particles.clear();

    int count = 150;
    float speedMin = 1.0f, speedMax = 5.0f;
    float sizeMin = 3.0f, sizeMax = 12.0f;
    Color bloodBase = GREEN;

    if (type == LARGE_SHIP) {
        count = 400;
        // VALORES ORIGINAIS GRANDES:
        speedMin = 2.0f; speedMax = 25.0f; 
        sizeMin = 15.0f; sizeMax = 40.0f;
    }
    else if (type == METEOR) {
        count = 100;
        speedMin = 1.0f; speedMax = 6.0f;
    }

    for (int i = 0; i < count; i++) {
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float speed = GetRandomValue(speedMin * 10, speedMax * 10) / 10.0f;
        
        Particle part;
        part.position = { p.x + GetRandomValue(-2, 2), p.y + GetRandomValue(-2, 2) };
        part.velocity = { cosf(angle) * speed, sinf(angle) * speed };
        part.lifePct = 1.0f;
        // Naves grandes duram mais (decay mais lento)
        part.decayRate = (type == LARGE_SHIP) ? GetRandomValue(40, 80) / 100.0f : GetRandomValue(80, 150) / 100.0f;
        part.isAdditive = true;
        part.size = GetRandomValue(sizeMin, sizeMax);
        novaEx.particles.push_back(part);
    }

    // Variações específicas de MATÉRIA (não usam blend aditivo)
    if (type == METEOR || type == MONSTER) {
        int extraCount = (type == METEOR) ? 60 : 200;
        if (type == MONSTER) {
            int r = GetRandomValue(0, 2);
            bloodBase = (r == 0) ? GREEN : (r == 1 ? BLUE : PURPLE);
        }

        for (int i = 0; i < extraCount; i++) {
            float angle = GetRandomValue(0, 360) * DEG2RAD;
            float speed = GetRandomValue(2, 12);
            Particle mat;
            mat.position = p;
            mat.velocity = { cosf(angle) * speed, sinf(angle) * speed };
            mat.lifePct = 1.0f;
            mat.decayRate = GetRandomValue(30, 70) / 100.0f; // Matéria dura MUITO mais
            mat.isAdditive = false;
            
            if (type == METEOR) {
                mat.color = (GetRandomValue(0, 1) == 0) ? GRAY : DARKGRAY;
                mat.size = GetRandomValue(2, 6);
            } else {
                int var = GetRandomValue(0, 2);
                mat.color = (var == 0) ? bloodBase : (var == 1 ? ColorBrightness(bloodBase, 0.4f) : ColorBrightness(bloodBase, -0.4f));
                mat.size = GetRandomValue(2, 5);
            }
            novaEx.particles.push_back(mat);
        }
    }
    explosoesAtivas.push_back(novaEx);
}

int main() {
    InitWindow(800, 600, "MVP Space Shooter - Explosões");
    SetTargetFPS(60);

    Explosion ex = {}; ex.active = false;
    ExplosionType selectedType = SMALL_SHIP;

    Image img = GenImageGradientRadial(64, 64, 0.0f, WHITE, BLACK);
    Texture2D particleTex = LoadTextureFromImage(img);
    UnloadImage(img);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ONE)) selectedType = SMALL_SHIP;
        if (IsKeyPressed(KEY_TWO)) selectedType = LARGE_SHIP;
        if (IsKeyPressed(KEY_THREE)) selectedType = METEOR;
        if (IsKeyPressed(KEY_FOUR)) selectedType = MONSTER;
        if (IsKeyPressed(KEY_SPACE)) SpawnExplosion(ex, {(float)GetMouseX(), (float)GetMouseY()}, selectedType);

        if (ex.active) {
            int alive = 0;
            for (auto &p : novaEx.particles) {
                if (p.lifePct > 0) {
                    p.position.x += p.velocity.x;
                    p.position.y += p.velocity.y;
                    
                    // --- FÍSICA DIFERENCIADA ---
                    // Fogo (Additive) tem muito atrito. Rocha/Sangue (Sólido) voa longe.
                    float friction = (p.isAdditive) ? 0.90f : 0.98f;
                    p.velocity.x *= friction;
                    p.velocity.y *= friction;

                    if (p.isAdditive) p.size += 0.3f; 
                    p.lifePct -= p.decayRate * GetFrameTime();
                    alive++;
                }
            }
            if (alive == 0) ex.active = false;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        if (ex.active) {
            // 1. Matéria (Sólidos) - Agora com Alpha mais forte
            for (auto &p : novaEx.particles) {
                if (p.lifePct > 0 && !p.isAdditive) {
                    Color c = (ex.currentType == MONSTER) ? GetBloodColor(p.color, p.lifePct) : ColorAlpha(p.color, p.lifePct > 0.15f ? 1.0f : p.lifePct * 6.0f);
                    DrawCircleV(p.position, p.size, c);
                }
            }
            // 2. Energia (Brilho)
            BeginBlendMode(BLEND_ADDITIVE);
            for (auto &p : novaEx.particles) {
                if (p.lifePct > 0 && p.isAdditive) {
                    DrawTexturePro(particleTex, {0,0,64,64}, {p.position.x, p.position.y, p.size, p.size}, {p.size/2, p.size/2}, 0.0f, ColorAlpha(GetFireColor(p.lifePct), p.lifePct));
                }
            }
            EndBlendMode();
        }

        DrawText("1:Pequena 2:Grande 3:Meteoro 4:Monstro", 10, 10, 20, RAYWHITE);
        EndDrawing();
    }
    UnloadTexture(particleTex);
    CloseWindow();
    return 0;
}