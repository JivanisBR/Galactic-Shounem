#include "TelaUpgrades.h"

// Estrutura local para facilitar o manuseio dos custos
struct CustoUpgrade { int ferro; int prata; int ouro; };

// =========================================================================
// TABELAS DE PREÇOS (Do Nível 1 indo para o 2, até o Nível 14 indo para 15)
// =========================================================================

// Tabela 0: Cadência e Escudo Máximo
const CustoUpgrade CUSTOS_TIRO_ESCUDO[16] = {
    {0,0,0}, {0,0,2}, {0,0,4}, {0,0,8}, {0,0,16}, {0,0,32}, 
    {0,0,64}, {0,0,120}, {0,0,220}, {0,0,400}, {0,0,700}, 
    {0,0,1000}, {0,0,1400}, {0,0,2000}, {0,0,3000}, {0,0,0} // Índice 15 é o MÁXIMO
};

// Tabela 1: Turbo e Eficiência de Combustível
const CustoUpgrade CUSTOS_TURBO_EFICIENCIA[16] = {
    {0,0,0}, {0,10,0}, {0,20,0}, {0,40,0}, {0,80,0}, {0,160,0}, 
    {0,320,4}, {0,608,8}, {0,1094,16}, {0,1860,32}, {0,2790,64},
    {0,3620,100}, {0,4440,130}, {0,5000,160}, {0,5500,180}, {0,0,0}
};

// Tabela 2: Capacidade Combustível e Condensador
const CustoUpgrade CUSTOS_CAPACIDADE_CONDENSADOR[16] = {
    {0,0,0}, {25,5,0}, {50,10,0}, {100,20,0}, {200,40,0}, {400,80,0},
    {800,160,0}, {1600,320,0}, {3000,640,0}, {6000,900,0}, {8000,1500,0},
    {13000,3000,0}, {18000,5000,0}, {25000,8000,0}, {35000,12000,0}, {0,0,0}
};

// Tabela 3: Mobilidade e Freio
const CustoUpgrade CUSTOS_MOBILIDADE_FREIO[16] = {
    {0,0,0}, {10,5,1}, {20,10,2}, {40,20,4}, {80,40,8}, {160,80,16},
    {320,160,32}, {640,320,64}, {1200,600,100}, {2000,900,200}, {4000,1300,400},
    {8000,2000,800}, {13000,2800,1300}, {18000,3500,1800}, {25000,4000,2500}, {0,0,0}
};

// Helper para pegar a tabela correta
const CustoUpgrade* GetTabelaCusto(int tipo) {
    if (tipo == 0) return CUSTOS_TIRO_ESCUDO;
    if (tipo == 1) return CUSTOS_TURBO_EFICIENCIA;
    if (tipo == 2) return CUSTOS_CAPACIDADE_CONDENSADOR;
    return CUSTOS_MOBILIDADE_FREIO;
}


TelaUpgrades::TelaUpgrades(int sw, int sh, Nave* naveRef) {
    screenWidth = sw;
    screenHeight = sh;
    msgTimer = 0.0f;
    msgTexto = "";

    // TIPO 0
    botoes.push_back({ {250, 150, 300, 80}, MAROON, RED, "Cadencia de Tiro", &(naveRef->levelTiro), 0, 
      [](Nave& n){ n.levelTiro++; n.cooldownTiro -= 0.05f; if(n.cooldownTiro < 0) n.cooldownTiro = 0; } });
    botoes.push_back({ {650, 150, 300, 80}, DARKBLUE, BLUE, "Escudo Maximo", &(naveRef->levelEscudo), 0, 
      [](Nave& n){ n.levelEscudo++; n.escudoMaximo += 5; n.escudoAtual = n.escudoMaximo; } });

    // TIPO 1
    botoes.push_back({ {250, 260, 300, 80}, DARKGREEN, GREEN, "Forca do Turbo", &(naveRef->levelTurbo), 1, 
      [](Nave& n){ n.levelTurbo++; n.forcaTurbo += 15.0f; n.velocidadeMaxima += 500.0f; } });
    botoes.push_back({ {650, 260, 300, 80}, PURPLE, VIOLET, "Eficiencia Comb.", &(naveRef->levelEficiencia), 1, 
      [](Nave& n){ n.levelEficiencia++; n.eficienciaCombustivel += 10.0f; } });

    // TIPO 2
    botoes.push_back({ {250, 370, 300, 80}, ORANGE, GOLD, "Capacidade Comb.", &(naveRef->levelCombustivelMax), 2, 
      [](Nave& n){ n.levelCombustivelMax++; n.combustivelMaximo += 25.0f; } });
    botoes.push_back({ {650, 370, 300, 80}, DARKGRAY, LIGHTGRAY, "Condensador AFK", &(naveRef->condensadorLevel), 2, 
      [](Nave& n){ n.condensadorLevel++; } });

    // TIPO 3
    botoes.push_back({ {250, 480, 300, 80}, DARKBROWN, BROWN, "Mobilidade (Eixos)", &(naveRef->levelMovimentacao), 3, 
      [](Nave& n){ n.levelMovimentacao++; n.velocidadeMovimentacao += 0.25f; } });
    botoes.push_back({ {650, 480, 300, 80}, {0, 128, 128, 255}, SKYBLUE, "Eficiencia do Freio", &(naveRef->levelFreio), 3, 
      [](Nave& n){ n.levelFreio++; n.forcaFreio += 20.0f; } });

    btnReset = { 450, 600, 300, 60 };
}

void TelaUpgrades::AtualizarEDesenhar(Nave* naveRef) {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.95f));
    
    // ================== HUD DE RECURSOS ==================
    DrawText("SISTEMA DE UPGRADES DA NAVE", screenWidth/2 - MeasureText("SISTEMA DE UPGRADES DA NAVE", 30)/2, 30, 30, WHITE);
    
    std::string txtInventario = TextFormat("INVENTÁRIO:   %d Ferro   |   %d Prata   |   %d Ouro", naveRef->invFerro, naveRef->invPrata, naveRef->invOuro);
    DrawText(txtInventario.c_str(), screenWidth/2 - MeasureText(txtInventario.c_str(), 20)/2, 75, 20, GOLD);
    
    DrawText("Aperte TAB para voltar", screenWidth/2 - MeasureText("Aperte TAB para voltar", 15)/2, 105, 15, GRAY);
    // =====================================================

    Vector2 mousePos = GetMousePosition();
    bool clicou = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    for (auto& btn : botoes) {
        int lvl = *(btn.levelAtual);
        bool maxed = (lvl >= 15);
        CustoUpgrade custoAtual = {0, 0, 0};
        bool temRecursos = false;

        // Puxa o custo apenas se não estiver no máximo
        if (!maxed) {
            custoAtual = GetTabelaCusto(btn.tipoCusto)[lvl];
            temRecursos = (naveRef->invFerro >= custoAtual.ferro && 
                           naveRef->invPrata >= custoAtual.prata && 
                           naveRef->invOuro >= custoAtual.ouro);
        }

        bool hover = CheckCollisionPointRec(mousePos, btn.rect);
        
        // Clicou, tem recurso e não está no máximo?
        if (hover && clicou && !maxed && temRecursos) {
            
            // 1. Desconta os minérios do inventário
            naveRef->invFerro -= custoAtual.ferro;
            naveRef->invPrata -= custoAtual.prata;
            naveRef->invOuro -= custoAtual.ouro;

            // 2. Aplica o upgrade e Salva
            btn.AcaoUpgrade(*naveRef);
            naveRef->SalvarStatus();
            
            msgTexto = btn.nome + " upado para level " + std::to_string(*(btn.levelAtual));
            msgTimer = 3.0f;
        }
        
        // --- RENDERIZAÇÃO DO BOTÃO E SEU CONTEXTO ---
        // Se Maxed: Fundo Cinza escuro. Se Não tem recurso: Fundo desbotado. Se tudo OK: Hover normal.
        Color corFundo = maxed ? DARKGRAY : (temRecursos ? (hover ? btn.corHover : btn.corBase) : ColorAlpha(btn.corBase, 0.4f));
        Color corBorda = maxed ? GRAY : (temRecursos ? WHITE : DARKGRAY);
        Color corTitulo = maxed ? GRAY : WHITE;

        DrawRectangleRec(btn.rect, corFundo);
        DrawRectangleLinesEx(btn.rect, 2, corBorda);
        
        // 1. Título do Botão (Nome e Level Atual)
        std::string titulo = btn.nome + (maxed ? " (MAX)" : " (Lv." + std::to_string(lvl) + ")");
        int textWidth = MeasureText(titulo.c_str(), 18);
        DrawText(titulo.c_str(), btn.rect.x + (btn.rect.width / 2) - (textWidth / 2), btn.rect.y + 15, 18, corTitulo);

        // 2. Custo (Desenhado logo abaixo do título dentro do botão)
        if (!maxed) {
            std::string strCusto = "";
            if (custoAtual.ferro > 0) strCusto += TextFormat("%d Fe   ", custoAtual.ferro);
            if (custoAtual.prata > 0) strCusto += TextFormat("%d Pt   ", custoAtual.prata);
            if (custoAtual.ouro > 0)  strCusto += TextFormat("%d Au", custoAtual.ouro);

            int custoW = MeasureText(strCusto.c_str(), 16);
            DrawText(strCusto.c_str(), btn.rect.x + (btn.rect.width / 2) - (custoW / 2), btn.rect.y + 45, 16, temRecursos ? LIME : RED);
        }
    }

    // Botão de Reset (Inalterado)
    bool hoverReset = CheckCollisionPointRec(mousePos, btnReset);
    if (hoverReset && clicou) {
        naveRef->ResetarUpgrades();
        msgTexto = "Status e Upgrades resetados!";
        msgTimer = 3.0f;
    }
    DrawRectangleRec(btnReset, hoverReset ? RED : DARKGRAY);
    DrawRectangleLinesEx(btnReset, 2, WHITE);
    DrawText("RESETAR TUDO (DEV)", btnReset.x + 40, btnReset.y + 20, 20, WHITE);

    if (msgTimer > 0) {
        msgTimer -= GetFrameTime();
        int msgWidth = MeasureText(msgTexto.c_str(), 20);
        DrawText(msgTexto.c_str(), screenWidth/2 - msgWidth/2, screenHeight - 30, 20, GREEN);
    }
}