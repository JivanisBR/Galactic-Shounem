#include "GameManager.h"
#include "../Shared/Nave.h"

GameManager::GameManager() {
    InitWindow(1200, 700, "Space Plagyl");
    InitAudioDevice();
    SetTargetFPS(60);

    jogador = new Player("Kreits");
    jogador->minhaNave->CarregarStatus();
    
    estadoAtual = nullptr;
    proximoEstado = nullptr;
}

GameManager::~GameManager() {
    if (estadoAtual) {
        estadoAtual->Sair();
        delete estadoAtual;
    }
    delete jogador;
    CloseAudioDevice();
    CloseWindow();
}

void GameManager::MudarEstado(GameState* novoEstado) {
    proximoEstado = novoEstado; // Agenda a troca para o próximo frame
}

void GameManager::Executar() {
    while (!WindowShouldClose()) {
        
        // Processa a transição de telas (Loading invisível)
        if (proximoEstado != nullptr) {
            if (estadoAtual != nullptr) {
                estadoAtual->Sair();
                delete estadoAtual;
            }
            estadoAtual = proximoEstado;
            estadoAtual->Entrar(this);
            proximoEstado = nullptr;
        }

        // Executa o modo de jogo atual
        if (estadoAtual != nullptr) {
            estadoAtual->Atualizar(GetFrameTime());
            estadoAtual->Desenhar(); // O BeginDrawing() vai ficar dentro de cada estado
        } else {
            // Tela preta de segurança caso nenhum estado seja carregado
            BeginDrawing();
            ClearBackground(BLACK);
            EndDrawing();
        }
    }
}