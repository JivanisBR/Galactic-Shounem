#include "Nave.h"
#include <fstream>

// Construtor: Quando o jogador "ganha" a nave, ela vem com esses status
Nave::Nave() {
    posicaoMapa = { 0.0f, 0.0f }; // Começa na Terra/Planeta inicial

    // Status Iniciais de Voo
    velocidadeAtual = 0.0f; 
    velocidadeMaxima = 5000.0f;
    combustivelMaximo = 100.0f;
    combustivelAtual = combustivelMaximo;
    
    // Níveis iniciais (Upáveis no futuro)
    forcaTurbo = 50.0f;          // Ganha 50 de vel por seg
    eficienciaCombustivel = 0.0f;// Começa sem bônus de economia
    taxaConsumoBase = 10.0f;     // Gasta 10 de comb por seg padrão
    escudoMaximo = 10; 
    escudoAtual = escudoMaximo;

    // Status Iniciais de Combate
    cooldownTiro = 0.0f;
    iFrame = 0.0f;

    // Porão vazio
    invFerro = 0;
    invPrata = 0;
    invOuro = 0;

    CarregarStatus();
}

Nave::~Nave() {
}

void Nave::SalvarStatus() {
    // Salva na raiz (Galactic Shounen/)
    std::ofstream file("../save_nave.txt"); 
    if (file.is_open()) {
        // Gravando os atributos de gameplay
        file << velocidadeMaxima << " " << combustivelMaximo << " " 
             << forcaTurbo << " " << eficienciaCombustivel << " " 
             << escudoMaximo << " " << cooldownTiro << " " << condensadorLevel << "\n";
             
        // Gravando os níveis dos upgrades
        file << levelTiro << " " << levelEscudo << " " << levelTurbo << " " 
             << levelEficiencia << " " << levelCombustivelMax << "\n";
        file.close();
    }
}

void Nave::CarregarStatus() {
    // Busca na raiz (Galactic Shounen/)
    std::ifstream file("../save_nave.txt");
    if (file.is_open()) {
        file >> velocidadeMaxima >> combustivelMaximo 
             >> forcaTurbo >> eficienciaCombustivel 
             >> escudoMaximo >> cooldownTiro >> condensadorLevel;
             
        file >> levelTiro >> levelEscudo >> levelTurbo 
             >> levelEficiencia >> levelCombustivelMax;
        file.close();
    } else {
        // Se o arquivo não existir (primeira vez jogando), 
        // ele mantém os valores padrão definidos no construtor.
    }
}

void Nave::ResetarUpgrades() {
    // Retorna ao Game Feel original
    velocidadeMaxima = 5000.0f;
    combustivelMaximo = 100.0f;
    forcaTurbo = 50.0f;
    eficienciaCombustivel = 0.0f;
    escudoMaximo = 10;
    cooldownTiro = 0.0f;
    condensadorLevel = 1;
    
    levelTiro = 1;
    levelEscudo = 1;
    levelTurbo = 1;
    levelEficiencia = 1;
    levelCombustivelMax = 1;

    SalvarStatus(); // Salva o reset no arquivo
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
        velocidadeAtual -= forcaTurbo * dt; // Freia com a mesma força do motor
        
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

void Nave::AtualizarCondensador(float deltaTime) {
    // Se o tanque tá cheio OU já gerou 5 no AFK, o condensador desliga
    if (combustivelAtual >= combustivelMaximo || fuelGeradoAFK >= 5) return;

    if (timerCondensador <= 0.0f) {
        // Gera o combustível (1 a 5)
        int stardustTam = GetRandomValue(1, 5);
        
        // Garante que não vai passar do limite de 5 do AFK
        if (fuelGeradoAFK + stardustTam > 10) {
            stardustTam = 10 - fuelGeradoAFK;
        }

        combustivelAtual += stardustTam;
        fuelGeradoAFK += stardustTam;
        if (combustivelAtual > combustivelMaximo) combustivelAtual = combustivelMaximo;

        // Calcula o próximo timer (30s a 180s) usando a sua fórmula exata
        float tempoBase = (float)GetRandomValue(30, 180);
        timerCondensador = tempoBase / (1.0f + (condensadorLevel / 10.0f)); 
        
    } else {
        timerCondensador -= deltaTime; // Delta time real, não framerate
    }
}

void Nave::ResetarTravaAFK() {
    fuelGeradoAFK = 0; // Chama isso sempre que a nave viajar ou reabastecer
}