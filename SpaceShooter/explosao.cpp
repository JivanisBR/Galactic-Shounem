#include "explosao.h"
#include <cmath>

// Lista global interna (escondida do main)
std::vector<Explosion> listaExplosoes;

// Auxiliares de cor
Color GetFireColor(float lifePct) {
    if (lifePct > 0.7f) return ColorLerp(YELLOW, WHITE, (lifePct - 0.7f) * 3.3f);
    if (lifePct > 0.4f) return ColorLerp(ORANGE, YELLOW, (lifePct - 0.4f) * 3.3f);
    return ColorLerp(DARKGRAY, ORANGE, lifePct * 2.5f);
}

void AdicionarExplosao(Vector2 p, ExplosionType type) {
    Explosion novaEx;
    novaEx.active = true;
    novaEx.type = type;

    int count = 150;
    float sMin = 1.0f, sMax = 5.0f;
    float szMin = 3.0f, szMax = 12.0f;

    if (type == LARGE_SHIP) { count = 400; sMin = 2.0f; sMax = 25.0f; szMin = 15.0f; szMax = 40.0f; }
    
    // Gera partículas de Fogo/Energia
    for (int i = 0; i < count; i++) {
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float speed = GetRandomValue(sMin * 10, sMax * 10) / 10.0f;
        Particle part = { p, {cosf(angle) * speed, sinf(angle) * speed}, (float)GetRandomValue(szMin, szMax), 1.0f, 
                          (type == LARGE_SHIP ? GetRandomValue(40, 80)/100.0f : GetRandomValue(80, 150)/100.0f), WHITE, true };
        novaEx.particles.push_back(part);
    }

    // Gera partículas de Matéria (Rocha/Sangue)
    if (type == METEOR || type == MONSTER) {
        int mCount = (type == METEOR) ? 60 : 200;
        Color bloodBase = (GetRandomValue(0, 1) == 0) ? GREEN : BLUE;
        for (int i = 0; i < mCount; i++) {
            float angle = GetRandomValue(0, 360) * DEG2RAD;
            Particle mat = { p, {cosf(angle) * 8.0f, sinf(angle) * 8.0f}, (float)GetRandomValue(2, 6), 1.0f, GetRandomValue(30, 70)/100.0f, GRAY, false };
            if (type == MONSTER) mat.color = ColorBrightness(bloodBase, GetRandomValue(-3, 3)/10.0f);
            novaEx.particles.push_back(mat);
        }
    }
    listaExplosoes.push_back(novaEx);
}

void UpdateExplosoes(float dt) {
    for (int i = 0; i < listaExplosoes.size(); i++) {
        bool alive = false;
        for (auto &p : listaExplosoes[i].particles) {
            if (p.lifePct > 0) {
                p.position.x += p.velocity.x; p.position.y += p.velocity.y;
                float fric = p.isAdditive ? 0.90f : 0.98f;
                p.velocity.x *= fric; p.velocity.y *= fric;
                if (p.isAdditive) p.size += 0.3f;
                p.lifePct -= p.decayRate * dt;
                alive = true;
            }
        }
        if (!alive) { listaExplosoes.erase(listaExplosoes.begin() + i); i--; }
    }
}

void DrawExplosoes(Texture2D tex) {
    // 1. Desenha Sólidos
    for (auto &ex : listaExplosoes) {
        for (auto &p : ex.particles) {
            if (p.lifePct > 0 && !p.isAdditive) {
                float a = (p.lifePct > 0.15f) ? 1.0f : p.lifePct * 6.0f;
                DrawCircleV(p.position, p.size, ColorAlpha(p.color, a));
            }
        }
    }
    // 2. Desenha Fogo (Additive)
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto &ex : listaExplosoes) {
        for (auto &p : ex.particles) {
            if (p.lifePct > 0 && p.isAdditive) {
                DrawTexturePro(tex, {0,0,64,64}, {p.position.x, p.position.y, p.size, p.size}, {p.size/2, p.size/2}, 0.0f, ColorAlpha(GetFireColor(p.lifePct), p.lifePct));
            }
        }
    }
    EndBlendMode();
}