#pragma once
#include "raylib.h"
#include <vector>
#include <cmath>

#define MAX_TIROS_BOSS 500

struct TiroBoss {
    Vector2 pos;
    bool ativo;
};

struct PecaBoss {
    Rectangle src;
    Rectangle dst;
    Vector2 orig;
    float rot;
    Vector2 vel;
    float rotVel;
    bool active;
};

class Boss {
public:
    float deathSoundDelay = 0.0f;
    int bombCount = 0;

    void ComportamentoVivo(float mult, float dt, int& vidaPlayer, int death_x, int death_y, bool& tocaSomDano);
    void ComportamentoMorto(float mult, float dt, bool& boss_defeated, bool& winn, int dist_travel, int dist_total, bool& engatilhaExplosao);

    // Texturas e Tela
    Texture2D bossSpriteHD, bglsHD;
    RenderTexture2D telaBoss;
    float escalaBoss;

    // Posição e Status
    int x, y, vida;
    bool mortoFlag;
    float timerMorte;
    int bossDir;
    float bossMoveTimer;
    
    // Vetores de Tiros e Peças
    std::vector<TiroBoss> tirosBoss;
    std::vector<PecaBoss> pecasDestruidas;

    // Variáveis de IA e Combate
    float tempoDor, tempoRaiva, cadenciaTiro;
    int padraoAtual, passoPadrao;
    float timerPadrao, timerIA;
    bool iaAtiva, loopConcluido;

    // Variáveis de Animação
    float angRaboEsq, alvoRaboEsq, timerRaboEsq;
    float angRaboDir, alvoRaboDir, timerRaboDir;
    float angPresaEsq, alvoPresaEsq, timerPresaEsq;
    float angPresaDir, alvoPresaDir, timerPresaDir;
    float distEspinhoEsq, alvoEspinhoEsq, timerEspinhoEsq;
    float distEspinhoDir, alvoEspinhoDir, timerEspinhoDir;
    float offsetCanhoesMeioY;
    
    Vector2 posPontas[8];
    float recuoPonta[8];
    Vector2 posLuzes[5];
    float intensidadeLuz[5];
    float tempoCarga[8];
    bool atirando[8];

    // Funções (Os Comandos do Boss)
    Boss();  // Construtor (Carrega texturas)
    ~Boss(); // Destrutor (Descarrega texturas)
    void AtualizarEDesenhar();
    void Resetar();
};