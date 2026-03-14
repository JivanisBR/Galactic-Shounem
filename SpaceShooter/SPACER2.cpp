#include "raylib.h"
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <cmath>
Texture2D menui, credits, nav, win_tex, mort, fire1, meteoro_tex, iboss;
Sound sob[5], bomb, ai, oh;
Music background_music;

float mult = 1.0f;

int distance_total = 100000;
int distance_traveled = 0;
int distance_left = distance_total;

// --- SISTEMA DE EXPLOSÕES (Início) ---
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

struct Explosion {
    std::vector<Particula> particles;
    bool active;
    ExplosionType currentType;
};

// Lista Global de Explosões
std::vector<Explosion> explosoesAtivas;
Texture2D texParticula; // Textura global

// Funções de Cor (Exatamente como no seu código)
Color GetFireColor(float lifePct) {
    if (lifePct > 0.7f) return ColorLerp(YELLOW, WHITE, (lifePct - 0.7f) * 3.3f);
    if (lifePct > 0.4f) return ColorLerp(ORANGE, YELLOW, (lifePct - 0.4f) * 3.3f);
    return ColorLerp(DARKGRAY, ORANGE, lifePct * 2.5f);
}

Color GetBloodColor(Color base, float lifePct) {
    float alphaBoost = (lifePct > 0.2f) ? 1.0f : lifePct * 5.0f;
    return ColorAlpha(base, alphaBoost);
}
// --- SISTEMA DE EXPLOSÕES (Fim das Estruturas) ---

//----------------------------------------------------------------------------------
// ESTRUTURAS DE DADOS PARA O FUNDO DE ESTRELAS
//----------------------------------------------------------------------------------
struct DustParticle {
    Vector2 position;
    Color color;
};

struct Star {
    Vector2 position;
    int size;
    int speed_delay;
    int move_counter;
    bool growing;
    int anim_timer;
    int anim_limit;
    int base_size;
    int growth_range;
};

struct NebulaParticle {
    Vector2 offset;
    float width;
    float height;
    Color color;
    int star_size;
    bool star_growing;
    int star_timer;
    int star_limit;
};

struct Nebula {
    Vector2 position;
    Color base_color;
    int speed_delay;
    int move_counter;
    std::vector<NebulaParticle> particles;
};

//----------------------------------------------------------------------------------
// ESTRUTURA DOS INIMIGOS
struct Enemy {
    Vector2 position;
    int enemylife;
    bool enemyactive;
};

int DUST_COUNT, BG_STAR_COUNT, FG_STAR_COUNT, NEBULA_COUNT, PARTICLES_PER_NEBULA, STAR_PARTICLE_COUNT;
std::vector<DustParticle> dust;
std::vector<Star> background_stars;
std::vector<Star> foreground_stars;
std::vector<Nebula> nebulas;
RenderTexture2D static_dust_texture;
int opt = 1, sob_index = 0;
bool menu = true, creds = false, quit = false, boss = false, winn = false, pause = false;
int fall, bum = 0, bump = 0, vmin = 1, vmax = 4, pisc = 0;
int mort_x = 250, mort_y = 70;
int pontos = 0, storo1 = 0, storo2 = 0, storo3 = 0, wait = 0, ng = 0;
int nave_x = 390, nave_y = 600, velo = 3, tvel = 5;
int exp_x, exp_y;
int death_x, death_y, vida = 1;
int fire_x, fire_y;
int projetil_x, projetil_y;
int foga = 0, fogd = 0, projetil = 0, cdpause = 0;
int sol_x, sol_y;
int bot_x, bot_y;
int expm_x, expm_y;
int meteoro_x, meteoro_y;
bool meter = true;
int hitbox1x, hitbox1y, hitbox2x, hitbox2y;
bool hit = false;
int death_sound_delay = 0;
int boss_death_sound_delay = 0;
int boss_bomb_count = 0;

int enemyx = GetRandomValue(30, 1150), enemyy = -220, enemyvida = 1;
int bossx = 400, bossy = -220, vidaboss = 50, tirobossx, tirobossy, d, t, v = 1;

//----------------------------------------------------------------------------------
// FUNÇÕES AUXILIARES PARA O FUNDO DE ESTRELAS
//----------------------------------------------------------------------------------
void DrawStarShape(Vector2 center, float size, Color color)
{
    if (size < 1.0f) return;

    // Caso especial para estrelas minúsculas 
    if (size <= 2) {
        DrawRectangle(center.x - size, center.y, size * 2 + 1, 1, color);
        DrawRectangle(center.x, center.y - size, 1, size * 2 + 1, color);
        return;
    }

    // Lógica de desenho para estrelas maiores
    float main_length = size;
    float width = (size / 3.0f) + 1.0f;

    // Raio Vertical
    DrawTriangle({center.x, center.y - main_length}, {center.x - width, center.y}, {center.x + width, center.y}, color);
    DrawTriangle({center.x, center.y + main_length}, {center.x + width, center.y}, {center.x - width, center.y}, color);
    
    // Raio Horizontal
    DrawTriangle({center.x + main_length, center.y}, {center.x, center.y - width}, {center.x, center.y + width}, color);
    DrawTriangle({center.x - main_length, center.y}, {center.x, center.y + width}, {center.x, center.y - width}, color);
}

void GenerateNebulaShape(Nebula& nebula, int particle_count, int star_particle_count) {
    nebula.particles.clear();
    nebula.particles.resize(particle_count);

    int particles_drawn = 0;
    int num_arms = 3;
    int particles_per_arm = particle_count / num_arms;
    if (particles_per_arm == 0) particles_per_arm = 1;

    for (int b = 0; b < num_arms; ++b) {
        Vector2 walker = {0, 0};
        Vector2 direction = {0, 0};
        int steps_left = 0;

        for (int p = 0; p < particles_per_arm; ++p) {
            if (particles_drawn >= particle_count) break;

            if (steps_left <= 0) {
                steps_left = GetRandomValue(10, 20);
                int dir_choice = GetRandomValue(0, 7);
                switch (dir_choice) {
                    case 0: direction = {0, -1}; break; case 1: direction = {1, -1}; break;
                    case 2: direction = {1, 0}; break;  case 3: direction = {1, 1}; break;
                    case 4: direction = {0, 1}; break;  case 5: direction = {-1, 1}; break;
                    case 6: direction = {-1, 0}; break; case 7: direction = {-1, -1}; break;
                }
            }

            walker.x += direction.x * GetRandomValue(4, 8);
            walker.y += direction.y * GetRandomValue(4, 8);
            steps_left--;

            int idx = particles_drawn++;
            
            nebula.particles[idx].offset.x = walker.x + GetRandomValue(-20, 20);
            nebula.particles[idx].offset.y = walker.y + GetRandomValue(-20, 20);
            
            if (idx >= star_particle_count) {
                nebula.particles[idx].width = GetRandomValue(20, 60);
                nebula.particles[idx].height = GetRandomValue(20, 60);
                unsigned char opacity = GetRandomValue(2, 8);
                nebula.particles[idx].color = {nebula.base_color.r, nebula.base_color.g, nebula.base_color.b, opacity};
            } else {
                nebula.particles[idx].width = 0;
                nebula.particles[idx].height = 0;
                nebula.particles[idx].color = nebula.base_color;
                nebula.particles[idx].star_size = GetRandomValue(1, 3);
                nebula.particles[idx].star_growing = GetRandomValue(0, 1);
                nebula.particles[idx].star_limit = GetRandomValue(5, 15);
                nebula.particles[idx].star_timer = 0;
            }
        }
    }
}

// --- LÓGICA DAS EXPLOSÕES ---

void AdicionarExplosao(Vector2 p, ExplosionType type) {
    Explosion novaEx = {};
    novaEx.active = true;
    novaEx.currentType = type;
    novaEx.particles.clear();

    int count = 150;
    float speedMin = 15.0f, speedMax = 55.0f;
    float sizeMin = 10.0f, sizeMax = 65.0f;
    Color bloodBase = GREEN;

    // Configurações por tipo
    if (type == LARGE_SHIP) {
        count = 300;
        speedMin = 10.0f; speedMax = 90.0f; 
        sizeMin = 10.0f; sizeMax = 30.0f;
    } else if (type == METEOR) {
        count = 50;
        speedMin = 10.0f; speedMax = 60.0f;
    }

    // Gerar Partículas Principais (Fogo/Energia)
    for (int i = 0; i < count; i++) {
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float speed = GetRandomValue((int)(speedMin * 10), (int)(speedMax * 10)) / 10.0f;
        
        Particula part;
        part.position = { p.x + GetRandomValue(-10, 10), p.y + GetRandomValue(-10, 10) };
        part.velocity = { cosf(angle) * speed, sinf(angle) * speed };
        part.lifePct = 1.0f;
        part.decayRate = (type == LARGE_SHIP) ? GetRandomValue(10, 40) / 100.0f : GetRandomValue(30, 80) / 100.0f;
        part.isAdditive = true;
        part.size = (float)GetRandomValue((int)sizeMin, (int)sizeMax);
        part.color = WHITE;
        novaEx.particles.push_back(part);
    }

    // Gerar Detritos (Matéria Sólida: Rocha ou Sangue)
    if (type == METEOR || type == MONSTER) {
        int extraCount = (type == METEOR) ? 60 : 200;
        if (type == MONSTER) {
            int r = GetRandomValue(0, 2);
            bloodBase = (r == 0) ? GREEN : (r == 1 ? BLUE : PURPLE);
        }

        for (int i = 0; i < extraCount; i++) {
            float angle = GetRandomValue(0, 360) * DEG2RAD;
            float speed = GetRandomValue(200, 420) / 10.0f; // 2.0 a 12.0
            Particula mat;
            mat.position = p;
            mat.velocity = { cosf(angle) * speed, sinf(angle) * speed };
            mat.lifePct = 50.0f;
            mat.decayRate = GetRandomValue(30, 70) / 100.0f; 
            mat.isAdditive = false;
            
            if (type == METEOR) {
                mat.color = (GetRandomValue(0, 1) == 0) ? GRAY : DARKGRAY;
                mat.size = (float)GetRandomValue(2, 6);
            } else {
                int var = GetRandomValue(0, 2);
                mat.color = (var == 0) ? bloodBase : (var == 1 ? ColorBrightness(bloodBase, 0.4f) : ColorBrightness(bloodBase, -0.4f));
                mat.size = (float)GetRandomValue(2, 5);
            }
            novaEx.particles.push_back(mat);
        }
    }
    explosoesAtivas.push_back(novaEx);
}

void AtualizarExplosoes() {
    float dt = GetFrameTime();
    bool turbo = IsKeyDown(KEY_LEFT_SHIFT);

    // 2. Define a velocidade com que o "cenário" passa
    // Ajuste esses valores: 3.0f (normal) e 15.0f (turbo) conforme o gosto
    float velocidadeQueda = turbo ? 10.0f : 3.0f;

    for (int i = 0; i < (int)explosoesAtivas.size(); i++) {
        int aliveCount = 0;
        for (auto &p : explosoesAtivas[i].particles) {
            if (p.lifePct > 0) {
                // Multiplicamos por 60 para manter a velocidade do seu teste original
                // mesmo rodando a 120 FPS
                p.position.x += p.velocity.x * dt * 10.0f;
                p.position.y += p.velocity.y * dt * 10.0f;
                
                // --- NOVO: APLICA A VELOCIDADE DA NAVE ---
                // Só aplica para partículas SÓLIDAS (!isAdditive) como você pediu.
                // Se quiser que o fogo fique pra trás também, remova o "if".
                //if (!p.isAdditive) { 
                    // Soma posição Y para fazer ela "cair"
                    p.position.y += velocidadeQueda * dt * 20.0f; 
                //}

                float friction = (p.isAdditive) ? 0.95f : 0.99f;
                p.velocity.x *= friction;
                p.velocity.y *= friction;

                if (p.isAdditive) p.size += 0.5f; 
                p.lifePct -= p.decayRate * dt * 4.0f; // Ajuste de tempo
                aliveCount++;
            }
        }
        if (aliveCount == 0) {
            explosoesAtivas.erase(explosoesAtivas.begin() + i);
            i--;
        }
    }
}

void DesenharExplosoes() {
    // Passagem 1: Sólidos (Sem brilho)
    for (auto &ex : explosoesAtivas) {
        for (auto &p : ex.particles) {
            if (p.lifePct > 0 && !p.isAdditive) {
                Color c = (ex.currentType == MONSTER) ? GetBloodColor(p.color, p.lifePct) : ColorAlpha(p.color, p.lifePct > 0.15f ? 1.0f : p.lifePct * 6.0f);
                DrawCircleV(p.position, p.size, c);
            }
        }
    }
    // Passagem 2: Brilho (Additive)
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

void controle() {
    if (menu) {
        cdpause++;
        if (IsKeyPressed(KEY_S) && cdpause > 50 || IsKeyPressed(KEY_DOWN) && cdpause > 50) {
            opt++;
            cdpause = 0;
        }
        if (opt > 3) opt = 1;
        if (IsKeyPressed(KEY_W) && cdpause > 50 || IsKeyPressed(KEY_UP) && cdpause > 50) {
            opt--;
            cdpause = 0;
        }
        if (opt < 1) opt = 3;
        if (opt == 1 && IsKeyPressed(KEY_SPACE)) {
            menu = false;
        }
        if (opt == 2 && IsKeyPressed(KEY_SPACE)) {
            creds = true;
        }
        if (creds && IsKeyPressed(KEY_ENTER)) {
            creds = false;
        }
        if (opt == 3 && IsKeyPressed(KEY_SPACE)) {
            quit = true;
        }
    } else {
        if (IsKeyPressed(KEY_SPACE) && projetil == 0) {
            projetil = 1;
            PlaySound(sob[sob_index]);
            sob_index = (sob_index + 1) % 5;
        }
        if (IsKeyDown(KEY_A) && vida > 0 || IsKeyDown(KEY_LEFT) && vida > 0) {
            nave_x -= (int)(velo * mult);
            foga++;
            if (foga > 90) foga = 0;
        }
        if (IsKeyDown(KEY_D) && vida > 0 || IsKeyDown(KEY_RIGHT) && vida > 0) {
            nave_x += (int)(velo * mult);
            fogd++;
            if (fogd > 90) fogd = 0;
        }
        if (IsKeyDown(KEY_W) && vida > 0 || IsKeyDown(KEY_UP) && vida > 0) {
            nave_y -= (int)(velo * mult);
            fogd++;
            if (fogd > 90) fogd = 0;
        }
        if (IsKeyDown(KEY_S) && vida > 0 || IsKeyDown(KEY_DOWN) && vida > 0) {
            nave_y += (int)(velo * mult);
            fogd++;
            if (fogd > 90) fogd = 0;
        }

        // Limites de tela (SEM HITBOX DENTRO)
        if (nave_x > 1115) nave_x = 1115;
        if (nave_x < -10) nave_x = -10;
        if (nave_y > 600) nave_y = 600;
        if (nave_y < 300) nave_y = 300;

        exp_x = nave_x;
        hitbox1x = nave_x + 10; hitbox2x = nave_x + 38;
        exp_x = nave_x;
        exp_y = nave_y;
        hitbox1y = nave_y + 40; hitbox2y = nave_y + 10;
        exp_y = nave_y;
        hitbox1y = nave_y + 40; hitbox2y = nave_y + 10;
        fire_x = nave_x + 41;
        fire_y = nave_y + 84;
        death_x = nave_x;
        death_y = nave_y;

    }
    if (IsKeyPressed(KEY_P) && !menu && cdpause > 100) {
        menu = true;
        cdpause = 0;
    }
    if (IsKeyPressed(KEY_P) && menu && cdpause > 100) {
        menu = false;
        cdpause = 0;
    }
    cdpause++;
}

void desenhar() {
    BeginDrawing();
    if (menu==true) {
        DrawTexture(menui, 0, 0, WHITE);
        if (opt == 1) {
            DrawRectangle(430, 380, 50, 10, WHITE);
            DrawRectangle(630, 380, 50, 10, WHITE);
        }
        if (opt == 2) {
            DrawRectangle(410, 460, 50, 10, WHITE);
            DrawRectangle(670, 460, 50, 10, WHITE);
        }
        if (opt == 3) {
            DrawRectangle(450, 550, 50, 10, WHITE);
            DrawRectangle(630, 550, 50, 10, WHITE);
        }
        if (creds) {
            DrawTexture(credits, 0, 0, WHITE);
        }
    } else {
        ClearBackground(BLACK);
        
        // --- ATUALIZAÇÃO E DESENHO DO FUNDO DE ESTRELAS ---

    // Verifica se o modo turbo está ativo
    bool turbo_on = IsKeyDown(KEY_LEFT_SHIFT);
    // CALCULA O AVANÇO SINCRONIZADO COM O TEMPO
    // Se turbo: 30 * mult. Se normal: 10 * mult.
    int avanco_frame = (int)((turbo_on ? 30.0f : 10.0f) * mult);

    distance_traveled += avanco_frame;
    distance_left -= avanco_frame;

    // Define a velocidade base: 30x mais rápido se o Turbo estiver ligado, senão 1x
    float base_speed = turbo_on ? 30.0f : 1.0f;

    // 1. BACKGROUND STARS (Estrelas do Fundo)
    for (auto& s : background_stars) {
        // LÓGICA NOVA: Velocidade / Atraso * Multiplicador de Lag
        // O "speed_delay" serve como divisor (quanto maior, mais lenta/longe a estrela)
        float move_amount = (base_speed / (float)s.speed_delay) * mult;
        
        s.position.y += move_amount;

        if(s.position.y > 710) { 
            // 1. Posição Y variável (entre -100 e -10) para não entrarem juntas
            s.position = {(float)GetRandomValue(0, 1200), (float)GetRandomValue(-100, -10)};
            
            // 2. Sorteia nova velocidade para quebrar padrões
            s.speed_delay = GetRandomValue(8, 20);
        }

        // Animação do brilho (também compensando o lag)
        s.anim_timer += (int)mult;
        if (s.anim_timer >= s.anim_limit) {
            s.anim_timer = 0;
            if (s.growing) { if (s.size < s.base_size + s.growth_range) s.size++; else s.growing = false; } 
            else { if (s.size > s.base_size) s.size--; else s.growing = true; }
        }
    }

    // 2. FOREGROUND STARS (Estrelas da Frente)
    for (auto& s : foreground_stars) {
        // Mesma lógica, mas geralmente o speed_delay delas é menor (são mais rápidas)
        float move_amount = (base_speed / (float)s.speed_delay) * mult;
        
        s.position.y += move_amount;

        if(s.position.y > 710) { 
            // Variação Y maior para estrelas rápidas
            s.position = {(float)GetRandomValue(0, 1200), (float)GetRandomValue(-150, -10)};
            
            // Reroll da velocidade (IMPORTANTE)
            s.speed_delay = GetRandomValue(1, 7); 
            
            // Recalcula tamanho base baseado na nova velocidade (opcional, mas fica bonito)
            s.base_size = (12 - s.speed_delay) / 2;
            if(s.base_size < 3) s.base_size = 3;
            s.size = s.base_size;
        }

        s.anim_timer += (int)mult;
        if (s.anim_timer >= s.anim_limit) {
            s.anim_timer = 0;
            if (s.growing) { if (s.size < s.base_size + s.growth_range) s.size++; else s.growing = false; } 
            else { if (s.size > s.base_size) s.size--; else s.growing = true; }
        }
    }

    // 3. NEBULAS
    for(auto& n : nebulas){
        // SEGURANÇA: Se speed_delay for 0, o jogo crasha ou buga. Corrigimos aqui:
        if (n.speed_delay <= 0) n.speed_delay = 20;

        // Cálculo da velocidade com multiplicador
        float move_amount = (base_speed / (float)n.speed_delay) * mult;
        
        // Move a nebulosa
        n.position.y += move_amount;

        // Se sair muito abaixo da tela (700 altura + 300 margem)
        if(n.position.y > 1000) { 
            // Reseta lá em cima (fora da tela)
            n.position = {(float)GetRandomValue(0, 1200), (float)GetRandomValue(-500, -300)};
            
            // Recria o formato para variar
            Color COLOR_ROXO = {128, 0, 128, 255};
            std::vector<Color> nebula_colors = {BLUE, COLOR_ROXO, RED, GREEN, YELLOW, WHITE};
            n.base_color = nebula_colors[GetRandomValue(0, nebula_colors.size() - 1)];
            GenerateNebulaShape(n, PARTICLES_PER_NEBULA, STAR_PARTICLE_COUNT);
        }
        
        // Animação das partículas internas
        for (int i=0; i < STAR_PARTICLE_COUNT; ++i){
            if (i >= n.particles.size()) break;
            auto& p = n.particles[i];
            
            p.star_timer += (int)mult; // Compensa o lag na animação
            
            if (p.star_timer >= p.star_limit) { 
                p.star_growing = !p.star_growing; 
                p.star_timer = 0; 
            }
            if (p.star_growing) { p.star_size++; } 
            else { if (p.star_size > 1) p.star_size--; }
        }
    }

    // DESENHO das camadas do fundo
    DrawTexture(static_dust_texture.texture, 0, 0, WHITE);
    
    for (const auto& s : background_stars) {
        DrawRectangle(s.position.x - s.size, s.position.y, s.size * 2 + 1, 1, {128, 128, 128, 255});
        DrawRectangle(s.position.x, s.position.y - s.size, 1, s.size * 2 + 1, {128, 128, 128, 255});
    }
    
    for (const auto& n : nebulas) {
        for (int i=0; i < n.particles.size(); ++i){
            const auto& p = n.particles[i];
            Vector2 particle_pos = {n.position.x + p.offset.x, n.position.y + p.offset.y};
            if (i < STAR_PARTICLE_COUNT) { DrawStarShape(particle_pos, p.star_size, p.color); } 
            else { DrawEllipse(particle_pos.x, particle_pos.y, p.width/2, p.height/2, p.color); }
        }
    }
    
    for (const auto& s : foreground_stars) { DrawStarShape(s.position, s.size, WHITE); }
    // --- FIM DO CÓDIGO DO FUNDO ---

        DrawText("Points: ", 520, 50, 20, WHITE);
        DrawText("Move: WASD/ARROW KEYS", 20, 30, 20, WHITE);
        DrawText("Shoot: ESPACE", 20, 50, 20, WHITE);
        DrawText("Pause: P", 20, 70, 20, WHITE);
        DrawText("Turbo: LEFT SHIFT", 20, 90, 20, WHITE);
        
        char distance_str[20];
        sprintf(distance_str, "%d       ", distance_left);
        if(distance_left>0){ 
            DrawText("Distance Left: ", 900, 30, 20, WHITE);
            DrawText(distance_str, 1050, 30, 20, WHITE);
        } 
        else {
            DrawText("YOU ARRIVED!", 900, 30, 20, WHITE);
        }
        
        if (projetil == 1 && vida > 0) {
            BeginBlendMode(BLEND_ADDITIVE);

            // 1. O BRILHO (Glow) - textura da partícula esticada
            // Note que aumentei o tamanho (largura 12, altura 45) para criar a aura
            DrawTexturePro(texParticula, 
                {0, 0, 64, 64}, 
                {(float)projetil_x, (float)projetil_y, 12.0f, 35.0f}, 
                {6.0f, 22.5f}, // Centro do brilho
                0.0f, 
                ColorAlpha(YELLOW, 0.5f)); // amarelo transparente

            // 2. O NÚCLEO (Core) - Branco puro para parecer energia concentrada
            // Mantivemos sua elipse original, mas agora BRANCA
            DrawEllipse((float)projetil_x, (float)projetil_y, 2.0f, 15.0f, WHITE);

            EndBlendMode();
            projetil_y -= (int)(tvel * mult);
        }

        // 1. Definição da intensidade do motor
        bool turbo = IsKeyDown(KEY_LEFT_SHIFT);
        bool movendo = IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) || IsKeyDown(KEY_D);

        // Define o tamanho "alvo" do fogo
        // 0.4f = Parado (Idle)
        // 1.0f = Movendo normal
        // 2.5f = Turbo ativado
        float targetScale = turbo ? 2.5f : (movendo ? 1.0f : 0.4f);

        // Variável estática para guardar o tamanho entre os frames (para a animação ser suave)
        static float currentScale = 0.5f;

        // Aumenta ou diminui suavemente em direção ao alvo (Lerp manual)
        // O '0.1f * mult' define a velocidade da transição
        currentScale += (targetScale - currentScale) * 0.1f * mult;

        // 2. Oscilação (Flicker) para parecer fogo instável
        // Um valor aleatório pequeno que muda todo frame
        float flickerX = (float)GetRandomValue(-5, 5) / 100.0f; // Vibra na largura
        float flickerY = (float)GetRandomValue(-10, 20) / 100.0f; // Vibra na altura

        // 3. Desenho do Propulsor
        // Usamos a posição fire_x, fire_y que você já configurou
        Vector2 enginePos = {(float)fire_x, (float)fire_y - 10}; // Subi 10px pra entrar na lataria

        BeginBlendMode(BLEND_ADDITIVE);

        // CAMADA 1: O Brilho Externo (Aura Azulada/Roxa)
        // É grande, transparente e vibra menos
        DrawTexturePro(texParticula, 
            {0, 0, 64, 64}, 
            {enginePos.x, enginePos.y + (20 * currentScale), 40.0f * (currentScale + 0.2f), 70.0f * (currentScale + flickerY)}, 
            {20.0f * (currentScale + 0.2f), 10.0f}, // Ponto de ancoragem (Topo central)
            0.0f, 
            ColorAlpha(DARKBLUE, 0.6f));

        // CAMADA 2: O Núcleo de Energia (Ciano/Azul Claro)
        // Fica dentro, é mais brilhante e vibra mais
        DrawTexturePro(texParticula, 
            {0, 0, 64, 64}, 
            {enginePos.x, enginePos.y + (10 * currentScale), 25.0f * (currentScale + flickerX), 50.0f * (currentScale + flickerY)}, 
            {12.5f * (currentScale + flickerX), 0.0f}, 
            0.0f, 
            ColorAlpha(SKYBLUE, 0.8f));

        // CAMADA 3: O Centro Quente (Branco)
        // Pequeno e sólido, a origem da propulsão
        DrawEllipse(enginePos.x, enginePos.y + 5, 6.0f * currentScale, 15.0f * currentScale, ColorAlpha(WHITE, 0.9f));
        EndBlendMode();

        if (vida > 0) {
            DrawTexture(nav, nave_x, nave_y, WHITE);
        }

        if (projetil_x >= 1200 || projetil_y >= 600 || projetil_x < 0 || projetil_y < 0) {
            projetil = 0;
            projetil_x = nave_x + 48;
            projetil_y = nave_y;
        }
        if (projetil == 0) {
            projetil_x = nave_x + 48;
            projetil_y = nave_y;
        }
        char pontos_str[10];
        sprintf(pontos_str, "%d       ", pontos);
        DrawText(pontos_str, 600, 50, 20, WHITE);

        if (meter && !winn) {
            DrawTexture(meteoro_tex, meteoro_x, meteoro_y, WHITE);
            if(turbo_on==false) {
                bot_y += (int)(fall * mult);
            }
            if(turbo_on==true) {
                bot_y += (int)(fall * mult) + 6;
            }
            meteoro_y = bot_y;
            meteoro_x = bot_x;
        }
        if (bot_y >= 750 && vida > 0) {
            bot_x = GetRandomValue(30, 1150);
            bot_y = GetRandomValue(-800, -10);
            fall = GetRandomValue(vmin, vmax);
        //    pontos--;
        }
        if (nave_y > bot_y && projetil_y <= bot_y && projetil_x >= bot_x && projetil_x <= bot_x + 60 && vida > 0) {
            PlaySound(bomb);
            pontos++;
            meter = false;
            bump = 0;
            projetil = 0;
            projetil_x = nave_x + 48;
            projetil_y = nave_y;
            expm_x = bot_x;
            expm_y = bot_y;
        }
        if (!meter && !winn) {
            wait++;
            bump++;
                //CHAMAR EXPLOSAO METEORO
            if (wait == 1) { // Garante que só roda 1 vez
                AdicionarExplosao({(float)expm_x + 30, (float)expm_y + 30}, METEOR);
            }
            if (wait > 30) {
                meter = true;
                wait = 0;
                bump = 0;
                bot_y = GetRandomValue(-800, -10);
                bot_x = GetRandomValue(30, 1150);
                fall = GetRandomValue(vmin, vmax);
                meteoro_x = bot_x;
                meteoro_y = bot_y;
            }
            if (pontos >= 3 && pontos <= 6) {
                vmin = 1;
                vmax = 4;
            }
            if (pontos >= 7 && pontos <= 9) {
                vmin = 2;
                vmax = 5;
            }
            if (boss) {
                vmax = 6;
            }
        }
        exp_x = nave_x;
        hitbox1x = nave_x + 10; hitbox2x = nave_x + 38;
        exp_x = nave_x;
        exp_y = nave_y;
        hitbox1y = nave_y + 40; hitbox2y = nave_y + 10;
        exp_y = nave_y;
        hitbox1y = nave_y + 40; hitbox2y = nave_y + 10;
        if (distance_traveled >= (distance_total*0.75f) && !winn) {
            boss = true;
        }
        if (boss && !winn) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(texParticula, 
                {0, 0, 64, 64}, 
                {(float)tirobossx, (float)tirobossy, 25.0f, 60.0f}, // Tamanho do brilho (Largo x Alto)
                {12.5f, 30.0f}, // Ponto central (Metade do tamanho)
                0.0f, 
                ColorAlpha(RED, 0.8f)); // Vermelho meio transparente
            DrawTexturePro(texParticula, 
                {0, 0, 64, 64}, 
                {(float)tirobossx+7.0f, (float)tirobossy+15.0f, 12.0f, 30.0f}, // Tamanho do brilho (Largo x Alto)
                {12.5f, 30.0f}, // Ponto central (Metade do tamanho)
                0.0f, 
                ColorAlpha(RED, 0.7f));

            // 2. O NÚCLEO (Branco)
            // O centro branco faz parecer que é tão quente que perdeu a cor
            DrawEllipse((float)tirobossx, (float)tirobossy, 2.0f, 7.0f, WHITE);

            EndBlendMode();
            DrawTexture(iboss, bossx, bossy, WHITE);
            if (bossy < 100) {
                bossy++;
            }
            char vidaboss_str[10];
            sprintf(vidaboss_str, "%d", vidaboss);
            DrawText(vidaboss_str, bossx + 90, bossy, 20, WHITE);
            if (vidaboss >= 40) {
                tirobossy += (int)(2 * mult);
                v = 1;
            } else if (vidaboss >= 30) {
                tirobossy += (int)(3 * mult);
            } else if (vidaboss >= 20) {
                tirobossy += (int)(4 * mult);
                v = 2;
            } else if (vidaboss >= 10) {
                tirobossy += (int)(5 * mult);
            } else {
                tirobossy += (int)(6 * mult);
                v = 3;
            }
            if (tirobossy > 700) {
                tirobossx = GetRandomValue(bossx + 10, bossx + 90);
                tirobossy = bossy + 120;
                PlaySound(oh);
            }
            t -= v;
            if (d == 1 && t > 0) {
                bossx += (int)(v * mult);
            } else if (d == 2 && t > 0) {
                bossx -= (int)(v * mult);
            }
            if (bossx + 200 >= 1200) {
                d = 2;
                t = GetRandomValue(30, 300);
            }
            if (bossx <= 0) {
                d = 1;
                t = GetRandomValue(30, 300);
            }
            if (t < 1) {
                t = GetRandomValue(30, 300);
                d = GetRandomValue(1, 2);
            }
        }
        if (projetil_x >= bossx && projetil_x <= bossx + 200 && projetil_y <= bossy + 150 && boss) {
            vidaboss--;
            projetil = 0;
            projetil_x = nave_x + 48;
            projetil_y = nave_y;
            sob_index = (sob_index + 1) % 5;
        }
        if (vidaboss < 1) {
                    //CHAMAR EXPLOSAO DE MONSTRO
            if (boss_bomb_count < 5 && boss_death_sound_delay == 0) {
                PlaySound(bomb);
                boss_bomb_count++;
                boss_death_sound_delay = 30; // 166ms a 60 FPS
            }
            if (boss_death_sound_delay == 30) { 
                AdicionarExplosao({(float)bossx + 100, (float)bossy + 75}, MONSTER);
            }
            if (bump < 100) {
                bump++;
            }
            
            if(bump>=60){
                bossy = -220;
                tirobossy = bossy + 120;
            }
            boss = false;
            if(distance_traveled >= distance_total){
                winn = true;
            }
        }
        if (boss_death_sound_delay > 0) {
            boss_death_sound_delay--;
            if (boss_death_sound_delay == 0 && boss_bomb_count < 5) {
                PlaySound(bomb);
                boss_bomb_count++;
                boss_death_sound_delay = 20; // Reinicia o atraso
            }
        }

        DesenharExplosoes();

        if (winn) {
            pisc++;
            DrawTexture(win_tex, 100, 100, WHITE);
            if (pisc < 100) {
                DrawText("PRESS ENTER TO LAND", 440, 480, 20, WHITE);
            }
            if (pisc >= 200) {
                pisc = 0;
            }
            if (IsKeyPressed(KEY_ENTER)) {
                // INICIA LOADING SCREEN
                // MUDA PARA GAMEPLAY EXPLORAÇÃO
                quit = true;
            }
        }
        if ((bot_y >= death_y - 30 && bot_y <= death_y + 50 && bot_x >= death_x - 50 && bot_x <= death_x + 60) ||
            (tirobossy >= death_y+20 && tirobossy <= death_y+80 && tirobossx >= death_x+20 && tirobossx <= death_x + 70)) {
            if (death_sound_delay == 0) {
                PlaySound(bomb);
                death_sound_delay = 20; // Atraso de 10 frames
            }
            vida = 0;
        }
        if (death_sound_delay > 0) {
            death_sound_delay--;
            if (death_sound_delay == 0 && vida == 0) {
                PlaySound(ai);
            }
        }
        if (vida < 1) {
            bum++;
            pisc++;
                    //CHAMAR EXPLOSAO GRANDE
            if (bum == 1) { // Só no primeiro frame da morte
                AdicionarExplosao({(float)nave_x + 40, (float)nave_y + 40}, LARGE_SHIP);
            }
            DrawTexture(mort, mort_x, mort_y, WHITE);
            if (pisc < 100) {
                DrawText("APERTE R PARA TENTAR DE NOVO", 470, 420, 20, RED);
            }
            if (pisc >= 200) {
                pisc = 0;
            }
            if (IsKeyPressed(KEY_R)) {
                winn = false;
                vida = 1;
                pontos = 1;
                bum = 0;
                vidaboss = 50;
                bossy = -220;
            }
        }
    }
    EndDrawing();
}

int main() {
    // --- INICIALIZAÇÃO DA JANELA ---
    const int screenWidth = 1200;
    const int screenHeight = 700;
    InitWindow(screenWidth, screenHeight, "SPACE PLAGYL");
    InitAudioDevice();
    SetTargetFPS(120);

    // Variáveis para a seleção de densidade
    std::string density_choice = "";
    bool density_selected = false;

    // --- LOOP PRÉ-JOGO: TELA DE SELEÇÃO DE DENSIDADE ---
    while (!density_selected && !WindowShouldClose()) {
        if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) {
            density_choice = "baixa";
            density_selected = true;
        }
        if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) {
            density_choice = "media";
            density_selected = true;
        }
        if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) {
            density_choice = "alta";
            density_selected = true;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("ESCOLHA A DENSIDADE DO CENARIO", 270, 250, 40, WHITE);
        DrawText("1 - BAIXA (Melhor Performance)", 400, 350, 20, GRAY);
        DrawText("2 - MEDIA (Equilibrado)", 400, 380, 20, GRAY);
        DrawText("3 - ALTA (Melhor Visual)", 400, 410, 20, GRAY);
        EndDrawing();
    }

    // Se o usuário fechou a janela na tela de seleção, sai do jogo
    if (WindowShouldClose()) {
        CloseAudioDevice();
        CloseWindow();
        return 0;
    }

    // --- CONFIGURAÇÃO DAS DENSIDADES (baseado na escolha) ---
    if (density_choice == "baixa") {
        // Estrelas em densidade Média, Nebulosas Desligadas
        DUST_COUNT = 500; BG_STAR_COUNT = 100; FG_STAR_COUNT = 50;
        NEBULA_COUNT = 0; PARTICLES_PER_NEBULA = 0; STAR_PARTICLE_COUNT = 0;
    } else if (density_choice == "alta") {
        // Estrelas em densidade Alta, Nebulosas em densidade Média
        DUST_COUNT = 800; BG_STAR_COUNT = 200; FG_STAR_COUNT = 100;
        NEBULA_COUNT = 10; PARTICLES_PER_NEBULA = 250; STAR_PARTICLE_COUNT = 10;
    } else { // Padrão: Média
        // Estrelas em densidade Média, Nebulosas em densidade Baixa
        DUST_COUNT = 500; BG_STAR_COUNT = 100; FG_STAR_COUNT = 50;
        NEBULA_COUNT = 5; PARTICLES_PER_NEBULA = 100; STAR_PARTICLE_COUNT = 10;
    }

    // --- REDIMENSIONAMENTO E INICIALIZAÇÃO DOS VETORES ---
    dust.resize(DUST_COUNT);
    background_stars.resize(BG_STAR_COUNT);
    foreground_stars.resize(FG_STAR_COUNT);
    nebulas.resize(NEBULA_COUNT);

    // --- CARREGAMENTO DE ASSETS ---
    srand(static_cast<unsigned>(time(0)));
    background_music = LoadMusicStream("C:/Users/giova/Desktop/Projetos/Space Plagyl Soundtrack.mp3");
    SetMusicVolume(background_music, 1.0f);
    PlayMusicStream(background_music);
    menui = LoadTexture("SPACE PLAGYL.png");
    credits = LoadTexture("CREDITS.png");
    sob[0] = LoadSound("sobio1.mp3"); sob[1] = LoadSound("sobio2.mp3"); sob[2] = LoadSound("sobio3.mp3"); sob[3] = LoadSound("sobio4.mp3"); sob[4] = LoadSound("sobio5.mp3");
    bomb = LoadSound("bomb.mp3"); ai = LoadSound("ai.mp3"); oh = LoadSound("tiroboss.wav");
    nav = LoadTexture("navee2.png"); win_tex = LoadTexture("winner.png"); mort = LoadTexture("mort.png");
    meteoro_tex = LoadTexture("meteoro.png"); iboss = LoadTexture("boss.png");
    // --- GERAÇÃO DA TEXTURA DE PARTÍCULA ---
    Image imgExp = GenImageGradientRadial(64, 64, 0.0f, WHITE, BLACK);
    texParticula = LoadTextureFromImage(imgExp);
    UnloadImage(imgExp);

    // --- INICIALIZAÇÃO DAS VARIÁVEIS ---
    fall = GetRandomValue(1, 4); exp_x = nave_x; exp_y = nave_y; death_x = nave_x + 25; death_y = nave_y + 10; fire_x = nave_x + 30;
    fire_y = nave_y + 84; projetil_x = nave_x + 48; projetil_y = nave_y; sol_x = GetRandomValue(10, 1100); sol_y = GetRandomValue(10, 400);
    bot_x = GetRandomValue(50, 1100); bot_y = -10; expm_x = bot_x + 200; expm_y = bot_y + 100; meteoro_x = bot_x; meteoro_y = bot_y;
    hitbox1x = nave_x + 10; hitbox1y = nave_y + 40; hitbox2x = nave_x + 38; hitbox2y = nave_y + 10;
    tirobossx = GetRandomValue(bossx + 10, bossx + 90); tirobossy = bossy + 50; d = GetRandomValue(1, 2); t = GetRandomValue(30, 500);

    // --- INICIALIZAÇÃO DO FUNDO DE ESTRELAS ---
    Color COLOR_ROXO = {128, 0, 128, 255}; Color COLOR_CINZA_ESCURO = {80, 80, 80, 255}; Color COLOR_CINZA = {128, 128, 128, 255};
    std::vector<Color> nebula_colors = {BLUE, COLOR_ROXO, RED, GREEN, YELLOW, WHITE};
    static_dust_texture = LoadRenderTexture(screenWidth, screenHeight);
    BeginTextureMode(static_dust_texture);
    ClearBackground(BLANK);
    for (int i = 0; i < DUST_COUNT; ++i) {
        dust[i].position = {(float)GetRandomValue(0, screenWidth), (float)GetRandomValue(0, screenHeight)};
        dust[i].color = (GetRandomValue(0,1) == 0) ? COLOR_CINZA : COLOR_CINZA_ESCURO;
        DrawPixelV(dust[i].position, dust[i].color);
    }
    EndTextureMode();
    for (int i = 0; i < BG_STAR_COUNT; ++i) { background_stars[i].position = {(float)GetRandomValue(0, 1200), (float)GetRandomValue(0, 700)};
        background_stars[i].speed_delay = GetRandomValue(8, 20);
        background_stars[i].base_size = GetRandomValue(1, 2);
        background_stars[i].size = background_stars[i].base_size;
        background_stars[i].move_counter = GetRandomValue(0, background_stars[i].speed_delay);
        background_stars[i].growing = (GetRandomValue(0, 1) == 1);
        background_stars[i].anim_limit = GetRandomValue(5, 15);
        background_stars[i].anim_timer = 0;
        background_stars[i].growth_range = 3; 
    }
    
    for (int i = 0; i < FG_STAR_COUNT; ++i) {  foreground_stars[i].position = {(float)GetRandomValue(0, 1200), (float)GetRandomValue(0, 700)};
        foreground_stars[i].speed_delay = GetRandomValue(1, 7);
        foreground_stars[i].base_size = (12 - foreground_stars[i].speed_delay) / 2;
        if(foreground_stars[i].base_size < 3) foreground_stars[i].base_size = 3;
        foreground_stars[i].size = foreground_stars[i].base_size;
        foreground_stars[i].move_counter = GetRandomValue(0, foreground_stars[i].speed_delay);
        foreground_stars[i].growing = (GetRandomValue(0, 1) == 1);
        foreground_stars[i].anim_limit = GetRandomValue(1, 6);
        foreground_stars[i].anim_timer = 0;
        foreground_stars[i].growth_range = (foreground_stars[i].base_size > 5) ? 9 : 6; 
    }
    
    for (int i = 0; i < NEBULA_COUNT; ++i) { 
        nebulas[i].position = {(float)GetRandomValue(0, 1200), (float)GetRandomValue(-400, 600)};
        
        // Define a velocidade (delay)
        nebulas[i].speed_delay = GetRandomValue(10, 30);
        nebulas[i].move_counter = 0;
        
        // Cores
        Color COLOR_ROXO = {128, 0, 128, 255};
        std::vector<Color> nebula_colors = {BLUE, COLOR_ROXO, RED, GREEN, YELLOW, WHITE};
        nebulas[i].base_color = nebula_colors[GetRandomValue(0, nebula_colors.size() - 1)];
        
        // Gera as partículas passando o item específico
        GenerateNebulaShape(nebulas[i], PARTICLES_PER_NEBULA, STAR_PARTICLE_COUNT);
    }

    // --- LOOP PRINCIPAL ---
    while (!WindowShouldClose() && !quit) {
        mult = GetFrameTime() * 120.0f;
        // Se o PC for muito rápido e o frame for minúsculo, garante um mínimo
        if (mult < 0.1f) mult = 0.1f;
        controle();
        AtualizarExplosoes();
        UpdateMusicStream(background_music);
        desenhar();
    }

    // --- LIMPEZA FINAL ---
    UnloadRenderTexture(static_dust_texture);
    UnloadTexture(menui); UnloadTexture(credits); UnloadTexture(nav); UnloadTexture(win_tex); UnloadTexture(mort);
    UnloadTexture(meteoro_tex); UnloadTexture(iboss);
    UnloadMusicStream(background_music);
    for (int i = 0; i < 5; i++) UnloadSound(sob[i]);
    UnloadSound(bomb); UnloadSound(ai); UnloadSound(oh);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}