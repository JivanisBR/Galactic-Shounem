#include "Player.h"
#include "Nave.h"

// O Construtor: Roda na hora que o jogador é criado no jogo
Player::Player(std::string nome) {
    this->nome = nome;
    
    // Inicialização do Ki
    this->pdlMaximo = GetRandomValue(2500000, 3500000); // Nível Entidade
    this->pdlBase = 50000;                              // Suprimido
    this->pdlAtual = this->pdlBase;
    this->corAura = ColorFromHSV((float)GetRandomValue(0, 360), 0.8f, 0.9f);
    
    // Status Iniciais (Level 1)
    vidaMaxima = 100;
    vidaAtual = 100;
    kiMaximo = 50;
    kiAtual = 50;
    forca = 10;
    defesa = 10;
    velocidade = 10;

    level = 1;
    xpAtual = 0;
    dinheiro = 0;

    estadoAtual = NO_MAPA_ESTELAR; // Começa na nave
    minhaNave = new Nave();
}

Player::~Player() {
    // Aqui limparemos coisas da memória se necessário no futuro
}

// A função que calcula o PDL dinamicamente
int Player::CalcularPDL() {
    // O Poder de Luta é a soma dos atributos base
    return vidaMaxima + kiMaximo + forca + defesa + velocidade;
}