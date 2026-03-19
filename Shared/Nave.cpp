#include "Nave.h"

// Construtor: Quando o jogador "ganha" a nave, ela vem com esses status
Nave::Nave() {
    posicaoMapa = { 0.0f, 0.0f }; // Começa na Terra/Planeta inicial

    // Status Iniciais de Voo
    velocidadeAtual = 0.0f; 
    velocidadeMaxima = 500000.0f;
    combustivelMaximo = 100.0f;
    combustivelAtual = combustivelMaximo;
    escudoMaximo = 10;
    escudoAtual = escudoMaximo;
    iFrame = 0.0f;
    
    // Níveis iniciais (Upáveis no futuro)
    forcaTurbo = 250.0f;          // Ganha 50 de vel por seg
    eficienciaCombustivel = 0.0f;// Começa sem bônus de economia
    taxaConsumoBase = 10.0f;     // Gasta 10 de comb por seg padrão

    // Status Iniciais de Combate
    cooldownTiro = 0.0f;

    // Porão vazio
    invFerro = 0;
    invPrata = 0;
    invOuro = 0;
}

Nave::~Nave() {
}

// O Cérebro da movimentação espacial
void Nave::AtualizarVoo(float dt, bool usandoTurbo, bool usandoFreio) {

    // MATEMÁTICA DA ECONOMIA: 
    // Se eficiencia for 0, gasta o valor base (10). 
    // Se for 100 (100%), o divisor vira 2.0, então gasta metade (5).
    float consumoReal = taxaConsumoBase / (1.0f + (eficienciaCombustivel / 100.0f));

    // 1. Lógica do Turbo
    if (usandoTurbo && combustivelAtual > 0.0f) {
        combustivelAtual -= consumoReal * dt;
        velocidadeAtual += forcaTurbo * dt;
    }
    
    // 2. Lógica do Freio (Retropropulsores)
    if (usandoFreio && combustivelAtual > 0.0f) {
        combustivelAtual -= consumoReal * dt;
        velocidadeAtual -= (forcaTurbo * 2.0f) * dt; //Freia com o dobro de força pq sim
        
        if (velocidadeAtual < 0.0f) velocidadeAtual = 0.0f; 
    }

    // Trava de segurança para o combustível não ficar negativo
    if (combustivelAtual < 0.0f) combustivelAtual = 0.0f;
}

// Função para facilitar a adição de itens no jogo principal
void Nave::GuardarMinerio(TipoMinerio tipo, int quantidade) {
    if (tipo == FERRO) invFerro += quantidade;
    else if (tipo == PRATA) invPrata += quantidade;
    else if (tipo == OURO) invOuro += quantidade;
}