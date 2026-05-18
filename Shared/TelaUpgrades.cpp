#include "TelaUpgrades.h"

TelaUpgrades::TelaUpgrades(int sw, int sh, Nave* naveRef) {
    screenWidth = sw;
    screenHeight = sh;
    msgTimer = 0.0f;
    msgTexto = "";

    // Centralizado para a resolução 1200x700 do Mapa
    botoes = {
        { {250, 150, 300, 80}, MAROON, RED, "Cadencia de Tiro", &(naveRef->levelTiro), 
          [](Nave& n){ n.levelTiro++; n.cooldownTiro -= 0.05f; if(n.cooldownTiro < 0) n.cooldownTiro = 0; } },
        { {650, 150, 300, 80}, DARKBLUE, BLUE, "Escudo Maximo", &(naveRef->levelEscudo), 
          [](Nave& n){ n.levelEscudo++; n.escudoMaximo += 5; n.escudoAtual = n.escudoMaximo; } },
        { {250, 260, 300, 80}, DARKGREEN, GREEN, "Forca do Turbo", &(naveRef->levelTurbo), 
          [](Nave& n){ n.levelTurbo++; n.forcaTurbo += 15.0f; n.velocidadeMaxima += 500.0f; } },
        { {650, 260, 300, 80}, ORANGE, GOLD, "Capacidade Combustivel", &(naveRef->levelCombustivelMax), 
          [](Nave& n){ n.levelCombustivelMax++; n.combustivelMaximo += 25.0f; } },
        { {250, 370, 300, 80}, PURPLE, VIOLET, "Eficiencia Combustivel", &(naveRef->levelEficiencia), 
          [](Nave& n){ n.levelEficiencia++; n.eficienciaCombustivel += 10.0f; } },
        { {650, 370, 300, 80}, DARKGRAY, LIGHTGRAY, "Condensador AFK", &(naveRef->condensadorLevel), 
          [](Nave& n){ n.condensadorLevel++; } },
        { {250, 480, 300, 80}, DARKBROWN, BROWN, "Mobilidade (Eixos)", &(naveRef->levelMovimentacao), 
          [](Nave& n){ n.levelMovimentacao++; n.velocidadeMovimentacao += 0.25f; } },
        { {650, 480, 300, 80}, {0, 128, 128, 255}, SKYBLUE, "Eficiencia do Freio", &(naveRef->levelFreio), 
          [](Nave& n){ n.levelFreio++; n.forcaFreio += 20.0f; } }
    };

    btnReset = { 450, 600, 300, 60 };
}

void TelaUpgrades::AtualizarEDesenhar(Nave* naveRef) {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.95f));
    
    DrawText("SISTEMA DE UPGRADES DA NAVE", screenWidth/2 - MeasureText("SISTEMA DE UPGRADES DA NAVE", 30)/2, 40, 30, WHITE);
    DrawText("Aperte TAB para voltar", screenWidth/2 - MeasureText("Aperte TAB para voltar", 15)/2, 80, 15, GRAY);

    Vector2 mousePos = GetMousePosition();
    bool clicou = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    for (auto& btn : botoes) {
        bool hover = CheckCollisionPointRec(mousePos, btn.rect);
        
        if (hover && clicou) {
            btn.AcaoUpgrade(*naveRef);
            naveRef->SalvarStatus();
            
            msgTexto = btn.nome + " upado para level " + std::to_string(*btn.levelAtual);
            msgTimer = 3.0f;
        }
        
        DrawRectangleRec(btn.rect, hover ? btn.corHover : btn.corBase);
        DrawRectangleLinesEx(btn.rect, 2, WHITE);
        
        std::string textoBtn = btn.nome + " (Lv." + std::to_string(*btn.levelAtual) + ")";
        int textWidth = MeasureText(textoBtn.c_str(), 18);
        DrawText(textoBtn.c_str(), btn.rect.x + (btn.rect.width / 2) - (textWidth / 2), btn.rect.y + 30, 18, WHITE);
    }

    bool hoverReset = CheckCollisionPointRec(mousePos, btnReset);
    if (hoverReset && clicou) {
        naveRef->ResetarUpgrades();
        msgTexto = "Status e Game Feel resetados!";
        msgTimer = 3.0f;
    }
    
    DrawRectangleRec(btnReset, hoverReset ? RED : DARKGRAY);
    DrawRectangleLinesEx(btnReset, 2, WHITE);
    DrawText("RESETAR TUDO", btnReset.x + 65, btnReset.y + 20, 20, WHITE);

    if (msgTimer > 0) {
        msgTimer -= GetFrameTime();
        int msgWidth = MeasureText(msgTexto.c_str(), 20);
        DrawText(msgTexto.c_str(), screenWidth/2 - msgWidth/2, screenHeight - 30, 20, GREEN);
    }
}


/*
TABELA DE UPGRADES
força do turbo:
nv2: 10pt
3: 20pt
4: 40pt
5: 80pt
6: 160pt
7: 320pt + 4ouro
8: 608pt + 8ouro
9: 1094pt + 16ouro
10: 1860pt + 32ouro
11: 2790pt + 64ouro
12: 3620pt + 100ouro
13: 4440pt + 130ouro
14: 5000pt + 160ouro
15: 5500pt + 180ouro */