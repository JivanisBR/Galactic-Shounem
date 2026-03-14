#include "raylib.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm> // Para random shuffle se precisasse, mas usaremos GetRandomValue

//----------------------------------------------------------------------------------
// ESTRUTURAS DE DADOS
//----------------------------------------------------------------------------------

struct Poeira {
    Vector2 pos;
    Color cor;
};

struct Estrela {
    Vector2 pos;
    int tam_nucleo;     // Tamanho da estrela branca fisica
    int atraso_vel;
    int cont_mov;
    
    // Animação
    bool crescendo;
    int timer_anim;
    int limite_anim;
    int tam_base;
    int alcance_cresc;

    // --- NOVOS DADOS DE GAMEPLAY (RPG) ---
    bool tem_chefe;
    std::string nome_chefe; // Nome do inimigo ou "Desabitado"
    float diametro_aura;    // Tamanho visual do Ki
    Color cor_aura;         // Cor do Ki
    int nivel_poder;        // Nível numérico para UI
    bool eh_lendario;       // Se caiu no 95-100
};

struct ParticulaNebulosa {
    Vector2 offset;
    float larg;
    float alt;
    Color cor;
    // Simplifiquei a nebulosa para focar no gameplay, removi estrelas internas da nebulosa 
    // para não confundir com as fases jogáveis
};

struct Nebulosa {
    Vector2 pos;
    Color cor_base;
    int atraso_vel;
    int cont_mov;
    std::vector<ParticulaNebulosa> particulas;
};

//----------------------------------------------------------------------------------
// BANCO DE NOMES
//----------------------------------------------------------------------------------
const std::vector<std::string> NOMES_BASE = {
    "Grom", "Xylo", "Zarkon", "Vazio", "Nebula", 
    "Kael", "Drako", "Solaris", "Voraz", "Espectro", 
    "Titã", "Chronos", "Lumina", "Umbra", "Ion"
};

//----------------------------------------------------------------------------------
// FUNÇÕES AUXILIARES
//----------------------------------------------------------------------------------

// Gera um nome simples ou composto
std::string GerarNomeProcedural() {
    std::string nome = NOMES_BASE[GetRandomValue(0, NOMES_BASE.size() - 1)];
    
    // 50% de chance de ser nome composto
    if (GetRandomValue(0, 100) < 50) {
        std::string segundo_nome = NOMES_BASE[GetRandomValue(0, NOMES_BASE.size() - 1)];
        if (nome != segundo_nome) { // Evita "Grom Grom"
            nome += " " + segundo_nome;
        }
    }
    return nome;
}

// Configura os dados de RPG da estrela (Roda quando nasce ou recicla)
void ConfigurarEstrelaRPG(Estrela& e) {
    int chance = GetRandomValue(0, 100);

    // Reset basico
    e.tem_chefe = false;
    e.eh_lendario = false;
    e.diametro_aura = 0;
    e.nome_chefe = "Setor Vazio";
    e.nivel_poder = GetRandomValue(1, 5); // Nivel basico da fase
    e.cor_aura = BLANK;

    // Lógica de Spawn do KI
    if (chance < 70) {
        // Estrela Comum (Sem chefe)
        e.tem_chefe = false;
    } 
    else {
        // TEM CHEFE
        e.tem_chefe = true;
        e.nome_chefe = GerarNomeProcedural();
        
        // Cor aleatória vibrante para a aura
        e.cor_aura = ColorFromHSV((float)GetRandomValue(0, 360), 0.8f, 0.9f); 

        if (chance >= 70 && chance < 95) {
            // Nível Médio
            e.diametro_aura = (float)GetRandomValue(30, 300); // 10 a 300 era mto pouco pro visual, ajustei min pra 30
            e.nivel_poder = (int)e.diametro_aura * 100;
        } 
        else {
            // Nível DIFÍCIL / LENDÁRIO (95-100)
            e.eh_lendario = true;
            e.diametro_aura = (float)GetRandomValue(301, 800);
            e.nivel_poder = (int)e.diametro_aura * 500;
            e.nome_chefe = "Imperador " + e.nome_chefe; // Dá um título
        }
    }
}

void DrawStarShape(Vector2 centro, float tamanho, Color cor)
{
    if (tamanho < 1.0f) return;

    // Desenha uma estrela de 4 pontas estilo "brilho"
    // Pontas verticais/horizontais finas
    DrawTriangle(
        {centro.x, centro.y - tamanho}, 
        {centro.x - tamanho * 0.2f, centro.y}, 
        {centro.x + tamanho * 0.2f, centro.y}, 
        cor
    );
    DrawTriangle(
        {centro.x, centro.y + tamanho}, 
        {centro.x + tamanho * 0.2f, centro.y}, 
        {centro.x - tamanho * 0.2f, centro.y}, 
        cor
    );
    DrawTriangle(
        {centro.x - tamanho, centro.y}, 
        {centro.x, centro.y + tamanho * 0.2f}, 
        {centro.x, centro.y - tamanho * 0.2f}, 
        cor
    );
    DrawTriangle(
        {centro.x + tamanho, centro.y}, 
        {centro.x, centro.y - tamanho * 0.2f}, 
        {centro.x, centro.y + tamanho * 0.2f}, 
        cor
    );
    
    // Centro brilhante solido
    DrawCircleV(centro, tamanho * 0.15f, cor);
}

void GenerateNebulaShape(Nebulosa& neb, int cont_particulas) {
    if (cont_particulas <= 0) return;
    neb.particulas.resize(cont_particulas);

    int particulas_desenhadas = 0;
    int num_bracos = 3;
    int particulas_por_braco = cont_particulas / num_bracos;
    
    for (int b = 0; b < num_bracos; ++b) {
        Vector2 walker = {0, 0};
        Vector2 direcao = {0, 0};
        int passos_restantes = 0;

        for (int p = 0; p < particulas_por_braco; ++p) {
            if (particulas_desenhadas >= cont_particulas) break;

            if (passos_restantes <= 0) {
                passos_restantes = GetRandomValue(5, 15);
                direcao = {(float)GetRandomValue(-1, 1), (float)GetRandomValue(-1, 1)};
            }

            walker.x += direcao.x * GetRandomValue(2, 5);
            walker.y += direcao.y * GetRandomValue(2, 5);
            passos_restantes--;

            int idx = particulas_desenhadas++;
            neb.particulas[idx].offset = {walker.x + GetRandomValue(-10, 10), walker.y + GetRandomValue(-10, 10)};
            neb.particulas[idx].larg = GetRandomValue(30, 80);
            neb.particulas[idx].alt = GetRandomValue(30, 80);
            unsigned char opacidade = GetRandomValue(5, 15); // Bem sutil no fundo
            neb.particulas[idx].cor = {neb.cor_base.r, neb.cor_base.g, neb.cor_base.b, opacidade};
        }
    }
}

//----------------------------------------------------------------------------------
// FUNÇÃO PRINCIPAL
//----------------------------------------------------------------------------------
int main(void)
{
    const int screenWidth = 1200;
    const int screenHeight = 700;
    
    // CONFIGURAÇÃO DRASTICA DE QUANTIDADE
    // Menos estrelas = Mais importância para cada "sala"
    const int CONT_POEIRA = 200;
    const int CONT_ESTRELAS_FUNDO = 40; 
    const int CONT_ESTRELAS_FRENTE = 12; // Apenas 12 fases visíveis por vez para não poluir
    const int CONT_NEBULOSAS = 3;
    
    InitWindow(screenWidth, screenHeight, "Mapa Estelar - Detecção de Ki");
    SetTargetFPS(60);

    // Inicialização Vetores
    std::vector<Poeira> poeira(CONT_POEIRA);
    std::vector<Estrela> estrelas_fundo(CONT_ESTRELAS_FUNDO);
    std::vector<Estrela> estrelas_frente(CONT_ESTRELAS_FRENTE);
    std::vector<Nebulosa> nebulosas(CONT_NEBULOSAS);

    std::vector<Color> cores_nebulosa = {DARKPURPLE, DARKBLUE, {50, 0, 0, 255}};

    // Textura estática para poeira (otimização)
    RenderTexture2D textura_poeira = LoadRenderTexture(screenWidth, screenHeight);
    BeginTextureMode(textura_poeira);
    ClearBackground(BLANK);
    for (int i = 0; i < CONT_POEIRA; ++i) {
        Vector2 p = {(float)GetRandomValue(0, screenWidth), (float)GetRandomValue(0, screenHeight)};
        DrawPixelV(p, {200, 200, 200, 100});
    }
    EndTextureMode();

    // Setup Estrelas Fundo (Decorativo)
    for (auto& e : estrelas_fundo) {
        e.pos = {(float)GetRandomValue(0, screenWidth), (float)GetRandomValue(0, screenHeight)};
        e.atraso_vel = GetRandomValue(15, 30);
        e.tam_nucleo = GetRandomValue(1, 2);
        e.cont_mov = 0;
    }

    // Setup Estrelas Frente (GAMEPLAY - INTERATIVAS)
    for (auto& e : estrelas_frente) {
        e.pos = {(float)GetRandomValue(0, screenWidth), (float)GetRandomValue(0, screenHeight)};
        e.atraso_vel = GetRandomValue(2, 6); // Mais rápidas, "passando" pela nave
        e.tam_base = GetRandomValue(4, 7);
        e.tam_nucleo = e.tam_base;
        e.cont_mov = GetRandomValue(0, e.atraso_vel);
        
        // Animação de pulso
        e.crescendo = true;
        e.limite_anim = GetRandomValue(5, 10);
        e.timer_anim = 0;
        e.alcance_cresc = 4;

        // --- GERAÇÃO PROCEDURAL DO INIMIGO ---
        ConfigurarEstrelaRPG(e);
    }

    // Setup Nebulosas
    for (auto& n : nebulosas) {
        n.pos = {(float)GetRandomValue(0, screenWidth), (float)GetRandomValue(0, screenHeight)};
        n.atraso_vel = GetRandomValue(10, 20);
        n.cor_base = cores_nebulosa[GetRandomValue(0, cores_nebulosa.size() - 1)];
        GenerateNebulaShape(n, 150);
    }

    // Loop Principal
    while (!WindowShouldClose()) {
        
        Vector2 mousePos = GetMousePosition();
        bool turbo_on = IsKeyDown(KEY_SPACE);
        float velocidade_scroll = turbo_on ? 5.0f : 0.5f;

        // --- ATUALIZAÇÃO ---

        // Fundo
        for (auto& e : estrelas_fundo) {
            e.pos.y += (1.0f / e.atraso_vel) + (turbo_on ? 2.0f : 0.0f);
            if(e.pos.y > screenHeight) e.pos = {(float)GetRandomValue(0, screenWidth), -5.0f};
        }

        // Nebulosas
        for (auto& n : nebulosas) {
            n.pos.y += (1.0f / n.atraso_vel) + (turbo_on ? 3.0f : 0.0f);
            if(n.pos.y > screenHeight + 200) {
                n.pos = {(float)GetRandomValue(0, screenWidth), -200.0f};
                n.cor_base = cores_nebulosa[GetRandomValue(0, cores_nebulosa.size() - 1)];
                GenerateNebulaShape(n, 150);
            }
        }

        // FRENTE (Gameplay)
        for (auto& e : estrelas_frente) {
            // Movimento
            float speed = (30.0f / e.atraso_vel) * (turbo_on ? 2.0f : 0.2f);
            e.pos.y += speed;

            // RECICLAGEM E GERAÇÃO DE NOVOS INIMIGOS
            if(e.pos.y > screenHeight + e.diametro_aura) { 
                e.pos = {(float)GetRandomValue(50, screenWidth - 50), -e.diametro_aura - 20.0f};
                // RODA A ROLETA NOVAMENTE
                ConfigurarEstrelaRPG(e);
            }

            // Animação de pulso da estrela física
            e.timer_anim++;
            if (e.timer_anim >= e.limite_anim) {
                e.timer_anim = 0;
                if (e.crescendo) { 
                    if (e.tam_nucleo < e.tam_base + e.alcance_cresc) e.tam_nucleo++; 
                    else e.crescendo = false; 
                } else { 
                    if (e.tam_nucleo > e.tam_base) e.tam_nucleo--; 
                    else e.crescendo = true; 
                }
            }
        }

        // --- DESENHO ---
        BeginDrawing();
        ClearBackground(BLACK);

        DrawTexture(textura_poeira.texture, 0, 0, WHITE);

        // 1. Nebulosas (Fundo distante)
        for (const auto& n : nebulosas) {
            for (const auto& p : n.particulas) {
                DrawEllipse(n.pos.x + p.offset.x, n.pos.y + p.offset.y, p.larg, p.alt, p.cor);
            }
        }

        // 2. Estrelas Decorativas
        for (const auto& e : estrelas_fundo) {
            DrawPixelV(e.pos, {150, 150, 150, 255});
        }

        // 3. AURAS DE KI (Modo Blend ADD para brilhar)
        BeginBlendMode(BLEND_ADD);
        for (const auto& e : estrelas_frente) {
            if (e.tem_chefe) {
                // A cor da aura com opacidade fixa ~130 (0.5f)
                Color auraColor = ColorAlpha(e.cor_aura, 0.5f);
                
                // Desenha a aura (proporcional ao poder)
                // Fazemos ela pulsar levemente baseada no tempo
                float pulso = sin(GetTime() * 5.0f) * (e.diametro_aura * 0.05f);
                DrawStarShape(e.pos, (e.diametro_aura / 2) + pulso, auraColor);
                
                // Se for lendário, adiciona um segundo núcleo mais quente
                if (e.eh_lendario) {
                     DrawStarShape(e.pos, (e.diametro_aura / 4), WHITE);
                }
            }
        }
        EndBlendMode();

        // 4. Núcleo Físico da Estrela (Fase)
        for (const auto& e : estrelas_frente) {
            DrawStarShape(e.pos, e.tam_nucleo, WHITE);
        }

        // 5. UI / DETECÇÃO (Mouse Hover)
        // Desenhamos por último para ficar em cima de tudo
        for (const auto& e : estrelas_frente) {
            // Colisão simples circular (considerando o tamanho visual da aura para facilitar o clique)
            float raio_detect = e.tem_chefe ? (e.diametro_aura / 2) : 20.0f;
            if (raio_detect < 20.0f) raio_detect = 20.0f; // Mínimo para clicar

            if (CheckCollisionPointCircle(mousePos, e.pos, raio_detect)) {
                
                // Desenha a linha de "Target"
                DrawLineV(e.pos, mousePos, GREEN);
                DrawCircleV(e.pos, raio_detect, Fade(GREEN, 0.2f));

                // Painel de Info
                int boxW = 220;
                int boxH = 100;
                int boxX = mousePos.x + 15;
                int boxY = mousePos.y + 15;

                // Ajuste para não sair da tela
                if (boxX + boxW > screenWidth) boxX -= boxW + 30;
                if (boxY + boxH > screenHeight) boxY -= boxH + 30;

                DrawRectangle(boxX, boxY, boxW, boxH, Fade(BLACK, 0.8f));
                DrawRectangleLines(boxX, boxY, boxW, boxH, e.tem_chefe ? e.cor_aura : GREEN);

                if (e.tem_chefe) {
                    DrawText("SINAL DETECTADO!", boxX + 10, boxY + 10, 10, RED);
                    DrawText(e.nome_chefe.c_str(), boxX + 10, boxY + 30, 20, WHITE);
                    DrawText(TextFormat("Poder de Luta: %i", e.nivel_poder), boxX + 10, boxY + 55, 10, YELLOW);
                    
                    if (e.eh_lendario) {
                        DrawText("PERIGO EXTREMO", boxX + 10, boxY + 75, 10, PURPLE);
                    } else {
                        DrawText("Ameaça Média", boxX + 10, boxY + 75, 10, ORANGE);
                    }
                } else {
                    DrawText("SISTEMA SEGURO", boxX + 10, boxY + 10, 10, GREEN);
                    DrawText("Recursos Padrão", boxX + 10, boxY + 30, 20, GRAY);
                    DrawText("Sem inimigos detectados", boxX + 10, boxY + 55, 10, LIGHTGRAY);
                }
            }
        }

        EndDrawing();
    }
    
    UnloadRenderTexture(textura_poeira);
    CloseWindow();
    return 0;
}