#include "raylib.h"
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <cmath>
#include "boss.h"
#include "Player.h"
#include "Nave.h"
Texture2D menui, credits, nav, mort, fire1, meteoro_tex, enemynav;
Sound sob[5], bomb, ai, oh;
Music background_music;

float mult = 1.0f;

int distance_total = (GetRandomValue(200,1000)*1000);
int distance_traveled = 0;
int distance_left = distance_total;

Boss* chefeFinal = nullptr; 
bool boss_defeated = false;

struct Drop {
    Vector2 pos; // Usando Vector2 para facilitar a física
    Vector2 vel; // Velocidade da explosão
    int valor;   // Quanto este pedacinho vale
    TipoMinerio tipo;
};
// A lista e os inventários continuam iguais:
std::vector<Drop> drops;
Player* jogador = nullptr;



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

// Funções de Cor
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
/*struct Enemy {
    Vector2 position;
    int enemylife;
    bool enemyactive;
};*/

// --- SISTEMA DE TIROS AVANÇADO ---
struct Tiro {
    float x, y;
    bool ativo;
};
std::vector<Tiro> tiros; // A lista que guarda todos os tiros
float cooldownTiro = 0.0f; // O temporizador para não atirar 1000 vezes por seg

int DUST_COUNT, BG_STAR_COUNT, FG_STAR_COUNT, NEBULA_COUNT, PARTICLES_PER_NEBULA, STAR_PARTICLE_COUNT;
std::vector<DustParticle> dust;
std::vector<Star> background_stars;
std::vector<Star> foreground_stars;
std::vector<Nebula> nebulas;
RenderTexture2D static_dust_texture;
int opt = 1, sob_index = 0;
bool menu = true, creds = false, quit = false, boss = false, enemy = false, winn = false, pause = false;
int fall, bum = 0, bump = 0, vmin = 1, vmax = 4, pisc = 0;
int mort_x = 250, mort_y = 70;
int pontos = 0, storo1 = 0, storo2 = 0, storo3 = 0, wait = 0, ng = 0;
int nave_x = 390, nave_y = 600, velo = 3, tvel = 5;
int exp_x, exp_y;
int death_x, death_y, vida = 10;
int fire_x, fire_y;
int foga = 0, fogd = 0, cdpause = 0;
int sol_x, sol_y;
int bot_x, bot_y;
int expm_x, expm_y;
int meteoro_x, meteoro_y;
float meteoro_rot = 0.0f;       // Ângulo atual do meteoro
float meteoro_rot_speed = 0.0f; // Velocidade e direção do giro
bool meter = true;
int hitbox1x, hitbox1y, hitbox2x, hitbox2y;
bool hit = false;
int death_sound_delay = 0;

int enemyx = GetRandomValue(30, 1150), enemyy = -220, enemylife = 5, enemytype = GetRandomValue(1, 4);
Color enemyColor = WHITE;
bool enemy_defeated = false; // Trava contra respawn infinito
float bossTimerMorte = 0.0f;

// Variáveis do tiro do inimigo
float tiroenemy_x = -100, tiroenemy_y = 1000;
bool tiroenemy_ativo = false;
float enemy_shoot_timer = 0; // Cooldown do tiro inimigo
int bossx = 400, bossy = -600, vidaboss = 50;

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

// --- LÓGICA DE DROPS DE MINÉRIOS ---
// Recebe Vector2 para facilitar
void SpawnLoot(Vector2 centroExplosao) {
    int chance = GetRandomValue(1, 100);
    TipoMinerio tipoAtual;
    int valorTotalRNG;

    // 1. Determina o TIPO e o VALOR TOTAL (Sua lógica original perfeita)
    if (chance <= 50) { 
        tipoAtual = FERRO; 
        valorTotalRNG = GetRandomValue(10, 50);
    } 
    else if (chance <= 85) { 
        tipoAtual = PRATA; 
        valorTotalRNG = GetRandomValue(5, 20);
    } 
    else { 
        tipoAtual = OURO; 
        valorTotalRNG = GetRandomValue(1, 5);
    }

    // 2. Define em quantos pedaços físicos o meteoro vai quebrar
    // Vamos gerar entre 4 e 8 pedacinhos para espalhar bem
    int numPedacos = GetRandomValue(4, 8);

    // Calcula quanto cada pedacinho vale (divisão inteira)
    int valorBase = valorTotalRNG / numPedacos;
    // O resto da divisão a gente distribui para não perder nenhum pontinho
    int resto = valorTotalRNG % numPedacos;

    // 3. Gera as partículas físicas
    for(int i = 0; i < numPedacos; i++) {
        Drop d;
        d.pos = centroExplosao;
        d.tipo = tipoAtual;
        
        // Distribui o valor: os primeiros pegam o resto da divisão
        d.valor = valorBase + (i < resto ? 1 : 0);
        // Segurança: garante que vale pelo menos 1
        if (d.valor < 1) d.valor = 1; 

        // FÍSICA DA EXPLOSÃO (Chafariz)
        // Ângulo entre 180 (Esquerda) e 360 (Direita), passando por 270 (Cima)
        // Isso faz eles sempre serem jogados "contra" o movimento da nave
        float ang = GetRandomValue(180, 360) * DEG2RAD;
        // Aumentei um pouco a força mínima (de 30 pra 50) pra garantir que subam
        float speed = GetRandomValue(50, 90) / 10.0f; 
        d.vel = { cosf(ang) * speed, sinf(ang) * speed };

        drops.push_back(d);
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
    bool turbo = IsKeyDown(KEY_K);

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
    } 
    else {
        // --- LÓGICA DE DISPARO (CADÊNCIA) ---
        // Diminui o tempo de espera a cada frame
        if (cooldownTiro > 0) cooldownTiro -= 1.0f * mult;

        // Se apertou ESPAÇO e o tempo de espera acabou
        if (IsKeyDown(KEY_SPACE) && cooldownTiro <= 0) {
            // Cria um novo tiro na ponta da nave
            Tiro novoTiro;
            novoTiro.x = (float)nave_x + 48;
            novoTiro.y = (float)nave_y;
            novoTiro.ativo = true;
            
            // Adiciona na lista
            tiros.push_back(novoTiro);
            
            // Toca o som
            PlaySound(sob[sob_index]);
            sob_index = (sob_index + 1) % 5;
            
            // RESETA O TEMPO DE ESPERA (Cadência)
            // 8.0f = Metralhadora rápida. 20.0f = Tiro lento. Ajuste a gosto!
            cooldownTiro = 40.0f; 
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
        fire_x = nave_x + 49;
        fire_y = nave_y + 84;
        death_x = nave_x + 25;
        death_y = nave_y + 15;

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

    // --- CONTROLE MESTRE DE VELOCIDADE E FÍSICA ---
    bool turbo_on = IsKeyDown(KEY_K);
    bool freio_on = IsKeyDown(KEY_L);

    // 1. Atualiza a Física da Nave no Espaço (Se estiver viva e não tiver chegado)
    if (distance_left > 0 && vida > 0 && !winn) {
        jogador->minhaNave->AtualizarVoo(GetFrameTime() * mult, turbo_on, freio_on);
    }

    // 2. Feedback Visual (Fator de Velocidade do Mundo)
    // Se a velocidade for 100, o fator é 1.0 (Normal). Se for 500, o fator é 5.0 (Rápido).
    float world_speed_factor = jogador->minhaNave->velocidadeAtual / 100.0f;
    
    if (distance_left <= 0) {
        world_speed_factor = 0.0f; // NAVE CHEGOU: PARA TUDO
    }

    // 3. Progresso da Viagem Real
    if (distance_left > 0 && vida > 0) {
        // O avanço real no espaço é puramente a velocidade atual da nave
        float avanco_exato = jogador->minhaNave->velocidadeAtual * GetFrameTime() * mult;
        
        distance_traveled += (int)avanco_exato;
        distance_left -= (int)avanco_exato;
        
        // Trava para não passar do limite e negativar
        if (distance_left < 0) {
            distance_left = 0;
            distance_traveled = distance_total;
        }
    }

    // 1. BACKGROUND STARS (Animação Senoidal - Blindada contra bugs)
    for (auto& s : background_stars) {
        float move_amount = (world_speed_factor / (float)s.speed_delay) * mult;
        s.position.y += move_amount;

        // Reset quando sai da tela
        if(s.position.y > 710) { 
            s.position = {(float)GetRandomValue(0, 1200), (float)GetRandomValue(-100, -10)};
            s.speed_delay = GetRandomValue(8, 20); 
            s.base_size = GetRandomValue(1, 2); // Garante tamanho base pequeno
            s.growth_range = 3;
        }

        // --- NOVA ANIMAÇÃO MATEMÁTICA ---
        // Pega o tempo atual * velocidade (3.0f) + posição X (para desincronizar uma da outra)
        // O resultado sempre vai variar suavemente entre -1 e 1
        float wave = sin(GetTime() * 3.0f + s.position.x * 0.05f); 
        
        // Transforma o -1..1 em 0..1
        float factor = (wave + 1.0f) / 2.0f;
        
        // Define o tamanho ABSOLUTO. É impossível passar do limite.
        s.size = s.base_size + (int)(factor * s.growth_range);
    }

    // 2. FOREGROUND STARS (Animação Senoidal)
    for (auto& s : foreground_stars) {
        float move_amount = (world_speed_factor / (float)s.speed_delay) * mult;
        s.position.y += move_amount;

        if(s.position.y > 710) { 
            s.position = {(float)GetRandomValue(0, 1200), (float)GetRandomValue(-150, -10)};
            s.speed_delay = GetRandomValue(1, 7); 
            
            // Recalcula Base
            s.base_size = (12 - s.speed_delay) / 2;
            if(s.base_size < 3) s.base_size = 3;
            
            // Recalcula Limite
            s.growth_range = (s.base_size > 4) ? 9 : 6;
        }

        // --- NOVA ANIMAÇÃO MATEMÁTICA ---
        // Velocidade 5.0f (mais rápido que o fundo)
        float wave = sin(GetTime() * 5.0f + s.position.x * 0.01f);
        float factor = (wave + 1.0f) / 2.0f;
        
        // Cálculo absoluto
        s.size = s.base_size + (int)(factor * s.growth_range);
    }

    // 3. NEBULAS
    for(auto& n : nebulas){
        if (n.speed_delay <= 0) n.speed_delay = 20;
        float move_amount = (world_speed_factor / (float)n.speed_delay) * mult;
        n.position.y += move_amount;

        if(n.position.y > 1000) { 
            n.position = {(float)GetRandomValue(0, 1200), (float)GetRandomValue(-500, -300)};
            Color COLOR_ROXO = {128, 0, 128, 255};
            std::vector<Color> nebula_colors = {BLUE, COLOR_ROXO, RED, GREEN, YELLOW, WHITE};
            n.base_color = nebula_colors[GetRandomValue(0, nebula_colors.size() - 1)];
            GenerateNebulaShape(n, PARTICLES_PER_NEBULA, STAR_PARTICLE_COUNT);
        }
        
        // Animação das partículas internas (SENOIDAL)
        for (int i=0; i < STAR_PARTICLE_COUNT; ++i){
            if (i >= n.particles.size()) break;
            auto& p = n.particles[i];
            
            // Usa o índice 'i' para que cada estrela da mesma nebulosa pisque diferente
            float wave = sin(GetTime() * 6.0f + (i * 0.5f));
            float factor = (wave + 1.0f) / 2.0f;
            
            // Tamanho fixo entre 1 e 3 (ou 1 e 4)
            p.star_size = 2 + (int)(factor * 3);
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

    // --- ESTRELA DE DESTINO (SOL 100% LUZ) ---
    if (distance_left <= 1000) {
        float sol_y_atual = 200.0f - (distance_left * 1.66f);
        float sol_x_atual = 600.0f; 
        float pulso = sin(GetTime() * 4.0f) * 15.0f;

        BeginBlendMode(BLEND_ADDITIVE);
        
        // 1. Aura Externa (Laranja difuso)
        DrawTexturePro(texParticula, 
            {0, 0, 64, 64}, 
            {sol_x_atual, sol_y_atual, 800.0f + pulso, 800.0f + pulso}, 
            {400.0f + pulso/2.0f, 400.0f + pulso/2.0f}, 
            0.0f, 
            ColorAlpha(ORANGE, 0.2f));
            
        // 2. Corona Intermediária (Amarela)
        DrawTexturePro(texParticula, 
            {0, 0, 64, 64}, 
            {sol_x_atual, sol_y_atual, 500.0f - pulso, 500.0f - pulso}, 
            {250.0f - pulso/2.0f, 250.0f - pulso/2.0f}, 
            0.0f, 
            ColorAlpha(YELLOW, 0.5f));
            
        // 3. Núcleo de Energia (Branco Suave - substitui o DrawCircle sólido)
        DrawTexturePro(texParticula, 
            {0, 0, 64, 64}, 
            {sol_x_atual, sol_y_atual, 250.0f, 250.0f}, 
            {125.0f, 125.0f}, 
            0.0f, 
            ColorAlpha(WHITE, 0.8f));

        // 4. Centro Quente (Branco concentrado no miolo)
        DrawTexturePro(texParticula, 
            {0, 0, 64, 64}, 
            {sol_x_atual, sol_y_atual, 100.0f, 100.0f}, 
            {50.0f, 50.0f}, 
            0.0f, 
            WHITE); 

        EndBlendMode();
    }

        DrawText("Points: ", 520, 50, 20, WHITE);
        DrawText("Move: WASD/ARROW KEYS", 20, 30, 20, WHITE);
        DrawText("Shoot: ESPACE", 20, 50, 20, WHITE);
        DrawText("Pause: P", 20, 70, 20, WHITE);
        DrawText("Turbo: K", 20, 90, 20, WHITE);
        DrawText("Break: L", 20, 110, 20, WHITE);
        DrawText(TextFormat("VELOCIDADE: %d km/s", (int)jogador->minhaNave->velocidadeAtual), 20, 140, 20, SKYBLUE);
        DrawText(TextFormat("COMBUSTÍVEL: %d / %d", (int)jogador->minhaNave->combustivelAtual, (int)jogador->minhaNave->combustivelMaximo), 20, 160, 20, ORANGE);
        DrawText(TextFormat("ESCUDO: %d N", (int)vida), 20, 180, 20, GREEN);

        char distance_str[20];
        sprintf(distance_str, "%d       ", distance_left);
        if(distance_left>0){ 
            DrawText("Distance Left: ", 900, 30, 20, WHITE);
            DrawText(distance_str, 1050, 30, 20, WHITE);
        } 
        else {
            DrawText("YOU ARRIVED!", 900, 30, 20, WHITE);
        }
        
        


        if (vida > 0) {
            // 1. Definição da intensidade do motor
            bool turbo = IsKeyDown(KEY_K);
            bool movendo = IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) || IsKeyDown(KEY_D);
            bool freio = IsKeyDown(KEY_L); 

            // Define o tamanho "alvo" do fogo
            // Turbo: 2.5f
            // Freio: 0.2f (Foguinho bem pequeno)
            // Normal: 1.0f ou 0.4f
            float targetScale;
            if (turbo) targetScale = 2.5f;
            else if (freio) targetScale = 0.2f; // <--- NOVO ESTADO
            else targetScale = (movendo ? 1.0f : 0.4f);

            // Variável estática para guardar o tamanho entre os frames (para a animação ser suave)
            static float currentScale = 0.5f;

            // Aumenta ou diminui suavemente em direção ao alvo (Lerp manual)
            // O '0.1f * mult' define a velocidade da transição
            currentScale += (targetScale - currentScale) * 0.1f * mult;

            // 2. Oscilação (Flicker) para parecer fogo instável
            // Um valor aleatório pequeno que muda todo frame
            int flickermin = turbo ? -20 : -10; // Se turbo, pode vibrar mais
            int flickermax = turbo ? 30 : 10; // Se turbo, pode vibrar mais
            float flickerX = (float)GetRandomValue(flickermin, flickermax) / 100.0f; // Vibra na largura
            float flickerY = (float)GetRandomValue(flickermin, flickermax) / 100.0f; // Vibra na altura

            // 3. Desenho do Propulsor
            // Usamos a posição fire_x, fire_y que você já configurou
            if(turbo) {
                // Se turbo, o fogo sai mais para baixo (na lataria) para parecer que está "empurrando" mais
                if(fire_y > nave_y + 24){
                    fire_y -= 1; // Sobe lentamente para a posição do motor
                }
            }
            else {
                // Se não, fica um pouco mais contido
                if(fire_y < nave_y + 84){
                    fire_y += 1; // Desce lentamente para a posição do motor
                }
            }
            float compensacaoY = turbo ? 35.0f : 10.0f;
            Vector2 enginePos = {(float)fire_x, (float)fire_y - compensacaoY};

            BeginBlendMode(BLEND_ADDITIVE);

            // CAMADA 1: O Brilho Externo (Aura Azulada/Roxa)
            // É grande, transparente e vibra menos
            DrawTexturePro(texParticula, 
                {0, 0, 64, 64}, 
                {enginePos.x, enginePos.y + (currentScale), 40.0f * (currentScale + 0.2f), 70.0f * (currentScale + flickerY)}, 
                {20.0f * (currentScale + 0.2f), 10.0f}, // Ponto de ancoragem (Topo central)
                0.0f, 
                ColorAlpha(GOLD, 0.6f));

            // CAMADA 2: O Núcleo de Energia (Ciano/Azul Claro)
            // Fica dentro, é mais brilhante e vibra mais
            DrawTexturePro(texParticula, 
                {0, 0, 64, 64}, 
                {enginePos.x, enginePos.y + (currentScale), 25.0f * (currentScale + flickerX), 50.0f * (currentScale + flickerY)}, 
                {12.5f * (currentScale + flickerX), 0.0f}, 
                0.0f, 
                ColorAlpha(YELLOW, 0.8f));

            EndBlendMode();

            // --- GERENCIAMENTO DOS PROJÉTEIS (Loop Geral) ---
    
            // Configura o visual de energia (BlendMode) UMA VEZ para todos os tiros
            BeginBlendMode(BLEND_ADDITIVE);
            
            for (int i = 0; i < (int)tiros.size(); i++) {
                // 1. DESENHO (Visual que criamos antes)
                DrawTexturePro(texParticula, 
                    {0, 0, 64, 64}, 
                    {tiros[i].x, tiros[i].y, 12.0f, 45.0f}, 
                    {6.0f, 22.5f}, 
                    0.0f, 
                    ColorAlpha(ORANGE, 0.5f));
                DrawEllipse(tiros[i].x, tiros[i].y, 2.0f, 15.0f, WHITE);

                // 2. MOVIMENTO
                tiros[i].y -= (tvel * 2) * mult; // Multipliquei tvel por 2 pra ficar rápido

                // 3. REMOVE SE SAIU DA TELA (Otimização)
                if (tiros[i].y < -50) {
                    tiros.erase(tiros.begin() + i);
                    i--; // Ajusta o índice porque removemos um item
                    continue; // Pula pro próximo
                }

                // --- COLISÕES ---

                // A) Colisão com METEORO (Inimigo comum)
                // Usando a mesma lógica de hitbox sua: projetil dentro da caixa do bot
                if (meter && vida > 0) {
                    if (tiros[i].y <= bot_y + 40 &&       // Altura
                        tiros[i].y >= bot_y - 20 && 
                        tiros[i].x >= bot_x &&            // Largura esquerda
                        tiros[i].x <= bot_x + 60) {       // Largura direita
                        
                        // Acertou!
                        PlaySound(bomb);
                        AdicionarExplosao({(float)bot_x + 30, (float)bot_y + 20}, SMALL_SHIP); // Sua explosão nova
                        SpawnLoot({ (float)bot_x + 30, (float)bot_y + 20 });
                        pontos++;
                        
                        // Reseta o inimigo
                        meter = false;
                        bump = 0;
                        // Importante: Remove o tiro que acertou!
                        tiros.erase(tiros.begin() + i);
                        i--; 
                        
                        // Lógica sua de respawn
                        expm_x = bot_x; expm_y = bot_y;
                        continue; // Pula pro próximo tiro
                    }
                }

                // B) Colisão com BOSS
                if (boss && vida > 0 && chefeFinal->vida > 0) { 
                    float larguraBoss = 2048.0f * chefeFinal->escalaBoss; 
                    float alturaBoss = 2048.0f * chefeFinal->escalaBoss;
                    
                    if (tiros[i].x >= chefeFinal->x && tiros[i].x <= chefeFinal->x + larguraBoss && 
                        tiros[i].y <= chefeFinal->y + alturaBoss - 50 && tiros[i].y >= chefeFinal->y) {
                        
                        chefeFinal->vida--;
                        chefeFinal->tempoDor = 0.5f; 
                        
                        // Fica com raiva
                        if (chefeFinal->vida == 40 || chefeFinal->vida == 30 || chefeFinal->vida == 20 || chefeFinal->vida == 10) {
                            chefeFinal->tempoRaiva = 3.0f;
                            chefeFinal->cadenciaTiro -= 0.1f; 
                            if (chefeFinal->cadenciaTiro < 0.1f) chefeFinal->cadenciaTiro = 0.1f;
                        }
                        
                        tiros.erase(tiros.begin() + i);
                        i--;
                        continue;
                    }
                }
                // C) Colisão com INIMIGO NORMAL
                if (enemy && vida > 0) {
                    // Hitbox do inimigo normal (Ajustada pra largura 60x40)
                    if (tiros[i].x >= enemyx && tiros[i].x <= enemyx + 60 && 
                        tiros[i].y <= enemyy + 40 && tiros[i].y >= enemyy) {
                        
                        enemylife--; // Só tira 1 de vida
                        
                        // Remove o tiro do player
                        tiros.erase(tiros.begin() + i);
                        i--;
                        continue;
                    }
                }
            }
            EndBlendMode(); // Fecha o modo de luz
            
            // --- DESENHO DA NAVE (Animação Fluida -5 a +5) ---
            // Escala: -5 (Máximo Esquerda) <--> 0 (Centro) <--> +5 (Máximo Direita)
            static float estado_inclinacao = 0.0f; 
            float vel_animacao = 0.4f * mult;

            // 1. Lógica da Escala Deslizante
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
                estado_inclinacao += vel_animacao;
                if (estado_inclinacao > 5.0f) estado_inclinacao = 5.0f; // Trava na direita
            } 
            else if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
                estado_inclinacao -= vel_animacao;
                if (estado_inclinacao < -5.0f) estado_inclinacao = -5.0f; // Trava na esquerda
            } 
            else {
                // 2. Retorno elástico para o centro (0.0) quando solta a tecla
                if (estado_inclinacao > 0.0f) {
                    estado_inclinacao -= vel_animacao;
                    if (estado_inclinacao < 0.0f) estado_inclinacao = 0.0f;
                } else if (estado_inclinacao < 0.0f) {
                    estado_inclinacao += vel_animacao;
                    if (estado_inclinacao > 0.0f) estado_inclinacao = 0.0f;
                }
            }

            // 3. Converte a escala matemática para os recortes do Sprite
            // O frame atual ignora o sinal negativo (ex: -3.5 vira frame 3)
            int frame_atual = (estado_inclinacao < 0.0f) ? (int)-estado_inclinacao : (int)estado_inclinacao;
            
            // Define a direção do espelhamento (1 = Normal, -1 = Espelhado)
            float direcao_textura = (estado_inclinacao < 0.0f) ? -1.0f : 1.0f;

            

            // Recorte do SpriteSheet
            Rectangle sourceRec = { (float)(frame_atual * 100), 0.0f, 100.0f * direcao_textura, 100.0f };
            Vector2 posNave = { (float)nave_x, (float)nave_y };

            DrawTextureRec(nav, sourceRec, posNave, WHITE);

            // --- REFLEXO DE LUZ NA NAVE (BACKLIGHT) ---
            // Recalculamos se está com turbo para sincronizar a luz
            bool turbo_glow = IsKeyDown(KEY_K);
            bool movendo_glow = IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) || IsKeyDown(KEY_D);

            // Define a intensidade da luz (Alpha)
            // Turbo: Muito forte (0.7)
            // Andando: Médio (0.4)
            // Parado: Fraco (0.15) - A nave brilha levemente só por estar ligada
            float glowAlpha;
            if (turbo_glow) glowAlpha = 0.7f;
            else if (IsKeyDown(KEY_L)) glowAlpha = 0.05f; // Luz bem fraquinha no freio
            else glowAlpha = (movendo_glow ? 0.4f : 0.15f);
            
            // Define o tamanho da "mancha" de luz
            float glowSize = turbo_glow ? 60.0f : 40.0f;

            // Posição: Centralizado horizontalmente no motor, mas um pouco pra cima (na lataria)
            // fire_x é o centro do fogo. Subimos 25 pixels (fire_y - 25) para pegar na bunda da nave
            Vector2 lightPos = {(float)fire_x, (float)fire_y - 25};

            BeginBlendMode(BLEND_ADDITIVE);
            
            // Usamos a mesma textura de partícula (redonda e suave)
            DrawTexturePro(texParticula,
                {0, 0, 64, 64}, // Origem
                {lightPos.x, lightPos.y + 30, glowSize, glowSize}, // Destino
                {glowSize / 2.0f, glowSize / 2.0f}, // Centro (Pivot)
                0.0f, // Rotação
                ColorAlpha(YELLOW, glowAlpha) // Azul Céu transparente
            );
            
            // Um segundo ponto de luz menor e branco no centro, para dar o "pico" de brilho
            DrawTexturePro(texParticula,
                {0, 0, 64, 64},
                {lightPos.x, lightPos.y + 30, glowSize * 0.5f, glowSize * 0.5f}, 
                {(glowSize * 0.5f) / 2.0f, (glowSize * 0.5f) / 2.0f},
                0.0f,
                ColorAlpha(WHITE, glowAlpha * 0.8f) // Branco quase puro
            );

            EndBlendMode();
        }


        // --- GERENCIAMENTO DE DROPS (Física de Chafariz + Coleta Fácil) ---
    
    // ATIVA O BRILHO PARA TODOS OS DROPS
    BeginBlendMode(BLEND_ADDITIVE); 

    for (int i = 0; i < (int)drops.size(); i++) {
        
        // --- FÍSICA ---
        drops[i].pos.x += drops[i].vel.x * mult;
        drops[i].pos.y += drops[i].vel.y * mult;

        drops[i].vel.x *= 0.96f;
        drops[i].vel.y *= 0.96f;

        // O Loot solto no espaço é engolido pela inércia da sua velocidade
        drops[i].pos.y += (3.0f * world_speed_factor) * mult;

        // --- DESENHO (Agora todos brilham) ---
        Color corLoot, corBorda;
        float raio;
        if (drops[i].tipo == FERRO) { 
            corLoot = {100, 100, 100, 255}; corBorda = WHITE; raio = 6.0f; // Cinza brilhante
        } else if (drops[i].tipo == PRATA) { 
            corLoot = {200, 200, 255, 255}; corBorda = BLUE; raio = 7.0f; // Azulado
        } else { 
            corLoot = GOLD; corBorda = ORANGE; raio = 8.0f; // Dourado forte
        }

        // Desenha com brilho
        DrawCircleV(drops[i].pos, raio + 3, ColorAlpha(corBorda, 0.4f)); // Aura
        DrawCircleV(drops[i].pos, raio, corLoot); // Núcleo

        // --- COLETA (HITBOX RETANGULAR LARGA) ---
        // Centro da nave
        float naveCenterX = (float)nave_x + 24;
        float naveCenterY = (float)nave_y + 40;

        // Verifica a distância X e Y separadamente
        // Distância X < 60 (Significa 120 pixels de largura total de coleta!)
        // Distância Y < 40 (Significa 80 pixels de altura)
        if (fabs(drops[i].pos.x - naveCenterX) < 60 && 
            fabs(drops[i].pos.y - naveCenterY) < 40) {
            
            if (drops[i].tipo == FERRO) jogador->minhaNave->GuardarMinerio(FERRO, drops[i].valor);
            else if (drops[i].tipo == PRATA) jogador->minhaNave->GuardarMinerio(PRATA, drops[i].valor);
            else jogador->minhaNave->GuardarMinerio(OURO, drops[i].valor);

            drops.erase(drops.begin() + i);
            i--;
            continue;
        }

        // --- LIMPEZA ---
        if (drops[i].pos.y > 750) {
            drops.erase(drops.begin() + i);
            i--;
        }
    }
    EndBlendMode(); // DESLIGA O BRILHO

        
        char pontos_str[10];
        sprintf(pontos_str, "%d       ", pontos);       
        DrawText(pontos_str, 600, 50, 20, WHITE);
        // HUD DE RECURSOS 
        DrawText(TextFormat("Fe: %d", jogador->minhaNave->invFerro), 1050, 600, 20, GRAY);
        DrawText(TextFormat("Ag: %d", jogador->minhaNave->invPrata), 1050, 630, 20, LIGHTGRAY);
        DrawText(TextFormat("Au: %d", jogador->minhaNave->invOuro), 1050, 660, 20, GOLD);

        if (meter && !winn) {
            // Atualiza o ângulo
            meteoro_rot += meteoro_rot_speed * GetFrameTime();

            // Desenha girando a partir do centro
            float w = (float)meteoro_tex.width;
            float h = (float)meteoro_tex.height;
            Rectangle source = { 0.0f, 0.0f, w, h };
            Rectangle dest = { (float)meteoro_x + w/2.0f, (float)meteoro_y + h/2.0f, w, h };
            Vector2 origin = { w/2.0f, h/2.0f };
            DrawTexturePro(meteoro_tex, source, dest, origin, meteoro_rot, WHITE);
            // O Meteoro está parado no espaço, então ele vem na sua direção na SUA velocidade
            float enemy_speed_mod = world_speed_factor; 
            if (meter) {
                bot_y += (fall * enemy_speed_mod) * mult;
            }
            meteoro_y = bot_y;
            meteoro_x = bot_x;
        }
        if (bot_y >= 750 && vida > 0) {
            bot_x = GetRandomValue(30, 1150);
            bot_y = GetRandomValue(-800, -10);
            fall = GetRandomValue(vmin, vmax);
            meteoro_rot_speed = (float)GetRandomValue(30, 150) * ((GetRandomValue(0, 1) == 0) ? 1.0f : -1.0f);
        //    pontos--;
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
                meteoro_rot_speed = (float)GetRandomValue(30, 150) * ((GetRandomValue(0, 1) == 0) ? 1.0f : -1.0f);
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

        // --- GERENCIAMENTO DO BOSS ---
        if (distance_traveled >= (distance_total*0.75f) && !winn && !boss_defeated){
            boss = true;
        }
        if (boss && !winn) {
            if (chefeFinal->vida > 0) {
                bool tocaSomDano = false;
                chefeFinal->ComportamentoVivo(mult, GetFrameTime(), vida, death_x, death_y, tocaSomDano);
                
                // O Boss cuida da matemática, o Jogo apenas toca o som se foi engatilhado
                if (tocaSomDano && death_sound_delay == 0) { 
                    PlaySound(bomb); 
                    death_sound_delay = 20; 
                }
            } 
            else {
                bool engatilhaExplosao = false;
                chefeFinal->ComportamentoMorto(mult, GetFrameTime(), boss_defeated, winn, distance_traveled, distance_total, engatilhaExplosao);
                
                // O Boss cuida dos timers, o Jogo apenas solta a partícula externa e o som
                if (engatilhaExplosao) {
                    PlaySound(bomb);
                    AdicionarExplosao({(float)chefeFinal->x + GetRandomValue(30, 270), (float)chefeFinal->y + GetRandomValue(30, 270)}, MONSTER);
                }
            }
        }

        // INIMIGO NORMAL
        if(distance_traveled >= (distance_total*0.5f) && !enemy && !enemy_defeated && enemylife > 0){
            enemy = true;
            Color paletaInimigos[] = {WHITE, RED, GREEN, BLUE, YELLOW, ORANGE, PURPLE};
            enemyColor = paletaInimigos[GetRandomValue(0, 6)]; 
        }

        // TIRO DO INIMIGO NORMAL 
        if (tiroenemy_ativo) {
            BeginBlendMode(BLEND_ADDITIVE);
            // Visual roxo para diferenciar
            DrawTexturePro(texParticula, {0,0,64,64}, {tiroenemy_x, tiroenemy_y, 16.0f, 32.0f}, {8.0f, 16.0f}, 0.0f, ColorAlpha(PURPLE, 0.7f));
            DrawEllipse(tiroenemy_x, tiroenemy_y, 3.0f, 12.0f, WHITE);
            EndBlendMode();

            // O inimigo está voando do seu lado. O tiro viaja na velocidade DELE, ignora o mundo!
            tiroenemy_y += 5.0f * mult;

            if (tiroenemy_y > 750) tiroenemy_ativo = false;

            // Colisão com o Player
            if (vida > 0 && tiroenemy_y >= death_y && tiroenemy_y <= death_y + 80 && tiroenemy_x >= death_x && tiroenemy_x <= death_x + 50) {
                if (death_sound_delay == 0) { PlaySound(bomb); death_sound_delay = 20; }
                vida = 0;
                tiroenemy_ativo = false; // Some ao acertar
            }
        }

        // --- COMPORTAMENTO DO INIMIGO NORMAL ---
        if(enemy) {
            if (enemylife <= 0) {
                // 1. MORTE ÚNICA
                PlaySound(bomb);
                AdicionarExplosao({(float)enemyx + 30, (float)enemyy + 20}, SMALL_SHIP); 
                
                // 2. DROP GARANTIDO (1 Ouro, 4 Ferros)
                for(int j=0; j<5; j++) {
                    Drop d; 
                    d.pos = {(float)enemyx+30, (float)enemyy+20};
                    float ang = GetRandomValue(180, 360) * DEG2RAD; 
                    float spd = GetRandomValue(50, 90) / 10.0f; 
                    d.vel = { cosf(ang) * spd, sinf(ang) * spd };
                    
                    d.tipo = (j == 0) ? OURO : FERRO; 
                    d.valor = (j == 0) ? 2 : 10; // Ouro vale 2, Ferros valem 10 cada
                    drops.push_back(d);
                }

                pontos += 5; // Recompensa extra
                enemy = false;
                enemy_defeated = true; // Trava ativada! Nunca mais renasce.
            } 
            else {
                // 3. DESENHO E MOVIMENTO
                DrawTexture(enemynav, enemyx, enemyy, enemyColor);
                
                // Barra de Vida / Texto em cima
                DrawText(TextFormat("%d", enemylife), enemyx + 10, enemyy - 5, 20, RED);

                if(enemyy < 150) enemyy += (int)(2 * mult);
                if(enemyy >= 150){
                    enemyy = 150;
                    if(enemyx < nave_x) enemyx += (int)(1.5f * mult); // Segue mais devagar
                    if(enemyx > nave_x) enemyx -= (int)(1.5f * mult);
                }

                // 4. ATIRA NO JOGADOR (Frequência baixa)
                enemy_shoot_timer -= mult;
                if (enemy_shoot_timer <= 0 && !tiroenemy_ativo) {
                    tiroenemy_ativo = true;
                    tiroenemy_x = enemyx + 30; // Meio do inimigo
                    tiroenemy_y = enemyy + 40;
                    enemy_shoot_timer = 150.0f; // Cooldown longo
                }
            }
        }

        DesenharExplosoes();

        if (winn) {
            pisc++;
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
        if ((bot_y >= death_y - 30 && bot_y <= death_y + 50 && bot_x >= death_x - 50 && bot_x <= death_x + 60)) {
            if (death_sound_delay == 0) {
                PlaySound(bomb);
                death_sound_delay = 20; // Atraso de 10 frames
            }
            vida--;
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
                DrawText("PRESS R TO RESPAWN", 450, 420, 20, RED);
            }
            if (pisc >= 200) {
                pisc = 0;
            }
            if (IsKeyPressed(KEY_R)) {
                winn = false;
                vida = 10;
                pontos = 1;
                bum = 0;
                boss_defeated = false;
                jogador->minhaNave->combustivelAtual = jogador->minhaNave->combustivelMaximo;
                jogador->minhaNave->velocidadeAtual = (float)GetRandomValue(80, 2500); // Sorteia o lançamento de novo
                if (boss) chefeFinal->Resetar(); 
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
        DrawText("CHOOSE THE LEVEL OF DENSITY", 270, 250, 40, WHITE);
        DrawText("1 - LOW (Better Performance)", 400, 350, 20, GRAY);
        DrawText("2 - MEDIUM", 400, 380, 20, GRAY);
        DrawText("3 - HIGH (Better Visuals)", 400, 410, 20, GRAY);
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
    nav = LoadTexture("SpriteSheetNave2.png"); mort = LoadTexture("mort.png");
    meteoro_tex = LoadTexture("meteoro.png"); // enemynav = LoadTexture("navinimiga.png");
    if(enemytype == 1){
        enemynav = LoadTexture("navinimiga.png");
    }
    else{
        enemynav = LoadTexture("navinimiga2.png");
    }

    // --- GERAÇÃO DA TEXTURA DE PARTÍCULA ---
    Image imgExp = GenImageGradientRadial(64, 64, 0.0f, WHITE, BLACK);
    texParticula = LoadTextureFromImage(imgExp);
    chefeFinal = new Boss();
    UnloadImage(imgExp);

    chefeFinal = new Boss();
    jogador = new Player("Goku"); // Cria o player e a nave dele automaticamente
    
    // Simula o resultado aleatório do futuro minigame de lançamento (ex: 80 a 200 de vel inicial)
    jogador->minhaNave->velocidadeAtual = (float)GetRandomValue(80, 1500);

    // --- INICIALIZAÇÃO DAS VARIÁVEIS ---
    fall = GetRandomValue(1, 4); exp_x = nave_x; exp_y = nave_y; death_x = nave_x + 25; death_y = nave_y + 10; fire_x = nave_x + 30;
    fire_y = nave_y + 84; sol_x = GetRandomValue(10, 1100); sol_y = GetRandomValue(10, 400);
    bot_x = GetRandomValue(50, 1100); bot_y = -10; expm_x = bot_x + 200; expm_y = bot_y + 100; meteoro_x = bot_x; meteoro_y = bot_y;
    meteoro_rot_speed = (float)GetRandomValue(30, 150) * ((GetRandomValue(0, 1) == 0) ? 1.0f : -1.0f);
    hitbox1x = nave_x + 10; hitbox1y = nave_y + 40; hitbox2x = nave_x + 38; hitbox2y = nave_y + 10;

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
    UnloadTexture(menui); UnloadTexture(credits); UnloadTexture(nav); UnloadTexture(mort);
    UnloadTexture(meteoro_tex); UnloadTexture(enemynav);
    UnloadMusicStream(background_music);
    for (int i = 0; i < 5; i++) UnloadSound(sob[i]);
    UnloadSound(bomb); UnloadSound(ai); UnloadSound(oh);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}