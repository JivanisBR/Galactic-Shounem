#include "raylib.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

//----------------------------------------------------------------------------------
// ESTRUTURAS DE DADOS
//----------------------------------------------------------------------------------

struct Estrela {
    Vector2 pos;
    int tam;
    Color cor;
    float velocidade_y;
    
    // Gameplay
    int raridade; // 0: Comum, 1: Incomum, 2: Rara, 3: Lendária
    
    // estrelas da constelação (espada)
    Vector2 pos_alvo; 
    bool chegou_no_alvo; 
    bool coletada; // ADICIONADO: Faltava isso!
    
    // Animação
    bool crescendo;
    int timer_anim;
    int limite_anim;
    int tam_base;
    int alcance_cresc;
};

struct ParticulaNebulosa {
    Vector2 offset;
    float larg;
    float alt;
    Color cor;
    int tam_estrela;
    int tam_base_estrela; 
    int alcance_cresc_estrela; 
    bool estrela_crescendo;
    int timer_estrela;
    int limite_estrela;
};

struct Nebulosa {
    Vector2 pos;
    Color cor_base;
    int atraso_vel;
    int cont_mov;
    std::vector<ParticulaNebulosa> particulas;
};

struct Linha {
    Vector2 inicio;
    Vector2 fim;
    float comprimento;
};

//----------------------------------------------------------------------------------
// VARIÁVEIS GLOBAIS
//----------------------------------------------------------------------------------
bool desenhar_diagonais = false; 

Color COR_COMUM = {200, 200, 200, 200};   
Color COR_INCOMUM = {0, 228, 255, 255};   
Color COR_RARA = {255, 0, 128, 255};      
Color COR_LENDARIA = {255, 215, 0, 255};  

Sound som_comum;
Sound som_raro;
Sound som_lendario;
Sound som_final;

std::vector<Estrela> estrelas_fundo;
std::vector<Estrela> estrelas_chuva; 
std::vector<Estrela> estrelas_constelacao; 
std::vector<Nebulosa> nebulosas;
std::vector<Linha> linhas_espada; 

//----------------------------------------------------------------------------------
// FUNÇÕES AUXILIARES
//----------------------------------------------------------------------------------

void ConfigurarEstrelaRaridade(Estrela& e, int screenWidth) {
    int sorteio = GetRandomValue(0, 100);
    e.chegou_no_alvo = false;
    e.coletada = false; // Reseta
    e.crescendo = (GetRandomValue(0, 1) == 1);
    e.timer_anim = 0;

    if (sorteio < 70) { 
        e.raridade = 0; e.cor = COR_COMUM; 
        e.tam_base = GetRandomValue(5, 6); 
        e.velocidade_y = (float)GetRandomValue(20, 35) / 10.0f; 
        e.limite_anim = GetRandomValue(10, 20); e.alcance_cresc = 2;
    } else if (sorteio < 90) { 
        e.raridade = 1; e.cor = COR_INCOMUM; 
        e.tam_base = GetRandomValue(7, 8); 
        e.velocidade_y = (float)GetRandomValue(35, 60) / 10.0f; 
        e.limite_anim = GetRandomValue(5, 15); e.alcance_cresc = 4;
    } else if (sorteio < 99) { 
        e.raridade = 2; e.cor = COR_RARA; 
        e.tam_base = GetRandomValue(9, 11); 
        e.velocidade_y = (float)GetRandomValue(60, 100) / 10.0f; 
        e.limite_anim = GetRandomValue(3, 8); e.alcance_cresc = 6;
    } else { 
        e.raridade = 3; e.cor = COR_LENDARIA; 
        e.tam_base = GetRandomValue(12, 18); 
        e.velocidade_y = (float)GetRandomValue(100, 200) / 10.0f; 
        e.limite_anim = GetRandomValue(2, 5); e.alcance_cresc = 8;
    }
    e.tam = e.tam_base;
    e.pos = {(float)GetRandomValue(0, screenWidth), (float)GetRandomValue(-50, -10)};
}

Vector2 GetPontoAleatorioEspada() {
    float comprimento_total = 0;
    for (const auto& l : linhas_espada) comprimento_total += l.comprimento;

    float sorteio = (float)GetRandomValue(0, (int)comprimento_total);
    
    float acumulado = 0;
    for (const auto& l : linhas_espada) {
        if (sorteio <= acumulado + l.comprimento) {
            float t = (float)GetRandomValue(0, 100) / 100.0f; 
            return {
                l.inicio.x + (l.fim.x - l.inicio.x) * t,
                l.inicio.y + (l.fim.y - l.inicio.y) * t
            };
        }
        acumulado += l.comprimento;
    }
    return linhas_espada[0].inicio;
}

void DrawStarShape(Vector2 centro, float tamanho, Color cor) {
    if (tamanho < 1.0f) return;
    if (tamanho <= 2) {
        DrawRectangle(centro.x - tamanho, centro.y, tamanho * 2 + 1, 1, cor);
        DrawRectangle(centro.x, centro.y - tamanho, 1, tamanho * 2 + 1, cor);
        return;
    }
    float main_length = tamanho;
    float diag_length = main_length * 0.7f;
    float width = (tamanho / 3.0f) + 1.0f;

    DrawTriangle({centro.x, centro.y - main_length}, {centro.x - width, centro.y}, {centro.x + width, centro.y}, cor);
    DrawTriangle({centro.x, centro.y + main_length}, {centro.x + width, centro.y}, {centro.x - width, centro.y}, cor);
    DrawTriangle({centro.x + main_length, centro.y}, {centro.x, centro.y - width}, {centro.x, centro.y + width}, cor);
    DrawTriangle({centro.x - main_length, centro.y}, {centro.x, centro.y + width}, {centro.x, centro.y - width}, cor);

    if (desenhar_diagonais || tamanho > 5) {
        float o_c = diag_length * 0.707f;
        float o_w = width * 0.707f;
        Vector2 p_se = { centro.x - o_c, centro.y - o_c }, p_id = { centro.x + o_c, centro.y + o_c };
        Vector2 o_sd = { centro.x + o_w, centro.y - o_w }, o_ie = { centro.x - o_w, centro.y + o_w };
        DrawTriangle(p_se, o_sd, o_ie, cor); DrawTriangle(p_id, o_sd, o_ie, cor);
        Vector2 p_sd = { centro.x + o_c, centro.y - o_c }, p_ie = { centro.x - o_c, centro.y + o_c };
        Vector2 o_id = { centro.x + o_w, centro.y + o_w }, o_se = { centro.x - o_w, centro.y - o_w };
        DrawTriangle(p_sd, o_id, o_se, cor); DrawTriangle(p_ie, o_id, o_se, cor);
    }
}

void GenerateNebulaShape(Nebulosa& neb, int cont_particulas, int cont_estrelas) {
    if (cont_particulas <= 0) { neb.particulas.clear(); return; }
    neb.particulas.resize(cont_particulas);
    int num_bracos = 3, particulas_por_braco = cont_particulas / num_bracos;
    if (particulas_por_braco == 0) particulas_por_braco = 1;
    int intervalo_estrela = 0;
    if (cont_estrelas > 0) intervalo_estrela = cont_particulas / cont_estrelas;

    int part_desenhadas = 0;
    for (int b = 0; b < num_bracos; ++b) {
        Vector2 walker = {0, 0}, direcao = {0, 0};
        int passos = 0;
        for (int p = 0; p < particulas_por_braco; ++p) {
            if (part_desenhadas >= cont_particulas) break;
            if (passos <= 0) {
                passos = GetRandomValue(10, 20);
                int dir = GetRandomValue(0, 7);
                if(dir==0) direcao={0,-1}; else if(dir==1) direcao={1,-1}; else if(dir==2) direcao={1,0};
                else if(dir==3) direcao={1,1}; else if(dir==4) direcao={0,1}; else if(dir==5) direcao={-1,1};
                else if(dir==6) direcao={-1,0}; else direcao={-1,-1};
            }
            walker.x += direcao.x * GetRandomValue(4, 8);
            walker.y += direcao.y * GetRandomValue(4, 8);
            passos--;
            int idx = part_desenhadas++;
            neb.particulas[idx].offset = {walker.x + GetRandomValue(-20, 20), walker.y + GetRandomValue(-20, 20)};
            bool eh_estrela = (intervalo_estrela > 0 && idx > 0 && idx % intervalo_estrela == 0);
            if (eh_estrela) {
                neb.particulas[idx].larg = 0; neb.particulas[idx].alt = 0;
                neb.particulas[idx].cor = WHITE;
                neb.particulas[idx].tam_base_estrela = GetRandomValue(4, 7);
                neb.particulas[idx].tam_estrela = neb.particulas[idx].tam_base_estrela;
                neb.particulas[idx].alcance_cresc_estrela = 3;
                neb.particulas[idx].estrela_crescendo = (GetRandomValue(0, 1) == 1);
                neb.particulas[idx].limite_estrela = GetRandomValue(5, 15);
                neb.particulas[idx].timer_estrela = 0;
            } else {
                neb.particulas[idx].larg = GetRandomValue(20, 60); neb.particulas[idx].alt = GetRandomValue(20, 60);
                unsigned char op = GetRandomValue(2, 8);
                neb.particulas[idx].cor = {neb.cor_base.r, neb.cor_base.g, neb.cor_base.b, op};
                neb.particulas[idx].tam_base_estrela = 0;
            }
        }
    }
}

//----------------------------------------------------------------------------------
// Função Principal
//----------------------------------------------------------------------------------
int main(void)
{
    const int screenWidth = 1200;
    const int screenHeight = 700;
    
    int cont_ESTRELAS_FUNDO, cont_ESTRELAS_CHUVA; 
    int cont_NEBULOSAS, PARTICULAS_POR_NEBULOSA, PARTICULAS_ESTRELA;
    int pontuacao = 0;
    
    InitWindow(screenWidth, screenHeight, "Upgrade de Skill - Conexão Astral");
    // --- VARIÁVEIS DE TEMPO E FINALIZAÇÃO ---
    float tempo_restante = GetRandomValue(10, 30); // Sorteia entre 10 e 30 segundos
    bool tempo_esgotado = false;
    bool espada_finalizada = false;
    int frames_pisca = 0; // Para o texto piscar

    InitAudioDevice();
    som_comum = LoadSound("sobio1.mp3");
    som_raro = LoadSound("sobio2.mp3");
    som_lendario = LoadSound("sobio3.mp3");
    som_final = LoadSound("final.mp3");

    // --- DEFINIÇÃO DA ESPADA ---
    linhas_espada.push_back({{200, 400}, {800, 400}, 0});
    linhas_espada.push_back({{300, 350}, {300, 450}, 0});
    linhas_espada.push_back({{300, 350}, {400, 400}, 0});
    linhas_espada.push_back({{300, 450}, {400, 400}, 0});

    for (auto& l : linhas_espada) {
        l.comprimento = sqrt(pow(l.fim.x - l.inicio.x, 2) + pow(l.fim.y - l.inicio.y, 2));
    }

    // --- TELA DE SELEÇÃO 
    std::string density_choice = "";
    bool density_selected = false;
    while (!density_selected && !WindowShouldClose()) {
        if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) { density_choice = "baixa"; density_selected = true; }
        if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) { density_choice = "media"; density_selected = true; }
        if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) { density_choice = "alta"; density_selected = true; }
        
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("ESCOLHA A INTENSIDADE DA CONEXAO ASTRAL", 250, 200, 30, WHITE);
        DrawText("1 - BAIXA (Equilibrado)", 400, 300, 20, GRAY);
        DrawText("2 - MEDIA (Chuva de Estrelas)", 400, 330, 20, GRAY);
        DrawText("3 - ALTA (Caos Cósmico)", 400, 360, 20, RED);
        EndDrawing();
    }
    if (WindowShouldClose()) { CloseWindow(); return 0; }
    
    // --- CONFIGURAÇÃO DAS DENSIDADES 
    if (density_choice == "baixa") {
        // Baixa
        cont_ESTRELAS_FUNDO = 50; cont_ESTRELAS_CHUVA = 25;
        cont_NEBULOSAS = 5; PARTICULAS_POR_NEBULOSA = 100; PARTICULAS_ESTRELA = 10;
    } else if (density_choice == "alta") {
        // Máxima
        desenhar_diagonais = true;
        cont_ESTRELAS_FUNDO = 100; cont_ESTRELAS_CHUVA = 75;
        cont_NEBULOSAS = 15; PARTICULAS_POR_NEBULOSA = 300; PARTICULAS_ESTRELA = 15;
    } else { // Média 
        cont_ESTRELAS_FUNDO = 100; cont_ESTRELAS_CHUVA = 100;
        cont_NEBULOSAS = 10; PARTICULAS_POR_NEBULOSA = 200; PARTICULAS_ESTRELA = 10;
    }

    estrelas_fundo.resize(cont_ESTRELAS_FUNDO);
    estrelas_chuva.resize(cont_ESTRELAS_CHUVA);
    nebulosas.resize(cont_NEBULOSAS);

    Color COR_ROXO = {128, 0, 128, 255};
    std::vector<Color> cores_nebulosa = {BLUE, COR_ROXO, RED, GREEN, YELLOW, WHITE};
    
    // Inicialização Estrelas Fundo
    for (int i = 0; i < cont_ESTRELAS_FUNDO; ++i) {
        estrelas_fundo[i].pos = {(float)GetRandomValue(0, screenWidth), (float)GetRandomValue(0, screenHeight)};
        estrelas_fundo[i].velocidade_y = (float)GetRandomValue(2, 8) / 10.0f;
        estrelas_fundo[i].tam_base = 5; 
        estrelas_fundo[i].tam = 5;
        estrelas_fundo[i].crescendo = (GetRandomValue(0, 1) == 1);
        estrelas_fundo[i].limite_anim = GetRandomValue(10, 20);
        estrelas_fundo[i].timer_anim = 0; estrelas_fundo[i].alcance_cresc = 1;
        estrelas_fundo[i].cor = {100, 100, 100, 150};
    }

    // Inicialização Estrelas Chuva
    for (int i = 0; i < cont_ESTRELAS_CHUVA; ++i) {
        ConfigurarEstrelaRaridade(estrelas_chuva[i], screenWidth);
        estrelas_chuva[i].pos.y = GetRandomValue(0, screenHeight);
    }

    // Inicialização Nebulosas
    for (int i = 0; i < cont_NEBULOSAS; ++i) {
        nebulosas[i].pos = {(float)GetRandomValue(0, screenWidth), (float)GetRandomValue(0, screenHeight)};
        nebulosas[i].atraso_vel = GetRandomValue(5, 15);
        nebulosas[i].cont_mov = GetRandomValue(0, nebulosas[i].atraso_vel);
        nebulosas[i].cor_base = cores_nebulosa[GetRandomValue(0, cores_nebulosa.size() - 1)];
        GenerateNebulaShape(nebulosas[i], PARTICULAS_POR_NEBULOSA, PARTICULAS_ESTRELA);
    }

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // --- LÓGICA DO TEMPO E FINALIZAÇÃO ---
        if (!tempo_esgotado) {
            tempo_restante -= GetFrameTime(); // Decrementa o tempo real (segundos)
            if (tempo_restante <= 0) {
                tempo_restante = 0;
                tempo_esgotado = true;
                // Opcional: Tocar um som de "alerta" ou "tempo acabou" aqui
            }
        }

        // Se o tempo acabou, espera o jogador apertar ENTER
        if (tempo_esgotado && !espada_finalizada) {
            if (IsKeyPressed(KEY_ENTER)) {
                espada_finalizada = true;
                PlaySound(som_final);
            }
        }
        if (espada_finalizada && tempo_esgotado) {
                if(IsKeyPressed(KEY_BACKSPACE)){
                    espada_finalizada = false;
                //PlaySound(som_final);
                }
        }
        if(IsKeyPressed(KEY_R)){
            // Reinicia o jogo
            pontuacao = 0;
            tempo_restante = GetRandomValue(10, 30);
            tempo_esgotado = false;
            espada_finalizada = false;
            frames_pisca = 0;

            // Reconfigura as estrelas da chuva
            for (int i = 0; i < cont_ESTRELAS_CHUVA; ++i) {
                ConfigurarEstrelaRaridade(estrelas_chuva[i], screenWidth);
                estrelas_chuva[i].pos.y = GetRandomValue(0, screenHeight);
                estrelas_fundo[i].pos.y = GetRandomValue(0, screenHeight);
            }
        }
        
        // --- INTERAÇÃO, PONTUAÇÃO E SOM ---
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mousePos = GetMousePosition();
            float margem_hitbox = 30.0f; 

            // 1. Verifica CHUVA 
            for (auto& e : estrelas_chuva) {
                // Verifica se está visível e se houve colisão
                if (e.pos.y > -20 && CheckCollisionPointCircle(mousePos, e.pos, e.tam + margem_hitbox)) {
                    
                    // Lógica de Pontos e Som
                    if (e.raridade == 3) { 
                        pontuacao += 20; 
                        PlaySound(som_lendario); // Som Lendário
                    }
                    else if (e.raridade == 2) { 
                        pontuacao += 5; 
                        PlaySound(som_raro); // Som Raro
                    }
                    else { 
                        pontuacao += 1; 
                        SetSoundPitch(som_comum, 1.0f + (GetRandomValue(-500, 500)*0.001));
                        PlaySound(som_comum); // Som Comum
                    }

                    Estrela nova = e; 
                    nova.pos_alvo = GetPontoAleatorioEspada();
                    nova.coletada = true; 
                    nova.chegou_no_alvo = false;
                    nova.crescendo = true; nova.tam = 1; nova.timer_anim = 0;
                    estrelas_constelacao.push_back(nova);

                    ConfigurarEstrelaRaridade(e, screenWidth); 
                    e.pos.y = GetRandomValue(-500, -100); 
                }
            }

            // 2. Verifica FUNDO 
            for (auto& e : estrelas_fundo) {
                if (CheckCollisionPointCircle(mousePos, e.pos, e.tam + margem_hitbox)) {
                    pontuacao += 1; 
                    SetSoundPitch(som_comum, 1.0f + (GetRandomValue(-500, 500)*0.001));
                    PlaySound(som_comum); 

                    Estrela nova = e; 
                    nova.pos_alvo = GetPontoAleatorioEspada();
                    nova.coletada = true; 
                    nova.chegou_no_alvo = false;
                    nova.crescendo = true; nova.tam = 1; nova.timer_anim = 0;
                    estrelas_constelacao.push_back(nova);

                    e.pos.y = GetRandomValue(-1000, -200); 
                    e.pos.x = GetRandomValue(0, screenWidth);
                }
            }

            // 3. Verifica NEBULOSAS 
            for (auto& n : nebulosas) {
                for (auto& p : n.particulas) {
                    if (p.tam_base_estrela > 0) {
                        Vector2 p_pos = {n.pos.x + p.offset.x, n.pos.y + p.offset.y};
                        if (p_pos.y > 0 && p_pos.y < screenHeight && CheckCollisionPointCircle(mousePos, p_pos, p.tam_estrela + margem_hitbox)) {
                            pontuacao += 1; 
                            SetSoundPitch(som_comum, 1.0f + (GetRandomValue(-500, 500)*0.001));
                            PlaySound(som_comum); 

                            Estrela nova;
                            nova.pos = p_pos;
                            nova.cor = p.cor; 
                            nova.tam = p.tam_estrela;
                            nova.tam_base = p.tam_base_estrela;
                            nova.crescendo = true; nova.limite_anim = 10; nova.timer_anim = 0; nova.alcance_cresc = 3;
                            nova.pos_alvo = GetPontoAleatorioEspada();
                            nova.coletada = true; nova.chegou_no_alvo = false;
                            estrelas_constelacao.push_back(nova);
                            
                            p.tam_estrela = 0; 
                        }
                    }
                }
            }
        }

        // --- UPDATES ---

        // Update Fundo
        for (auto& e : estrelas_fundo) {
            e.pos.y += e.velocidade_y;
            if(e.pos.y > screenHeight + 10) { e.pos.y = -10; e.pos.x = GetRandomValue(0, screenWidth); }
            e.timer_anim++;
            if (e.timer_anim >= e.limite_anim) {
                e.timer_anim = 0;
                if (e.crescendo) { if (e.tam < e.tam_base + e.alcance_cresc) e.tam++; else e.crescendo = false; } 
                else { if (e.tam > e.tam_base) e.tam--; else e.crescendo = true; }
            }
        }

        // Update Chuva Infinita
        for (auto& e : estrelas_chuva) {
            e.pos.y += e.velocidade_y;
            if(e.pos.y > screenHeight + 20) { ConfigurarEstrelaRaridade(e, screenWidth); } // Recicla se sair
            
            e.timer_anim++;
            if (e.timer_anim >= e.limite_anim) {
                e.timer_anim = 0;
                if (e.crescendo) { if (e.tam < e.tam_base + e.alcance_cresc) e.tam++; else e.crescendo = false; } 
                else { if (e.tam > e.tam_base) e.tam--; else e.crescendo = true; }
            }
        }

        // Update Constelação (Espada)
        for (auto& e : estrelas_constelacao) {
            if (!e.chegou_no_alvo) {
                e.pos.x += (e.pos_alvo.x - e.pos.x) * 0.1f;
                e.pos.y += (e.pos_alvo.y - e.pos.y) * 0.1f;
                if (abs(e.pos.x - e.pos_alvo.x) < 1.0f && abs(e.pos.y - e.pos_alvo.y) < 1.0f) {
                    e.chegou_no_alvo = true;
                }
            }
            // Cintilação na espada
            e.timer_anim++;
            if (e.timer_anim >= e.limite_anim) {
                e.timer_anim = 0;
                if (e.crescendo) { if (e.tam < e.tam_base + e.alcance_cresc) e.tam++; else e.crescendo = false; } 
                else { if (e.tam > e.tam_base) e.tam--; else e.crescendo = true; }
            }
        }

        // Update Nebulosas
        for(auto& n : nebulosas){
            n.cont_mov++; if(n.cont_mov >= n.atraso_vel){ n.pos.y++; n.cont_mov = 0; }
            if(n.pos.y > screenHeight + 300){
                n.pos = {(float)GetRandomValue(0, screenWidth), (float)GetRandomValue(-400, -200)};
                n.cor_base = cores_nebulosa[GetRandomValue(0, cores_nebulosa.size() - 1)];
                GenerateNebulaShape(n, PARTICULAS_POR_NEBULOSA, PARTICULAS_ESTRELA);
            }
            for (int i=0; i < PARTICULAS_ESTRELA; ++i){
                if (i >= n.particulas.size()) break;
                auto& p = n.particulas[i];
                if (p.tam_base_estrela > 0) {
                    p.timer_estrela++;
                    if (p.timer_estrela >= p.limite_estrela) { p.estrela_crescendo = !p.estrela_crescendo; p.timer_estrela = 0; }
                    if (p.estrela_crescendo) { p.tam_estrela++; } else { if (p.tam_estrela > 1) p.tam_estrela--; }
                }
            }
        }
        
        // --- DESENHO ---
        BeginDrawing();
        ClearBackground(BLACK);
        
        // Desenha Fundo
        for (const auto& e : estrelas_fundo) {
            DrawStarShape(e.pos, e.tam, e.cor);
        }
        
        // Desenha Nebulosas
        for (const auto& n : nebulosas) {
            for (int i=0; i < n.particulas.size(); ++i){
                const auto& p = n.particulas[i];
                Vector2 pos_particula = {n.pos.x + p.offset.x, n.pos.y + p.offset.y};
                int intervalo_estrela = 0;
                if (PARTICULAS_ESTRELA > 0) intervalo_estrela = PARTICULAS_POR_NEBULOSA / PARTICULAS_ESTRELA;
                bool eh_estrela = (intervalo_estrela > 0 && i > 0 && i % intervalo_estrela == 0);
                if (eh_estrela) { DrawStarShape(pos_particula, p.tam_estrela, p.cor); } 
                else { DrawEllipse(pos_particula.x, pos_particula.y, p.larg/2, p.alt/2, p.cor); }
            }
        }
        
        // Desenha a Constelação (Espada)
        
        // 1. Desenha o Glow
        BeginBlendMode(BLEND_ADDITIVE); // Ativa o brilho
        for (const auto& e : estrelas_constelacao) {
            // O glow é 4x maior q a estrela, talvez seja melhor diminuir futuramente?
            float raio_glow = e.tam * 4.0f; 
            Color cor_glow = e.cor;
            cor_glow.a = 50; // Opacidade do brilho (0-255)

            // Desenha um circulo que enfraquece do meio para fora mt top
            DrawCircleGradient((int)e.pos.x, (int)e.pos.y, raio_glow, cor_glow, BLANK);
            if(espada_finalizada){
                // Glow extra branco puro no estado final
                DrawCircleGradient((int)e.pos.x, (int)e.pos.y, raio_glow/2, WHITE, BLANK);
              // raio_glow=raio_glow*10.0f;
            }
        }
        EndBlendMode(); // Volta ao modo de desenho normal

        // 2. Desenha o Núcleo da Estrela por cima
        for (const auto& e : estrelas_constelacao) { 
            if (espada_finalizada) {
                // ESTADO FINAL: Tudo branco puro e brilhante (energia unificada)
                DrawStarShape(e.pos, e.tam, WHITE);
            } else {
                // ESTADO DURANTE O JOGO: Cores normais + núcleo nas raras
                DrawStarShape(e.pos, e.tam, e.cor);
                if(e.raridade > 1){
                    DrawStarShape(e.pos, e.tam/2, WHITE);
                }
            }
        }

        // Desenha Chuva Interativa com glow nas Lendárias
        for (const auto& e : estrelas_chuva) { 
            // Se for lendária desenha o Glow primeiro
            if (e.raridade == 3) {
                BeginBlendMode(BLEND_ADDITIVE);
                
                // brilho 4x o tamanho da estrela)
                float raio_glow = e.tam * 4.0f;
                
                // Cor do glow igual a da estrela, mas translúcida
                Color cor_glow = e.cor;
                cor_glow.a = 180; // Transparência pra parecer luz
                
                DrawCircleGradient((int)e.pos.x, (int)e.pos.y, raio_glow, cor_glow, BLANK);
                
                EndBlendMode();
            }

            // Desenha a estrela normalmente por cima
            DrawStarShape(e.pos, e.tam, e.cor); 
        }
        
        // HUD
        DrawText(TextFormat("PONTOS: %04i", pontuacao), 20, 20, 30, YELLOW);
        // Desenha o Timer no canto superior direito
        DrawText(TextFormat("TEMPO: %02.0f", tempo_restante), screenWidth - 250, 20, 40, tempo_restante < 5 ? RED : WHITE);

        // Desenha o texto piscante se o tempo acabou
        if (tempo_esgotado && !espada_finalizada) {
            frames_pisca++;
            if ((frames_pisca / 30) % 2 == 0) { // Pisca a cada meio segundo (aprox)
                DrawText("APERTE ENTER PARA FINALIZAR", screenWidth / 2 - 250, screenHeight - 100, 40, YELLOW);
            }
        }
        if (tempo_esgotado && espada_finalizada) {
            frames_pisca++;
            if ((frames_pisca / 30) % 2 == 0) { // Pisca a cada meio segundo (aprox)
                DrawText("APERTE BACKSPACE PARA CONTINUAR", screenWidth / 2 - 300, screenHeight - 100, 40, YELLOW);
                DrawText("APERTE R PARA REINICIAR", screenWidth / 2 - 300, screenHeight - 50, 40, YELLOW);
            }
        }

        EndDrawing();
    }
    
    UnloadSound(som_comum);
    UnloadSound(som_raro);
    UnloadSound(som_lendario);
    UnloadSound(som_final);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}