#pragma once
#include "raylib.h"
#include <string>

// Os estados possíveis do nosso jogador
enum PlayerState {
    NO_MAPA_ESTELAR, // Pilotando a nave
    EM_COMBATE,      // Metendo a porrada
    MORTO            // Faleceu
};

// Avisamos ao compilador que a classe Nave vai existir no futuro
class Nave; 

class Player {
public:
    // Identidade
    std::string nome;

    // Status de Combate (RPG)
    int vidaMaxima;
    int vidaAtual;
    int kiMaximo;
    int kiAtual;
    int forca;
    int defesa;
    int velocidade;

    // --- STATUS DE KI (RPG) ---
    int pdlMaximo;
    int pdlBase;
    int pdlAtual;
    
    // Cosmético
    Color corAura;

    // Progressão
    int level;
    int xpAtual;
    float dinheiro = 100.0f;

    // Estado Atual
    PlayerState estadoAtual;

    // A posse da Nave (Um ponteiro que vai apontar para a nave dele)
    Nave* minhaNave;

    // Funções
    Player(std::string nomeInicial); // Construtor
    ~Player();                       // Destrutor
    
    int CalcularPDL();               // Calcula o Poder de Luta na hora
};