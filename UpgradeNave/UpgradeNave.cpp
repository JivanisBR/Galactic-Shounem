#include "raylib.h"
#include "../Shared/Nave.h"
#include <string>
#include <vector>

// Estrutura para facilitar a criação e renderização dos botões
struct BotaoUpgrade {
    Rectangle rect;
    Color corBase;
    Color corHover;
    std::string nome;
    int* levelAtual;
    void (*AcaoUpgrade)(Nave&);
};

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Space Shooter - Upgrade da Nave");
    SetTargetFPS(60);

    Nave minhaNave; // Já vai tentar puxar o save via construtor

    // Variáveis para a mensagem pop-up
    float msgTimer = 0.0f;
    std::string msgTexto = "";

    // Configuração dos Botões de Upgrade
    std::vector<BotaoUpgrade> botoes = {
        { {100, 100, 250, 80}, MAROON, RED, "Cadencia de Tiro", &minhaNave.levelTiro, 
          [](Nave& n){ n.levelTiro++; n.cooldownTiro -= 0.05f; if(n.cooldownTiro < 0) n.cooldownTiro = 0; } },
          
        { {450, 100, 250, 80}, DARKBLUE, BLUE, "Escudo Maximo", &minhaNave.levelEscudo, 
          [](Nave& n){ n.levelEscudo++; n.escudoMaximo += 5; n.escudoAtual = n.escudoMaximo; } },
          
        { {100, 220, 250, 80}, DARKGREEN, GREEN, "Forca do Turbo", &minhaNave.levelTurbo, 
          [](Nave& n){ n.levelTurbo++; n.forcaTurbo += 15.0f; n.velocidadeMaxima += 500.0f; } },
          
        { {450, 220, 250, 80}, ORANGE, GOLD, "Capacidade Combustivel", &minhaNave.levelCombustivelMax, 
          [](Nave& n){ n.levelCombustivelMax++; n.combustivelMaximo += 25.0f; } },
          
        { {100, 340, 250, 80}, PURPLE, VIOLET, "Eficiencia Combustivel", &minhaNave.levelEficiencia, 
          [](Nave& n){ n.levelEficiencia++; n.eficienciaCombustivel += 10.0f; } },
          
        { {450, 340, 250, 80}, DARKGRAY, LIGHTGRAY, "Condensador AFK", &minhaNave.condensadorLevel, 
          [](Nave& n){ n.condensadorLevel++; } }
    };

    Rectangle btnReset = { 275, 480, 250, 60 };

    while (!WindowShouldClose()) {
        Vector2 mousePos = GetMousePosition();
        bool clicou = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        // Lógica dos Botões de Upgrade
        for (auto& btn : botoes) {
            if (CheckCollisionPointRec(mousePos, btn.rect)) {
                if (clicou) {
                    btn.AcaoUpgrade(minhaNave);
                    minhaNave.SalvarStatus(); // Salva a alteração para a main gameplay puxar depois
                    
                    msgTexto = btn.nome + " upado para level " + std::to_string(*btn.levelAtual);
                    msgTimer = 3.0f;
                }
            }
        }

        // Lógica do Botão de Reset
        if (CheckCollisionPointRec(mousePos, btnReset) && clicou) {
            minhaNave.ResetarUpgrades();
            msgTexto = "Status e Game Feel resetados!";
            msgTimer = 3.0f;
        }

        // Atualiza Timer da Mensagem
        if (msgTimer > 0) msgTimer -= GetFrameTime();

        // RENDERIZAÇÃO
        BeginDrawing();
        ClearBackground(BLACK);

        DrawText("SISTEMA DE UPGRADES", 250, 30, 28, WHITE);

        // Desenha Blocos de Upgrade
        for (const auto& btn : botoes) {
            bool hover = CheckCollisionPointRec(mousePos, btn.rect);
            DrawRectangleRec(btn.rect, hover ? btn.corHover : btn.corBase);
            
            // Texto centralizado (aproximado)
            std::string textoBtn = btn.nome + " (Lv." + std::to_string(*btn.levelAtual) + ")";
            int textWidth = MeasureText(textoBtn.c_str(), 18);
            DrawText(textoBtn.c_str(), btn.rect.x + (btn.rect.width / 2) - (textWidth / 2), btn.rect.y + 30, 18, BLACK);
        }

        // Desenha Botão de Reset
        bool hoverReset = CheckCollisionPointRec(mousePos, btnReset);
        DrawRectangleRec(btnReset, hoverReset ? RED : DARKGRAY);
        DrawText("RESETAR TUDO", btnReset.x + 45, btnReset.y + 20, 20, BLACK);

        // Renderiza Pop-up de Aviso
        if (msgTimer > 0) {
            int msgWidth = MeasureText(msgTexto.c_str(), 20);
            DrawText(msgTexto.c_str(), screenWidth - msgWidth - 20, screenHeight - 40, 20, GREEN);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}