#include "Explosao.h"
#include <cmath>

GerenciadorDeExplosoes::GerenciadorDeExplosoes() {}
GerenciadorDeExplosoes::~GerenciadorDeExplosoes() {}

void GerenciadorDeExplosoes::Inicializar(Texture2D textura) {
    texParticula = textura;
    explosoesAtivas.clear();
}

Color GerenciadorDeExplosoes::GetFireColor(float lifePct) {
    if (lifePct > 0.7f) return ColorLerp(YELLOW, WHITE, (lifePct - 0.7f) * 3.3f);
    if (lifePct > 0.4f) return ColorLerp(ORANGE, YELLOW, (lifePct - 0.4f) * 3.3f);
    return ColorLerp(DARKGRAY, ORANGE, lifePct * 2.5f);
}

Color GerenciadorDeExplosoes::GetBloodColor(Color base, float lifePct) {
    float alphaBoost = (lifePct > 0.2f) ? 1.0f : lifePct * 5.0f;
    return ColorAlpha(base, alphaBoost);
}

void GerenciadorDeExplosoes::AdicionarExplosao(Vector2 p, ExplosionType type) {
    ExplosionObj novaEx = {};
    novaEx.active = true;
    novaEx.currentType = type;

    int count = 150;
    float speedMin = 1.0f, speedMax = 5.0f;
    float sizeMin = 3.0f, sizeMax = 12.0f;
    Color bloodBase = GREEN;

    // Resgatando as configurações ORIGINAIS do Sandbox
    if (type == LARGE_SHIP) {
        count = 400;
        speedMin = 2.0f; speedMax = 25.0f; 
        sizeMin = 15.0f; sizeMax = 40.0f;
    } else if (type == METEOR) {
        count = 100;
        speedMin = 1.0f; speedMax = 6.0f;
    }

    for (int i = 0; i < count; i++) {
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float speed = GetRandomValue(speedMin * 10, speedMax * 10) / 10.0f;
        
        Particula part;
        part.position = { p.x + GetRandomValue(-2, 2), p.y + GetRandomValue(-2, 2) };
        part.velocity = { cosf(angle) * speed, sinf(angle) * speed };
        part.lifePct = 1.0f;
        part.decayRate = (type == LARGE_SHIP) ? GetRandomValue(40, 80) / 100.0f : GetRandomValue(80, 150) / 100.0f;
        part.isAdditive = true;
        part.size = GetRandomValue(sizeMin, sizeMax);
        part.color = WHITE;
        novaEx.particles.push_back(part);
    }

    if (type == METEOR || type == MONSTER) {
        int extraCount = (type == METEOR) ? 60 : 200;
        if (type == MONSTER) {
            int r = GetRandomValue(0, 2);
            bloodBase = (r == 0) ? GREEN : (r == 1 ? BLUE : PURPLE);
        }

        for (int i = 0; i < extraCount; i++) {
            float angle = GetRandomValue(0, 360) * DEG2RAD;
            float speed = GetRandomValue(2, 12);
            Particula mat;
            mat.position = p;
            mat.velocity = { cosf(angle) * speed, sinf(angle) * speed };
            mat.lifePct = 1.0f;
            mat.decayRate = GetRandomValue(30, 70) / 100.0f; 
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

void GerenciadorDeExplosoes::Atualizar(float dt, float velocidadeMundo) {
    // Normaliza o dt para simular os 60FPS do sandbox independente do framerate do PC
    float fatorTempo = dt * 60.0f; 

    for (int i = 0; i < (int)explosoesAtivas.size(); i++) {
        int aliveCount = 0;
        for (auto &p : explosoesAtivas[i].particles) {
            if (p.lifePct > 0) {
                // Física PURA do sandbox
                p.position.x += p.velocity.x * fatorTempo;
                p.position.y += p.velocity.y * fatorTempo;
                
                // Atrito original (90% pra fogo, 98% pra pedras)
                float friction = (p.isAdditive) ? 0.90f : 0.98f;
                // Aplicamos o atrito ajustado ao tempo
                p.velocity.x *= pow(friction, fatorTempo);
                p.velocity.y *= pow(friction, fatorTempo);

                // Efeito do cenário "deixando a explosão pra trás"
                p.position.y += velocidadeMundo * dt * 20.0f;

                if (p.isAdditive) p.size += 0.3f * fatorTempo; 
                p.lifePct -= p.decayRate * dt; 
                aliveCount++;
            }
        }
        if (aliveCount == 0) {
            explosoesAtivas.erase(explosoesAtivas.begin() + i);
            i--;
        }
    }
}

void GerenciadorDeExplosoes::Desenhar() {
    for (auto &ex : explosoesAtivas) {
        for (auto &p : ex.particles) {
            if (p.lifePct > 0 && !p.isAdditive) {
                Color c = (ex.currentType == MONSTER) ? GetBloodColor(p.color, p.lifePct) : ColorAlpha(p.color, p.lifePct > 0.15f ? 1.0f : p.lifePct * 6.0f);
                DrawCircleV(p.position, p.size, c);
            }
        }
    }
    
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto &ex : explosoesAtivas) {
        for (auto &p : ex.particles) {
            if (p.lifePct > 0 && p.isAdditive) {
                DrawTexturePro(texParticula, {0,0,64,64}, {p.position.x, p.position.y, p.size, p.size}, {p.size/2, p.size/2}, 0.0f, ColorAlpha(GetFireColor(p.lifePct), p.lifePct));
            }
        }
    }
    EndBlendMode();
}