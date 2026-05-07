#include "raylib.h"
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <cmath>
#include "boss.h"
#include "Player.h"
#include "Nave.h"
#include "explosao.h"
#include <fstream>

struct EventoViagem {
    int inicioAL;
    int fimAL;
    bool processado = false;
    float tempoRandomBoss;
    int framesNoEscuro = 0;
    int framesParaAtacar = 0;
    bool calculouTempo = false;
};

std::vector<EventoViagem> planoMeteoros;
std::vector<EventoViagem> planoPiratas;
std::vector<EventoViagem> planoBosses;

// 1 Ano-Luz no Mapa = 100 unidades de distância no Space Shooter (Ajuste isso depois se achar muito curto/longo)
const int MULTIPLICADOR_AL = 300;

Texture2D menui, credits, nav, mort, fire1, meteoro_tex, enemynav, texParticula;
Sound sob[5], bomb, ai, oh;
Music background_music;

float mult = 1.0f;

float distance_total = 0.0f; // Será preenchido pelo arquivo txt
float distance_traveled = 0.0f;
float distance_left = 0.0f;

// --- EVENTO CINTURÃO DE ASTEROIDES ---
bool temCinturao = false;
float timerZonaSegura = 0.0f;
bool estavaNoCinturao = false;

Boss* chefeFinal = nullptr; 
bool boss_defeated = false;

struct Drop {
    Vector2 pos; // Usando Vector2 para facilitar a física
    Vector2 vel; // Velocidade da explosão
    int valor;   // Quanto este pedacinho vale
    TipoMinerio tipo;
    bool isCombustivel = false;
};
// A lista e os inventários continuam iguais:
std::vector<Drop> drops;
Player* jogador = nullptr;
GerenciadorDeExplosoes* fxExplosoes = nullptr;


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

struct Meteoro {
    Vector2 pos;
    float fall;
    float rot;
    float rotSpeed;
    bool ativo;
    Vector2 velIndependente; // <--- NOVO: Velocidade própria no espaço
    bool isEmergencia;
};
std::vector<Meteoro> meteoros; // A lista que guarda todos os meteoros

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
int bum = 0, vmin = 1, vmax = 4, pisc = 0;
int mort_x = 250, mort_y = 70;
int pontos = 0, storo1 = 0, storo2 = 0, storo3 = 0, ng = 0;
int nave_x = 390, nave_y = 600, velo = 3, tvel = 5;
int exp_x, exp_y;
int death_x, death_y, vida = 10;
int fire_x, fire_y;
int foga = 0, fogd = 0, cdpause = 0;
int sol_x, sol_y;
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

        d.isCombustivel = false;
        drops.push_back(d);
    }
}

void CarregarPlanoDeVoo() {
    std::ifstream arquivo("viagem_data.txt");
    
    // Se por acaso o jogador abrir o SpaceShooter direto pelo executável sem passar pelo Mapa,
    // o arquivo não vai existir, então o jogo roda com valores padrão pra não crashar.
    if (!arquivo.is_open()) return; 

    std::string chave;
    // O C++ é inteligente: ele lê a primeira palavra, vê o que é, e puxa os números seguintes!
    while (arquivo >> chave) {
        if (chave == "PLAYER_NOME") { arquivo >> jogador->nome; }
        else if (chave == "PLAYER_VIDA") { arquivo >> jogador->vidaAtual >> jogador->vidaMaxima; }
        else if (chave == "PLAYER_KI") { arquivo >> jogador->kiAtual >> jogador->kiMaximo; }
        else if (chave == "PLAYER_PDL") { arquivo >> jogador->pdlAtual >> jogador->pdlMaximo; }
        else if (chave == "PLAYER_DINHEIRO") { arquivo >> jogador->dinheiro; }
        
        else if (chave == "NAVE_COMBUSTIVEL") { arquivo >> jogador->minhaNave->combustivelAtual >> jogador->minhaNave->combustivelMaximo; }
        else if (chave == "NAVE_ESCUDO") { arquivo >> jogador->minhaNave->escudoAtual >> jogador->minhaNave->escudoMaximo; }
        else if (chave == "NAVE_MINERIOS") { arquivo >> jogador->minhaNave->invFerro >> jogador->minhaNave->invPrata >> jogador->minhaNave->invOuro; }
       // else if (chave == "NAVE_UPGRADES") { arquivo >> jogador->minhaNave->forcaTurbo >> jogador->minhaNave->eficienciaCombustivel >> jogador->minhaNave->taxaConsumoBase; }
        
        else if (chave == "VIAGEM_DISTANCIA") { 
            int distAL; arquivo >> distAL; 
            distance_total = distAL * MULTIPLICADOR_AL;
            distance_left = distance_total; // Reseta a distância atual
        }
        
        else if (chave == "QTD_METEOROS") {
            int qtd; arquivo >> qtd;
            for (int i = 0; i < qtd; i++) { 
                EventoViagem e; 
                arquivo >> e.inicioAL >> e.fimAL; 
                // Multiplica a distância do evento pela escala do jogo
                e.inicioAL *= MULTIPLICADOR_AL; 
                e.fimAL *= MULTIPLICADOR_AL;
                planoMeteoros.push_back(e); 
            }
        }
        else if (chave == "QTD_PIRATAS") {
            int qtd; arquivo >> qtd;
            for (int i = 0; i < qtd; i++) { 
                EventoViagem e; 
                arquivo >> e.inicioAL >> e.fimAL; 
                e.inicioAL *= MULTIPLICADOR_AL;
                e.fimAL *= MULTIPLICADOR_AL;
                planoPiratas.push_back(e); 
            }
        }
        else if (chave == "QTD_BOSSES") {
            int qtd; arquivo >> qtd;
            for (int i = 0; i < qtd; i++) { 
                EventoViagem e; 
                arquivo >> e.inicioAL >> e.fimAL; 
                e.inicioAL *= MULTIPLICADOR_AL;
                e.fimAL *= MULTIPLICADOR_AL;
                e.tempoRandomBoss = (float)GetRandomValue(5, 10); // Sorteia o tempo de "chegada" do boss
                planoBosses.push_back(e); 
            }
        }
    }
    arquivo.close();
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
            
            //cadencia do tiro
            cooldownTiro = 50.0f - (jogador->minhaNave->levelTiro * 5.0f); 
            if (cooldownTiro < 5.0f) cooldownTiro = 5.0f;
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
        if (nave_y < 20) nave_y = 20;

        exp_x = nave_x;
        hitbox1x = nave_x + 10; hitbox2x = nave_x + 38;
        exp_x = nave_x;
        exp_y = nave_y;
        hitbox1y = nave_y + 40; hitbox2y = nave_y + 10;
        exp_y = nave_y;
        hitbox1y = nave_y + 40; hitbox2y = nave_y + 10;
        fire_x = nave_x + 49;
        fire_y = nave_y + 84;
        death_x = nave_x - 30;
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
    } 
    else {
        ClearBackground(BLACK);

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
    // A trava: Se o boss está ativo e não foi derrotado, a distância congela!
    // =================================================================
    // --- LÓGICA DE BLOQUEIO DO BOSS E RECOMPENSAS ---
    // =================================================================
    static float timerLiberarNave = 0.0f;
    static bool dropouGasol = false;
    bool bloqueioBoss = false;

    if (boss) {
        // ATENÇÃO: Substitua "vidaAtual" caso a variável de HP do seu chefe tenha outro nome!
        if (chefeFinal->vida > 0) {
            bloqueioBoss = true;
            timerLiberarNave = 0.0f;
            dropouGasol = false;
        } 
        else {
            // O Boss explodiu (HP <= 0), mas as peças ainda estão caindo...
            
            // 1. Drop de Combustível (Roda uma única vez)
            if (!dropouGasol) {
                for(int j=0; j<5; j++) { // Solta 5 orbs de gasol
                    Drop d; 
                    d.pos = {(float)enemyx+30, (float)enemyy+20};
                    float ang = GetRandomValue(180, 360) * DEG2RAD; 
                    float spd = GetRandomValue(30, 60) / 10.0f; 
                    d.vel = { cosf(ang) * spd, sinf(ang) * spd };
                    d.isCombustivel = true; // <--- É COMBUSTÍVEL
                    d.valor = 15; // Cada bolha recupera 15 de combustível
                    drops.push_back(d);
                }
                dropouGasol = true;
            }

            // 2. Timer de Liberação da Nave
            timerLiberarNave += GetFrameTime();
            
            if (timerLiberarNave < 3.0f) {
                bloqueioBoss = true; // Segura a nave por mais 3 segundos de suspense
            } else {
                bloqueioBoss = false; 
                boss = false;
                boss_defeated = true;
            }
        }
    } else {
        timerLiberarNave = 0.0f;
        dropouGasol = false;
    }

    if (distance_left > 0 && vida > 0) {
        
        // 1. Pega o tempo do frame, mas limita a no máximo 0.1 segundos
        float dtSeguro = GetFrameTime();
        if (dtSeguro > 0.1f) dtSeguro = 0.1f; 

        // 2. Usa o dtSeguro ao invés do GetFrameTime()
        float avanco_exato = (jogador->minhaNave->velocidadeAtual * dtSeguro) * mult; 
        
        distance_traveled += avanco_exato;
        distance_left -= avanco_exato;
            
        if (distance_left < 0.0f) {
            distance_left = 0.0f;
            distance_traveled = distance_total;
        }
    }

    // 2. DESACELERAÇÃO FORÇADA PELO BOSS:
    if (bloqueioBoss) {
        if (jogador->minhaNave->velocidadeAtual > 50.0f) {
            // Cai 1km/s por frame, dando aquela sensação de estar sendo freado brutalmente
            jogador->minhaNave->velocidadeAtual -= 1.0f; 
            
            if (jogador->minhaNave->velocidadeAtual < 50.0f) {
                jogador->minhaNave->velocidadeAtual = 50.0f; // Trava o piso em 50km/s
            }
        }
    }

    // =================================================================
    // --- DIRETOR DE EVENTOS ---
    // =================================================================
    temCinturao = false;
    
    bool inPirateZone = false;
    bool inBossZone = false;
    bool avisoMeteoro = false, avisoPirata = false, avisoBoss = false;

    float distAviso = jogador->minhaNave->velocidadeAtual * 3.0f;
    if (distAviso < 3000.0f) distAviso = 3000.0f; // Distância mínima de aviso

    // Scanner de Meteoros e Piratas
    for (const auto& m : planoMeteoros) {
        if (distance_traveled >= m.inicioAL - distAviso && distance_traveled < m.inicioAL) avisoMeteoro = true;
        if (distance_traveled >= m.inicioAL && distance_traveled <= m.fimAL) { temCinturao = true; }
    }
    for (const auto& p : planoPiratas) {
        if (distance_traveled >= p.inicioAL - distAviso && distance_traveled < p.inicioAL) avisoPirata = true;
        if (distance_traveled >= p.inicioAL && distance_traveled <= p.fimAL) inPirateZone = true;
    }

    // =================================================================
    // 3. Scanner de Bosses - Lógica de Coordenadas Fixas
    // =================================================================
    for (int i = 0; i < (int)planoBosses.size(); i++) {
        auto& b = planoBosses[i];
        
        bool fisicamenteNaZona = (distance_traveled >= b.inicioAL && distance_traveled <= b.fimAL);

        if (fisicamenteNaZona) {
            // REGRA VISUAL: Enquanto estiver no abismo, as estrelas somem.
            inBossZone = true; 

            // REGRA LÓGICA: Só tenta spawnar se a zona ainda for inédita
            if (!b.processado && !boss) {
                if (!b.calculouTempo) {
                    b.framesParaAtacar = GetRandomValue(300, 900);
                    b.framesNoEscuro = 0;
                    b.calculouTempo = true;
                }

                b.framesNoEscuro++; 
                
                if (b.framesNoEscuro >= b.framesParaAtacar) {
                    boss = true;
                    boss_defeated = false;
                    b.processado = true; 
                    chefeFinal->Resetar();
                }
            }
        } 
        else if (distance_traveled > b.fimAL && !b.processado) {
            b.processado = true; 
        }
        
        // Alerta do Radar (só aparece se o boss ainda não foi ativado/passado)
        if (distance_traveled >= b.inicioAL - distAviso && distance_traveled < b.inicioAL && !b.processado) {
            avisoBoss = true;
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
            s.base_size = GetRandomValue(1, 2); 
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
            if (!inBossZone) { // <--- TRAVA DO ABISMO AQUI
                s.position = {(float)GetRandomValue(0, 1200), (float)GetRandomValue(-150, -10)};
                s.speed_delay = GetRandomValue(1, 7); 
                
                s.base_size = (12 - s.speed_delay) / 2;
                if(s.base_size < 3) s.base_size = 3;
                
                s.growth_range = (s.base_size > 4) ? 9 : 6;
            }
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
            if (!inBossZone) { // <--- TRAVA DO ABISMO AQUI
                n.position = {(float)GetRandomValue(0, 1200), (float)GetRandomValue(-500, -300)};
                Color COLOR_ROXO = {128, 0, 128, 255};
                std::vector<Color> nebula_colors = {BLUE, COLOR_ROXO, RED, GREEN, YELLOW, WHITE};
                n.base_color = nebula_colors[GetRandomValue(0, nebula_colors.size() - 1)];
                GenerateNebulaShape(n, PARTICLES_PER_NEBULA, STAR_PARTICLE_COUNT);
            }
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

        DrawText("Pause: P", 20, 70, 20, WHITE);
        DrawText("Turbo: K", 20, 90, 20, WHITE);
        DrawText("Break: L", 20, 110, 20, WHITE);
        DrawText(TextFormat("VELOCIDADE: %d km/s", (int)jogador->minhaNave->velocidadeAtual), 20, 140, 20, SKYBLUE);
        DrawText(TextFormat("COMBUSTÍVEL: %d / %d", (int)jogador->minhaNave->combustivelAtual, (int)jogador->minhaNave->combustivelMaximo), 20, 160, 20, ORANGE);
        DrawText(TextFormat("ESCUDO: %d N", jogador->minhaNave->escudoAtual), 20, 180, 20, SKYBLUE); 
        
        // Sincroniza a morte: Se o escudo zerar, a 'vida' zera para disparar a tela de Game Over
        if (jogador->minhaNave->escudoAtual <= 0) vida = 0;

        char distance_str[20];
        sprintf(distance_str, "%.0f       ", distance_left);
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

                // A) Colisão com a LISTA DE METEOROS
                bool tiroAcertou = false;
                for (int m = 0; m < (int)meteoros.size(); m++) {
                    if (!meteoros[m].ativo) continue;

                    if (tiros[i].y <= meteoros[m].pos.y + 40 &&       
                        tiros[i].y >= meteoros[m].pos.y - 20 && 
                        tiros[i].x >= meteoros[m].pos.x &&            
                        tiros[i].x <= meteoros[m].pos.x + 60) {       
                        
                        PlaySound(bomb);
                        fxExplosoes->AdicionarExplosao({meteoros[m].pos.x + 30, meteoros[m].pos.y + 20}, METEOR); 
                        
                        // Lógica de Sobrevivência (Game Design)
                        if (meteoros[m].isEmergencia) {
                            // O de emergência SEMPRE dropa 3 combustíveis
                            for(int d=0; d<3; d++) { 
                                Drop dropComb; 
                                dropComb.pos = {meteoros[m].pos.x + 30, meteoros[m].pos.y + 20};
                                float ang = GetRandomValue(180, 360) * DEG2RAD; 
                                dropComb.vel = { cosf(ang) * (GetRandomValue(30, 60)/10.0f), sinf(ang) * (GetRandomValue(30, 60)/10.0f) };
                                dropComb.isCombustivel = true; dropComb.valor = 15; 
                                drops.push_back(dropComb);
                            }
                        } 
                        else if (jogador->minhaNave->combustivelAtual <= 0.0f && GetRandomValue(0, 1) == 0) {
                            // Se tá sem gasolina e destruiu um meteoro normal: 50% de chance de dropar 1 combustível
                            Drop dropComb; 
                            dropComb.pos = {meteoros[m].pos.x + 30, meteoros[m].pos.y + 20};
                            float ang = GetRandomValue(180, 360) * DEG2RAD; 
                            dropComb.vel = { cosf(ang) * 4.0f, sinf(ang) * 4.0f };
                            dropComb.isCombustivel = true; dropComb.valor = 10; 
                            drops.push_back(dropComb);
                        } 
                        else {
                            // Drop normal de minérios
                            SpawnLoot({meteoros[m].pos.x + 30, meteoros[m].pos.y + 20});
                        }
                        
                        meteoros[m].ativo = false; // Destrói o meteoro
                        tiroAcertou = true;
                        break; // O tiro já explodiu, para de checar os outros meteoros
                    }
                }
                
                if (tiroAcertou) {
                    tiros.erase(tiros.begin() + i); // Remove o tiro
                    i--; 
                    continue; // Pula pro próximo tiro
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

            // O relógio do iFrame agora roda independente da nave estar viajando ou parada no Boss
            if (jogador->minhaNave->iFrame > 0.0f) {
                jogador->minhaNave->iFrame -= GetFrameTime();
            }
            
            // Recorte do SpriteSheet
            Rectangle sourceRec = { (float)(frame_atual * 100), 0.0f, 100.0f * direcao_textura, 100.0f };
            Vector2 posNave = { (float)nave_x, (float)nave_y };

            // A nave pisca (mostrarNave) durante o iFrame
            bool mostrarNave = !(jogador->minhaNave->iFrame > 0.0f && ((int)(GetTime() * 15) % 2 == 0));

            // Desenha a nave normalmente
            if (mostrarNave && jogador->minhaNave->escudoAtual > 0) {
                DrawTextureRec(nav, sourceRec, posNave, WHITE);
            }

            // O Campo de Força SÓ aparece quando toma dano (no iFrame)
            if (jogador->minhaNave->iFrame > 0.0f) {
                // Matemática do pulso (mantemos a mesma para sincronizar tudo)
                float pulsoEscudo = sin(GetTime() * 20.0f) * 10.0f; 
                float baseRaio = 75.0f + (pulsoEscudo / 2.0f); // Raio base para a estrutura
                float tamanhoGlow = baseRaio * 3.0f;           // Tamanho total para a textura de brilho

                // Centralização exata (baseada nos testes anteriores de hitbox)
                int centerX = nave_x + 50;
                int centerY = nave_y + 45;

                // --- 1. O Brilho Volumétrico (Blend Additive / Luz) ---
                BeginBlendMode(BLEND_ADDITIVE); 
                
                // Aura azul externa difusa
                DrawTexturePro(texParticula, 
                    {0, 0, 64, 64}, 
                    { (float)centerX, (float)centerY, tamanhoGlow, tamanhoGlow }, 
                    { tamanhoGlow / 2.0f, tamanhoGlow / 2.0f }, 
                    0.0f, ColorAlpha(BLUE, 0.5f)); 
                
                // Núcleo branco no centro (efeito "brilho da lâmpada")
                DrawTexturePro(texParticula, 
                    {0, 0, 64, 64}, 
                    { (float)centerX, (float)centerY, tamanhoGlow * 0.6f, tamanhoGlow * 0.6f }, 
                    { (tamanhoGlow * 0.6f) / 2.0f, (tamanhoGlow * 0.6f) / 2.0f }, 
                    0.0f, ColorAlpha(SKYBLUE, 0.3f));

                Color corCasca = ColorAlpha(SKYBLUE, 0.5f);
                // Desenha a linha da elipse (raioH e raioV ligeiramente diferentes para dar perspectiva)
                DrawEllipseLines(centerX, centerY, baseRaio, baseRaio * 0.95f, corCasca);
                    
                EndBlendMode(); // Encerra o modo luz    
            }

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

        // SE FOR COMBUSTÍVEL, DESCE DEVAGAR INDEPENDENTE DA NAVE. SE FOR LOOT, É ENGOLIDO PELA INÉRCIA.
        if (drops[i].isCombustivel) drops[i].pos.y += 2.0f * mult; 
        else drops[i].pos.y += (3.0f * world_speed_factor) * mult;

        // --- DESENHO COM BRILHO ---
        Color corLoot, corBorda;
        float raio;
        
        if (drops[i].isCombustivel) {
            corLoot = RED; corBorda = RED; raio = 8.0f; // Combustível Vermelho Vivo
        } else if (drops[i].tipo == FERRO) { 
            corLoot = {100, 100, 100, 255}; corBorda = WHITE; raio = 6.0f; 
        } else if (drops[i].tipo == PRATA) { 
            corLoot = {200, 200, 255, 255}; corBorda = BLUE; raio = 7.0f; 
        } else { 
            corLoot = GOLD; corBorda = ORANGE; raio = 8.0f; 
        }

        DrawCircleV(drops[i].pos, raio + 3, ColorAlpha(corBorda, 0.4f)); 
        DrawCircleV(drops[i].pos, raio, corLoot);

        // --- COLETA (HITBOX RETANGULAR LARGA) ---
        // Centro da nave
        float naveCenterX = (float)nave_x + 24;
        float naveCenterY = (float)nave_y + 40;

        // Verifica a distância X e Y separadamente
        // Distância X < 60 (Significa 120 pixels de largura total de coleta!)
        // Distância Y < 40 (Significa 80 pixels de altura)
        if (fabs(drops[i].pos.x - naveCenterX) < 60 && 
            fabs(drops[i].pos.y - naveCenterY) < 40) {
            
            if (drops[i].isCombustivel) {
                jogador->minhaNave->combustivelAtual += drops[i].valor;
                // Não deixa passar do limite do tanque
                if (jogador->minhaNave->combustivelAtual > jogador->minhaNave->combustivelMaximo) {
                    jogador->minhaNave->combustivelAtual = jogador->minhaNave->combustivelMaximo;
                }
            } else {
                if (drops[i].tipo == FERRO) jogador->minhaNave->GuardarMinerio(FERRO, drops[i].valor);
                else if (drops[i].tipo == PRATA) jogador->minhaNave->GuardarMinerio(PRATA, drops[i].valor);
                else jogador->minhaNave->GuardarMinerio(OURO, drops[i].valor);
            }

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

        // --- BARRA DE PROGRESSO DA VIAGEM (HUD) ---
    if (distance_total > 0.0f) {
        float startX = 200.0f;
        float endX = 800.0f;
        float barY = 40.0f; // Altura no topo da tela
        float barWidth = endX - startX;

        // 1. Fundo da Barra (Cinza vazio)
        DrawLineEx({startX, barY}, {endX, barY}, 6.0f, Fade(GRAY, 0.3f));

        // 2. Marcações de Eventos na Barra (Excelente para Debug e Game Feel)
        auto DesenharTrechoBarra = [&](float inicio, float fim, Color cor) {
            float x1 = startX + (barWidth * (inicio / distance_total));
            float x2 = startX + (barWidth * (fim / distance_total));
            
            // Travas de segurança para não desenhar fora da barra
            if (x1 < startX) x1 = startX;
            if (x2 > endX) x2 = endX;
            
            if (x1 < x2) DrawLineEx({x1, barY}, {x2, barY}, 8.0f, Fade(cor, 0.7f));
        };

        // Pinta os trechos correspondentes de cada evento
        for (const auto& m : planoMeteoros) DesenharTrechoBarra(m.inicioAL, m.fimAL, RED);
        for (const auto& p : planoPiratas) DesenharTrechoBarra(p.inicioAL, p.fimAL, PURPLE);
        for (const auto& b : planoBosses) DesenharTrechoBarra(b.inicioAL, b.fimAL, BLACK); 

        // 3. Progresso Concluído (Linha Verde)
        float pct = distance_traveled / distance_total;
        if (pct < 0.0f) pct = 0.0f; 
        if (pct > 1.0f) pct = 1.0f;
        
        float playerX = startX + (barWidth * pct);
        DrawLineEx({startX, barY}, {playerX, barY}, 6.0f, GREEN);

        // 4. Bolinha do Jogador
        DrawCircle((int)playerX, (int)barY, 8.0f, WHITE);
        DrawCircleLines((int)playerX, (int)barY, 10.0f, GREEN);

        // 5. Textos Laterais
        DrawText("ORIGEM", (int)startX - 60, (int)barY - 5, 10, LIGHTGRAY);
        DrawText("DESTINO", (int)endX + 15, (int)barY - 5, 10, LIGHTGRAY);
    }

        // AVISOS DO RADAR 
        
        float avisoY = 60.0f; // Altura fixa: 40 da barra + 20 de margem
        int fontSize = 15; 

        if (avisoMeteoro) {
            const char* txt = "CINTURAO DE ASTEROIDES A FRENTE: REDUZA A VELOCIDADE!";
            int txtW = MeasureText(txt, fontSize);
            DrawText(txt, (GetScreenWidth() / 2) - (txtW / 2), avisoY, fontSize, RED);
        } 
        else if (avisoPirata) {
            const char* txt = "NAVE INIMIGA ANALISANDO PADRÃO DE VOO!";
            int txtW = MeasureText(txt, fontSize);
            DrawText(txt, (GetScreenWidth() / 2) - (txtW / 2), avisoY, fontSize, PURPLE);
        } 
        else if (avisoBoss) {
            const char* txt = "ENTRANDO NO ABISMO GALÁTICO!";
            int txtW = MeasureText(txt, fontSize);
            DrawText(txt, (GetScreenWidth() / 2) - (txtW / 2), avisoY, fontSize, DARKGRAY);
        } 
        else if (timerZonaSegura > 0.0f) {
            const char* txt = "ZONA SEGURA: PODE ACELERAR!";
            int txtW = MeasureText(txt, fontSize);
            DrawText(txt, (GetScreenWidth() / 2) - (txtW / 2), avisoY, fontSize, GREEN);
        }

        // 1.5. Aviso de Zona Segura 
        estavaNoCinturao = false;
        timerZonaSegura = 0.0f;
        bool semMeteoros = true;
        for (auto& m : meteoros) if (m.ativo) semMeteoros = false;

        if (temCinturao) estavaNoCinturao = true;
        if (!temCinturao && estavaNoCinturao && semMeteoros) {
            timerZonaSegura = 3.0f; // Avisa por 3 segundos
            estavaNoCinturao = false;
        }

        // 2. Lógica de Máximo de Meteoros (Cinturão VERDADEIRO)
        bool velocidadeBaixa = (jogador->minhaNave->velocidadeAtual <= 500.0f);
        int limiteMeteoros = 0;
        
        if (!winn && !boss) {
            if (temCinturao) {
                limiteMeteoros = 10; // 15 METEOROS AO MESMO TEMPO NA TELA!
            } else if (velocidadeBaixa) {
                limiteMeteoros = 2; // Farm tranquilo
            }
        }

        // 2.5 Conta meteoros e checa se já tem um de emergência na tela
        int meteorosAtivos = 0;
        bool temEmergenciaNaTela = false;
        for (auto& m : meteoros) {
            if (m.ativo) {
                meteorosAtivos++;
                if (m.isEmergencia) temEmergenciaNaTela = true;
            }
        }

        // --- SISTEMA ANTI-SOFTLOCK (Meteoro de Emergência) ---
        // Se a nave está parada E sem combustível, spawna 1 meteoro viajante de vez em quando
        if (jogador->minhaNave->velocidadeAtual <= 0.0f && jogador->minhaNave->combustivelAtual <= 0.0f && !temEmergenciaNaTela && !winn && !boss) {
            // Chance rara por frame para ele aparecer ocasionalmente, e não toda hora
            if (GetRandomValue(0, 100) == 1) { 
                Meteoro emg;
                emg.ativo = true;
                emg.isEmergencia = true;
                emg.fall = 0; 
                emg.rot = 0.0f;
                emg.rotSpeed = (float)GetRandomValue(30, 150) * ((GetRandomValue(0, 1) == 0) ? 1.0f : -1.0f);

                // Sorteia a origem: 0=Topo, 1=Esquerda, 2=Direita
                int origem = GetRandomValue(0, 2);
                if (origem == 0) { // Topo
                    emg.pos = { (float)GetRandomValue(100, 1100), -100.0f };
                    emg.velIndependente = { (float)GetRandomValue(-30, 30) / 10.0f, (float)GetRandomValue(20, 50) / 10.0f };
                } else if (origem == 1) { // Esquerda
                    emg.pos = { -100.0f, (float)GetRandomValue(50, 400) };
                    emg.velIndependente = { (float)GetRandomValue(20, 50) / 10.0f, (float)GetRandomValue(-10, 30) / 10.0f };
                } else { // Direita
                    emg.pos = { 1300.0f, (float)GetRandomValue(50, 400) };
                    emg.velIndependente = { (float)GetRandomValue(-50, -20) / 10.0f, (float)GetRandomValue(-10, 30) / 10.0f };
                }
                meteoros.push_back(emg);
                meteorosAtivos++;
            }
        }

        // --- SPAWN NORMAL DE METEOROS ---
        while (meteorosAtivos < limiteMeteoros) {
            Meteoro novo;
            novo.pos.x = (float)GetRandomValue(30, 1150);
            novo.pos.y = (float)GetRandomValue(-800, -100); 
            novo.fall = (float)GetRandomValue(4, 9);
            novo.rot = 0.0f;
            novo.rotSpeed = (float)GetRandomValue(30, 150) * ((GetRandomValue(0, 1) == 0) ? 1.0f : -1.0f);
            novo.ativo = true;
            novo.isEmergencia = false;
            novo.velIndependente = {0.0f, 0.0f};
            meteoros.push_back(novo);
            meteorosAtivos++;
        }

        // 3. Física, Desenho e Colisão de TODOS os Meteoros
        for (int m = 0; m < (int)meteoros.size(); m++) {
            if (!meteoros[m].ativo) continue;

            // Movimento rasgando a tela
            // Movimento: Independente se for emergência, relativo se for normal
            if (meteoros[m].isEmergencia) {
                meteoros[m].pos.x += meteoros[m].velIndependente.x * mult;
                meteoros[m].pos.y += meteoros[m].velIndependente.y * mult;
            } else {
                meteoros[m].pos.y += (meteoros[m].fall * world_speed_factor) * mult;
            }
            meteoros[m].rot += meteoros[m].rotSpeed * GetFrameTime();

            // Desenho
            float w = (float)meteoro_tex.width; 
            float h = (float)meteoro_tex.height;
            DrawTexturePro(meteoro_tex, { 0.0f, 0.0f, w, h }, 
                           { meteoros[m].pos.x + w/2.0f, meteoros[m].pos.y + h/2.0f, w, h }, 
                           { w/2.0f, h/2.0f }, meteoros[m].rot, WHITE);

            // Colisão com a Nave (Escudo e I-frames)
            if (jogador->minhaNave->escudoAtual > 0 && 
                meteoros[m].pos.y >= nave_y - 10 && meteoros[m].pos.y <= nave_y + 80 && 
                meteoros[m].pos.x >= nave_x - 30 && meteoros[m].pos.x <= nave_x + 65) {
                
                if (jogador->minhaNave->iFrame <= 0.0f) {
                    jogador->minhaNave->escudoAtual--;
                    jogador->minhaNave->iFrame = 2.0f;
                    PlaySound(bomb);
                }

                // --- NOVA LÓGICA DE EXPLOSÃO NO ESCUDO ---
                // Pega os centros exatos
                float centroEscudoX = nave_x + 50.0f;
                float centroEscudoY = nave_y + 45.0f;
                float centroMeteoroX = meteoros[m].pos.x + 30.0f;
                float centroMeteoroY = meteoros[m].pos.y + 20.0f;

                // Calcula a direção do impacto (Vetor Direcional)
                float dx = centroMeteoroX - centroEscudoX;
                float dy = centroMeteoroY - centroEscudoY;
                float distancia = sqrt(dx*dx + dy*dy);

                Vector2 posExplosao = {centroMeteoroX, centroMeteoroY};

                // Empurra a explosão exatamante para a "casca" do escudo (raio de 75 pixels)
                if (distancia > 0) {
                    posExplosao.x = centroEscudoX + (dx / distancia) * 75.0f;
                    posExplosao.y = centroEscudoY + (dy / distancia) * 75.0f;
                }

                fxExplosoes->AdicionarExplosao(posExplosao, METEOR); 
                
                meteoros[m].ativo = false; // Desativa a pedra
            }

            // Limpeza: Se saiu da tela, desativa para o Spawner criar outro lá em cima
            if (meteoros[m].pos.y > 800 || meteoros[m].pos.x < -250 || meteoros[m].pos.x > 1450 || meteoros[m].pos.y < -250) {
                meteoros[m].ativo = false;
            }
        }
        
        // Remove os inativos da memória para o vetor não crescer infinitamente
        for (int m = 0; m < (int)meteoros.size(); m++) {
            if (!meteoros[m].ativo) {
                meteoros.erase(meteoros.begin() + m);
                m--;
            }
        }

        exp_x = nave_x;
        hitbox1x = nave_x + 10; hitbox2x = nave_x + 38;
        exp_x = nave_x;
        exp_y = nave_y;
        hitbox1y = nave_y + 40; hitbox2y = nave_y + 10;
        exp_y = nave_y;
        hitbox1y = nave_y + 40; hitbox2y = nave_y + 10;

        // =================================================================
        // --- ATIVADORES DE COMBATE ---
        // =================================================================

        // 1. GATILHO DO PIRATA
        static bool pirataResetadoNestaZona = false;
        if (inPirateZone) {
            if (!pirataResetadoNestaZona) {
                enemy = true;
                enemylife = 5;
                enemy_defeated = false;
                Color paletaInimigos[] = {WHITE, RED, GREEN, BLUE, YELLOW, ORANGE, PURPLE};
                enemyColor = paletaInimigos[GetRandomValue(0, 6)]; 
                pirataResetadoNestaZona = true;
            }
        } else {
            pirataResetadoNestaZona = false; // Se tiver outra zona roxa, spawna outro!
        }

        if ((boss || boss_defeated) && !winn) {
            if (chefeFinal->vida > 0) {
                bool tocaSomDano = false;
                // Passamos o 'jogador' em vez da 'vida', e a hitbox ajustada (nave_x + 20, nave_y)
                chefeFinal->ComportamentoVivo(mult, GetFrameTime(), jogador, nave_x + 20, nave_y, tocaSomDano);
                
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
                    // Dispara apenas 1 explosão no centro do chefe
                    fxExplosoes->AdicionarExplosao({(float)chefeFinal->x + 150.0f, (float)chefeFinal->y + 150.0f}, MONSTER);
                }
            }
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

            // Colisão do Tiro Inimigo com o Player
            // nave_x + 20 empurra a hitbox do tiro para a direita, centralizando na lataria
            if (vida > 0 && tiroenemy_y >= nave_y && tiroenemy_y <= nave_y + 80 && 
                tiroenemy_x >= nave_x + 20 && tiroenemy_x <= nave_x + 70) {
                if (death_sound_delay == 0) { PlaySound(bomb); death_sound_delay = 20; }
                jogador->minhaNave->escudoAtual--;
                tiroenemy_ativo = false; 
            }
        }

        // --- COMPORTAMENTO DO INIMIGO NORMAL ---
        if(enemy) {
            if (enemylife <= 0) {
                // 1. MORTE ÚNICA
                PlaySound(bomb);
                fxExplosoes->AdicionarExplosao({(float)enemyx + 30, (float)enemyy + 20}, SMALL_SHIP); 
                
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
                // 2.5 DROP DE COMBUSTÍVEL (Exclusivo do Inimigo)
                for(int j=0; j<3; j++) { // Solta 3 bolhas de energia
                    Drop d; 
                    d.pos = {(float)enemyx+30, (float)enemyy+20};
                    float ang = GetRandomValue(180, 360) * DEG2RAD; 
                    float spd = GetRandomValue(30, 60) / 10.0f; 
                    d.vel = { cosf(ang) * spd, sinf(ang) * spd };
                    d.isCombustivel = true; // <--- É COMBUSTÍVEL
                    d.valor = 15; // Cada bolha recupera 15 de combustível
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

        fxExplosoes->Desenhar();

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
                fxExplosoes->AdicionarExplosao({(float)nave_x + 40, (float)nave_y + 40}, LARGE_SHIP);
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
                jogador->minhaNave->escudoAtual=1;
                pontos = 1;
                bum = 0;
                boss_defeated = false;
                jogador->minhaNave->combustivelAtual = jogador->minhaNave->combustivelMaximo;
                //jogador->minhaNave->velocidadeAtual = (float)GetRandomValue(80, 2500); // Sorteia o lançamento de novo
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
    fxExplosoes = new GerenciadorDeExplosoes();
    fxExplosoes->Inicializar(texParticula);
    chefeFinal = new Boss();
    UnloadImage(imgExp);

    // --- PUXAMENTO DOS DADOS DO MAPA ESTELAR ---
    chefeFinal = new Boss();
    jogador = new Player("Piloto"); 
    CarregarPlanoDeVoo(); 
    jogador->minhaNave->CarregarStatus();
    jogador->minhaNave->escudoAtual = jogador->minhaNave->escudoMaximo;
    jogador->minhaNave->velocidadeAtual = 50.0f;

    // --- INICIALIZAÇÃO DAS VARIÁVEIS ---
    exp_x = nave_x; exp_y = nave_y; death_x = nave_x - 30; death_y = nave_y + 10; fire_x = nave_x + 30;
    fire_y = nave_y + 84; sol_x = GetRandomValue(10, 1100); sol_y = GetRandomValue(10, 400);
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
        float velocidadeQuedaCenario = IsKeyDown(KEY_K) ? 10.0f : 3.0f; // Turbo afeta o fundo
        fxExplosoes->Atualizar(GetFrameTime(), velocidadeQuedaCenario);
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