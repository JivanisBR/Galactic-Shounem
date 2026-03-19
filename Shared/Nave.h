#pragma once
#include "raylib.h"

// Trazemos o enum de minérios para cá, já que é a nave que guarda eles
enum TipoMinerio { FERRO, PRATA, OURO };

class Nave {
public:
    // Posição no Mapa Estelar (Não é a posição do minigame de tiro)
    Vector2 posicaoMapa;

    // Física e Voo
    float velocidadeAtual;
    float velocidadeMaxima;
    float combustivelAtual;
    float combustivelMaximo;
    
    // Variáveis Upáveis
    float forcaTurbo;            // O quanto a velocidade sobe por segundo
    float eficienciaCombustivel; // Nível de economia (0 = gasta normal)
    float taxaConsumoBase;       // O gasto padrão de combustível por segundo
    int escudoMaximo;
    int escudoAtual;
    
    float iFrame; // Cronômetro do tempo de invencibilidad
    
    // Combate
    float cooldownTiro;

    // Inventário (O porão da nave)
    int invFerro;
    int invPrata;
    int invOuro;

    // Funções
    Nave();  // Construtor
    ~Nave(); // Destrutor

    // Função que vai calcular o gasto de combustível e a inércia
    void AtualizarVoo(float dt, bool usandoTurbo, bool usandoFreio);
    
    // Função para guardar o loot de forma organizada
    void GuardarMinerio(TipoMinerio tipo, int quantidade);
};