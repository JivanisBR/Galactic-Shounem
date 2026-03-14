#include "raylib.h"
#include <string>
#include <vector>
#include <cmath>
#include <iostream>

// --- Estruturas de Dados ---

struct Player {
    int x = 190;
    int y = 290;
    int vida = 1000;
    int gold = 1000; 
    int xp = 0;
    int mov = 1; // 1:Dir, 2:Esq, 3:Cima, 4:Baix
    int animFrame = 0;
    
    // Loja
    int espadaNv = 1;
    int armaduraNv = 1;
    int lanhits = 0; 
};

struct Inimigo {
    int x, y;
    int mapa;
    int vida;
    bool vivo;
    bool derrotado;
    int xCombate = 0, yCombate = 0;
    int direcao = 0;
    int distRestante = 0;
    int tempoPensamento = 0;
};

struct Placa {
    int x, y;
};

// --- Variáveis Globais (Estado do Jogo) ---
const char* PATH_MAPA_COMPLETO = "mapcompleto.png";
const char* PATH_MAPA_10       = "map10.png";
const char* PATH_MAPA_11       = "map11.jpg";
const char* PATH_ARVORES       = "arvres.png";
const char* PATH_ANI           = "ani.png";
const char* PATH_ANI_CAMINH    = "ani caminh.png";
const char* PATH_ANI_ESQ       = "ani esq.png";
const char* PATH_ANI_ESQ_CMN   = "ani esq caminh.png";
const char* PATH_ARVORE_SOLO   = "arvra.png";
const char* PATH_BAYA          = "baya.png";
const char* PATH_ENTRADAS      = "entradas.png";
// Texturas
Texture2D texMapCompleto, texMap10, texMap11, texArvres, texAni, texAniCaminh, texAniEsq, texAniEsqCmn, texArv, texBaya, texEntrad;

// Estado
int map = 1;
bool emCombate = false;
int inimigoAtual = -1;
int goldDrop = 0, xpDrop = 0;
bool naLoja = false;
char mensagemLoja[64] = ""; // Pra guardar o texto de feedback
int tempoMensagemLoja = 0;  
bool podeInteragir = false;

// Floresta (Map 10)
std::vector<Placa> placas;
bool placasGeradas = false;
bool ladoD = false, ladoE = false;
int salaDestino = 0;
int checkNeg = 1;

// Combat Feedback
int feedbackTipo = 0;
int feedbackTimer = 0;
int cooldownEsquiva = 0;

// Instâncias
Player p;
Inimigo inimigos[5];
int frameCounter = 0;

// --- Funções Auxiliares ---

void CarregarTexturas() {
    texMapCompleto = LoadTexture(PATH_MAPA_COMPLETO);
    texMap10 = LoadTexture(PATH_MAPA_10);
    texMap11 = LoadTexture(PATH_MAPA_11);
    texArvres = LoadTexture(PATH_ARVORES);
    texAni = LoadTexture(PATH_ANI);
    texAniCaminh = LoadTexture(PATH_ANI_CAMINH);
    texAniEsq = LoadTexture(PATH_ANI_ESQ);
    texAniEsqCmn = LoadTexture(PATH_ANI_ESQ_CMN);
    texArv = LoadTexture(PATH_ARVORE_SOLO);
    texBaya = LoadTexture(PATH_BAYA);
    texEntrad = LoadTexture(PATH_ENTRADAS);
}

void DescarregarTexturas() {
    UnloadTexture(texMapCompleto); UnloadTexture(texMap10); UnloadTexture(texMap11);
    UnloadTexture(texArvres); UnloadTexture(texAni); UnloadTexture(texAniCaminh);
    UnloadTexture(texAniEsq); UnloadTexture(texAniEsqCmn); UnloadTexture(texArv);
    UnloadTexture(texBaya); UnloadTexture(texEntrad);
}

void InicializarInimigos() {
    for (int i = 0; i < 5; i++) {
        inimigos[i].vivo = true;
        inimigos[i].derrotado = false;
        inimigos[i].x = GetRandomValue(100, 900);
        inimigos[i].y = GetRandomValue(100, 700);
        
        if (i == 0) {
            inimigos[i].mapa = 1;
            inimigos[i].vida = 50;
        } else {
            inimigos[i].mapa = GetRandomValue(1, 9);
            inimigos[i].vida = GetRandomValue(20, 200);
        }
    }
    if(map>10){
        inimigos[0].mapa = 10;
        inimigos[0].vida = 300;
    }
}

void LogicaFloresta() {
    // Gerar Placas
    if (map == 10 && !placasGeradas) {
        int qtd = GetRandomValue(1, 9);
        placas.clear();
        for (int i = 0; i < qtd; i++) {
            Placa nova;
            bool valida = false;
            int tentativa = 0;
            while (!valida && tentativa < 50) {
                tentativa++;
                nova.x = (GetRandomValue(0, 1) == 0) ? 320 : 600;
                nova.y = GetRandomValue(100, 700);
                valida = true;
                for (const auto& pl : placas) {
                    if (abs(nova.y - pl.y) < 100 && nova.x == pl.x) valida = false;
                }
            }
            placas.push_back(nova);
        }
        placasGeradas = true;
    }
    if (map != 10) placasGeradas = false;

    // Colisão Lateral Floresta
    if (map == 10) {
        if (p.x < 320) {
            bool passagem = false;
            for (auto& pl : placas) {
                if (pl.x == 320 && abs(p.y - pl.y) < 30) {
                    passagem = true;
                    if (p.x < 100) ladoE = true;
                }
            }
            if (!passagem) p.x = 320;
        }
        if (p.x > 600) {
            bool passagem = false;
            for (auto& pl : placas) {
                if (pl.x == 600 && abs(p.y - pl.y) < 30) {
                    passagem = true;
                    if (p.x > 800) ladoD = true;
                }
            }
            if (!passagem) p.x = 600;
        }
    }
}

void AtualizarInimigos() {
    for (int i = 0; i < 5; i++) {
        if (inimigos[i].vivo && map == inimigos[i].mapa) {
            if (frameCounter % 3 == 0) { // Controle de velocidade
                if (inimigos[i].tempoPensamento > 0) {
                    inimigos[i].tempoPensamento--;
                } else {
                    if (inimigos[i].distRestante <= 0) {
                        inimigos[i].direcao = GetRandomValue(1, 4);
                        inimigos[i].distRestante = GetRandomValue(50, 500);
                        inimigos[i].tempoPensamento = 60;
                    }
                    // Movimento Simples
                    if (inimigos[i].distRestante > 0) {
                        if (inimigos[i].direcao == 1 && inimigos[i].x > 100) inimigos[i].x--;
                        else if (inimigos[i].direcao == 2 && inimigos[i].x < 900) inimigos[i].x++;
                        else if (inimigos[i].direcao == 3 && inimigos[i].y > 100) inimigos[i].y--;
                        else if (inimigos[i].direcao == 4 && inimigos[i].y < 700) inimigos[i].y++;
                        else inimigos[i].distRestante = 0;
                        inimigos[i].distRestante--;
                    }
                }
            }
            
            // Colisão Combate
            Rectangle recPlayer = {(float)p.x, (float)p.y, 50, 50}; // Hitbox aprox
            Rectangle recInimigo = {(float)inimigos[i].x, (float)inimigos[i].y, 40, 40};
            
            if (CheckCollisionRecs(recPlayer, recInimigo)) {
                emCombate = true;
                inimigoAtual = i;
                inimigos[i].xCombate = inimigos[i].x;
                inimigos[i].yCombate = inimigos[i].y;
                
                // Drop 
                int baseXP = 500 * ((inimigos[i].vida / 50) + 1);
                xpDrop = GetRandomValue(baseXP, baseXP + 500);
                goldDrop = GetRandomValue(5, 50 * ((inimigos[i].vida / 50) + 1));
            }
        }
    }
}

void LogicaCombate() {
    if (feedbackTimer > 0) feedbackTimer--;
    if (cooldownEsquiva > 0) cooldownEsquiva--;

    if (cooldownEsquiva == 0 && inimigos[inimigoAtual].vida > 0 && p.vida > 0) { 
        if (IsKeyPressed(KEY_ONE)) {
            inimigos[inimigoAtual].vida -= 20;
            feedbackTipo = 1; feedbackTimer = 60;
            // Resposta inimigo
            if (inimigos[inimigoAtual].vida > 0) {
                p.vida -= 15;
                // Feedback 2 é setado depois, na real seria melhor uma fila, mas deps faço
                // No próximo frame o player toma dano.
            } else {
                inimigos[inimigoAtual].derrotado = true;
            }
        }
        if (IsKeyPressed(KEY_TWO)) {
             if (GetRandomValue(0, 1) == 1) {
                 feedbackTipo = 3; feedbackTimer = 60; cooldownEsquiva = 30;
             } else {
                 p.vida -= 10;
                 feedbackTipo = 4; feedbackTimer = 60;
             }
        }
        if (IsKeyPressed(KEY_THREE)) {
            p.vida -= 20;
            int dano = GetRandomValue(25, 60);
            inimigos[inimigoAtual].vida -= dano;
            feedbackTipo = 5; feedbackTimer = 60;
             if (inimigos[inimigoAtual].vida > 0) {
                p.vida -= 15;
            } else {
                inimigos[inimigoAtual].derrotado = true;
            }
        }
    }

    if (inimigos[inimigoAtual].vida <= 0) {
         // vitória
         if (inimigos[inimigoAtual].derrotado) {
             p.gold += goldDrop;
             p.xp += xpDrop;
             inimigos[inimigoAtual].derrotado = false;
         }
         // Delayzinho para ler "Vitoria" pode ser aqui
         if (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
             emCombate = false;
             inimigos[inimigoAtual].vivo = false;
             inimigoAtual = -1;
         }
    }
     if (p.vida <= 0) {
         if (IsKeyPressed(KEY_ENTER)) {
            // Game Over
            emCombate = false; 
         }
     }
}

void DesenharMapa() {
    // Lógica para recortar mapcompleto.png
    Rectangle source = {0, 0, 1000, 800};
    
    if (map == 1) { source.x = 0; source.y = 800; }
    else if (map == 2) { source.x = 1000; source.y = 800; }
    else if (map == 3) { source.x = 0; source.y = 0; }
    else if (map == 4) { source.x = 0; source.y = 1600; }
    else if (map == 5) { source.x = 1000; source.y = 0; }
    else if (map == 6) { source.x = 1000; source.y = 1600; }
    else if (map == 7) { source.x = 2000; source.y = 0; }
    else if (map == 8) { source.x = 2000; source.y = 800; }
    else if (map == 9) { source.x = 2000; source.y = 1600; }

    if (map >= 1 && map <= 9) {
        DrawTextureRec(texMapCompleto, source, (Vector2){0, 0}, WHITE);
        if (map == 7) DrawTexture(texBaya, 550, 50, WHITE);
        if (map == 9) DrawTexture(texArv, 500, 300, WHITE);
    }
    
    if (map == 10) DrawTexture(texMap10, 0, 0, WHITE);
    if (map > 10) DrawTexture(texMap11, 0, 0, WHITE); 
}

void DesenharInimigos() {
    for (int i = 0; i < 5; i++) {
        if (map == inimigos[i].mapa) {
            Color cor = RED;
            if (!inimigos[i].vivo) cor = BLACK; // Cadáver
            else {
                // Tamanho baseado na vida 
                int tamanho = 20;
                if(inimigos[i].vida > 50) tamanho = 40;
                if(inimigos[i].vida > 100) tamanho = 60;
                if(inimigos[i].vida > 150) tamanho = 80;
                DrawRectangle(inimigos[i].x, inimigos[i].y, tamanho, tamanho, cor);
            }
        }
    }
}

void DesenharPlayer() {
    Texture2D tUse = texAni;
    if (p.mov == 1 || p.mov == 3) tUse = (p.animFrame > 50) ? texAniCaminh : texAni;
    if (p.mov == 2 || p.mov == 4) tUse = (p.animFrame > 50) ? texAniEsqCmn : texAniEsq;
    
    DrawTexture(tUse, p.x, p.y, WHITE);
}

void LogicaLoja() {
    // Definição de preços baseada na fórmula Preço * 2^(nivel-1)
    // BTF falou que 1 << n é 2 elevad a n
    int precoEspada = 30 * (1 << (p.espadaNv - 1)); 
    int precoArmadura = 50 * (1 << (p.armaduraNv - 1));
    int precoLanhits = 10;

    // Se tiver mensagem na tela, só decrementa o tempo e ignora input de compra (exceto sair)
    if (tempoMensagemLoja > 0) {
        tempoMensagemLoja--;
        if (tempoMensagemLoja == 0) {
             sprintf(mensagemLoja, ""); // Limpa 
        }
    } 
    else {
        // Opção 1 - Espada
        if (IsKeyPressed(KEY_ONE)) {
            if (p.gold >= precoEspada) {
                p.gold -= precoEspada;
                p.espadaNv++;
                sprintf(mensagemLoja, "Vc comprou Espada nv %d", p.espadaNv);
                tempoMensagemLoja = 180; // 
            } else {
                sprintf(mensagemLoja, "Gold Insuficiente!");
                tempoMensagemLoja = 120;
            }
        }
        // Opção 2 - Armadura
        if (IsKeyPressed(KEY_TWO)) {
            if (p.gold >= precoArmadura) {
                p.gold -= precoArmadura;
                p.armaduraNv++;
                sprintf(mensagemLoja, "Vc comprou Armadura nv %d", p.armaduraNv);
                tempoMensagemLoja = 180;
            } else {
                sprintf(mensagemLoja, "Gold Insuficiente!");
                tempoMensagemLoja = 120;
            }
        }
        // Opção 3 - Lanhits 
        if (IsKeyPressed(KEY_THREE)) {
            if (p.gold >= precoLanhits) {
                p.gold -= precoLanhits;
                p.lanhits++;
                sprintf(mensagemLoja, "Vc comprou Lanhits (%d total)", p.lanhits);
                tempoMensagemLoja = 180;
            } else {
                sprintf(mensagemLoja, "Gold Insuficiente!");
                tempoMensagemLoja = 120;
            }
        }
    }

    // Opção 4 - Sair 
    if (IsKeyPressed(KEY_FOUR) || IsKeyPressed(KEY_E)) {
        naLoja = false;
        // Move o player pra baixo pra não entrar na loja dnv
        p.y += 10; 
    }
}

void DesenharLoja() {
    // Cores
    Color corFundo = {105, 105, 105, 255};      // Cinza
    Color corAmarela = {218, 165, 32, 255};     // Dourado
    Color corMenu = {210, 105, 30, 255};        // Marrom/Laranja
    Color corDin = {255, 140, 0, 255};          // Laranja claro

    ClearBackground(corFundo);
    
    // Fundo Amarelo
    DrawRectangle(50, 50, 900, 700, corAmarela);
    
    // Fundo Menu (Caixa inferior)
    DrawRectangle(50, 290, 400, 100, corMenu);
    
    // Fundo Dinheiro
    DrawRectangle(50, 110, 150, 50, corDin);

    // Textos
    DrawText("QUER FAZE NEGOCIO?", 400, 70, 30, BLACK);
    DrawText(TextFormat("Din: %d", p.gold), 65, 130, 20, BLACK);

    int precoEspada = 30 * (1 << (p.espadaNv - 1)); 
    int precoArmadura = 50 * (1 << (p.armaduraNv - 1));
    int precoLanhits = 10;

    DrawText(TextFormat("1- Espada (%d gold)", precoEspada), 100, 300, 20, BLACK);
    DrawText(TextFormat("2- Armadura (%d gold)", precoArmadura), 100, 320, 20, BLACK);
    DrawText(TextFormat("3- Lanhits (%d gold)", precoLanhits), 100, 340, 20, BLACK);
    DrawText("4- Sair (ou E)", 100, 360, 20, BLACK);

    // Mensagem de Feedback
    if (tempoMensagemLoja > 0) {
        DrawText(mensagemLoja, 100, 400, 30, RED);
    }
}

// --- Loop Principal ---
int main() {
    InitWindow(1000, 730, "Jogo Convertido - C++ Raylib");
    SetTargetFPS(180);
    CarregarTexturas();
    InicializarInimigos();

    while (!WindowShouldClose()) {
        // --- UPDATE ---
        if (naLoja) {
            // Se estiver na loja, roda APENAS a lógica da loja
            LogicaLoja(); 
        } 
        else if (emCombate) {
            LogicaCombate();
        }
        else {
            // Movimento
            if (IsKeyDown(KEY_A)) { p.x -= 2; p.mov = 2; p.animFrame++; }
            if (IsKeyDown(KEY_D)) { p.x += 2; p.mov = 1; p.animFrame++; }
            if (IsKeyDown(KEY_W)) { p.y -= 2; p.mov = 3; }
            if (IsKeyDown(KEY_S)) { p.y += 2; p.mov = 4; }
            if (p.animFrame > 100) p.animFrame = 0;

            // --- TRANSIÇÕES DE MAPA E COLISÕES E PAREDES ---

            if (podeInteragir && IsKeyPressed(KEY_E)) {
                naLoja = true;
            }

            // Mapa 1
            if (map == 1 && p.x > 950) {
                map = 2; p.x = 0;
            }
            if (map == 1 && p.y > 750) {
                map = 4; p.y = 0;
            }
            if (map == 1 && p.y < 20) {
                map = 3; p.y = 740;
            }
            // Paredes Esquerda (Mapas 1, 3, 4)
            if ((map == 1 || map == 3 || map == 4) && p.x < 71) {
                p.x = 71;
            }

            // Mapa 2
            if (map == 2 && p.x < 0) {
                map = 1; p.x = 940;
            }
            if (map == 2 && p.y > 750) {
                map = 6; p.y = 0;
            }
            if (map == 2 && p.y < 20) {
                map = 5; p.y = 740;
            }
            if (map == 2 && p.x > 950) {
                map = 8; p.x = 0;
            }

            // Paredes Direita (Mapas 7, 8, 9)
            if ((map == 7 || map == 8 || map == 9) && p.x > 845) {
                p.x = 845;
            }

            // Mapa 3
            if (map == 3 && p.x > 950) {
                map = 5; p.x = 0;
            }
            if (map == 3 && p.y > 750) {
                map = 1; p.y = 20;
            }
            // Paredes Cima (Mapas 3, 7)
            if ((map == 3 || map == 7) && p.y < 55) {
                p.y = 55;
            }

            // Mapa 5
            // Paredes Superiores Recortadas
            if (map == 5 && ((p.y < 35 && p.x < 349) || (p.x >= 570 && p.y < 35))) {
                p.y = 35;
            }
            // Entrada da Floresta (Map 10)
            if (map == 5 && p.x >= 349 && p.x < 570 && p.y < -20) {
                map = 10; p.y = 740;
            }
            if (map == 5 && p.x < 0) {
                map = 3; p.x = 940;
            }
            if (map == 5 && p.y > 750) {
                map = 2; p.y = 20;
            }
            if (map == 5 && p.x > 950) {
                map = 7; p.x = 0;
            }

            // Mapa 10 (Floresta)
            // Saída da Floresta
            if (map == 10 && p.x >= 349 && p.x < 570 && p.y > 760) {
                map = 5; p.y = 20;
            }
            // Limites Laterais Floresta
            if (map == 10 && ((p.x < 349 && p.y > 640) || (p.x > 570 && p.y > 640))) {
                p.y = 640;
            }

            // Mapa 4
            if (map == 4 && p.x > 950) {
                map = 6; p.x = 0;
            }
            if (map == 4 && p.y < 0) {
                map = 1; p.y = 740;
            }
            // Paredes Baixo (Mapas 4, 6, 9)
            if ((map == 4 || map == 6 || map == 9) && p.y > 638) {
                p.y = 638;
            }

            // Mapa 6
            if (map == 6 && p.x < 0) {
                map = 4; p.x = 940;
            }
            if (map == 6 && p.y < 0) {
                map = 2; p.y = 740;
            }
            if (map == 6 && p.x > 950) {
                map = 9; p.x = 0;
            }

            // Mapa 7
            if (map == 7 && p.y > 750) {
                map = 8; p.y = 20;
            }
            if (map == 7 && p.x < 0) {
                map = 5; p.x = 940;
            }
            // Colisões Específicas Mapa 7 (Loja/Porta)
            if (map == 7 && p.x >= 538 && p.x < 550 && p.y < 268) {
                p.x = 538;
            }
            if (map == 7 && ((p.x > 538 && p.x <= 740 && p.y <= 270) || (p.x > 790 && p.y <= 270))) {
                p.y = 270;
            }
            // Interação Loja
            //bool podeInteragir = false; // Reset a cada frame
            if (map == 7 && p.x >= 742 && p.x <= 780 && p.y < 230) {
                podeInteragir = true;
            }
            if(map==7 && podeInteragir==true && p.y>260){
                podeInteragir=false;
            }

            // Mapa 8
            if (map == 8 && p.y > 750) {
                map = 9; p.y = 20;
            }
            if (map == 8 && p.x < 0) {
                map = 2; p.x = 940;
            }
            if (map == 8 && p.y < 0) {
                map = 7; p.y = 740;
            }

            // Mapa 9
            if (map == 9 && p.x < 0) {
                map = 6; p.x = 940;
            }
            if (map == 9 && p.y < 0) {
                map = 8; p.y = 740;
            }
            // Colisões Árvore Mapa 9 e Interação
            if (map == 9 && p.x >= 530 && p.x < 650 && p.y >= 525 && p.y < 600) {
                p.x = 530; //podeInteragir = true;
            }
            if (map == 9 && p.x > 531 && p.x <= 692 && p.y >= 525 && p.y < 600) {
                p.x = 692; //podeInteragir = true;
            }
            if (map == 9 && p.x > 500 && p.x < 650 && p.y >= 601 && p.y < 604) {
                p.y = 603;// podeInteragir = true;
            }
           /* if (map == 9 && p.x > 400 && p.x < 650 && p.y > 500 && p.y < 604) {
                podeInteragir = true;
            }
            if(map==9 && podeInteragir==true && p.y>605){
                podeInteragir=false;
            }*/

            // Floresta Lógica
            LogicaFloresta();
            if (ladoD) {
                salaDestino = GetRandomValue(1, 9);
                map = 10 + salaDestino;
                p.x = 50;
                ladoE = ladoD = false;
             }
             if(ladoE){
                salaDestino = GetRandomValue(1, 9);
                map = 10 + salaDestino;    
                p.x = 905;
                ladoD = ladoE = false;
             }

             //SAÍDA DA SALA DA FLORESTA
             if (salaDestino > 0 && (map == 10 + salaDestino)) {
                if(ladoD){
                    if (p.x < 0) {
                     map = 10; p.x = 500;
                     p.x = 800;
                     salaDestino = 0;
                 }
                } 
                
             }

             if(map>10){
                emCombate = true;
             }

             // Inimigos
             AtualizarInimigos();
             frameCounter++;
        } 

        // --- DRAW ---
        BeginDrawing();
        if (naLoja) {
            DesenharLoja();
        }
        else{
            ClearBackground(RAYWHITE);

            DesenharMapa();

            if(podeInteragir) {
                DrawText("Pressione E para interagir", p.x - 40, p.y - 20, 10, DARKGRAY);
               // podeInteragir = false; // Reseta para o próximo frame
            }
            // Placas Floresta
            if (map == 10) {
                for (const auto& pl : placas) DrawTexture(texEntrad, pl.x, pl.y, WHITE);
            }
             DesenharPlayer();
            if (map == 10) DrawTexture(texArvres, 0, 0, WHITE); // Overlay copa das arvores

           
            DesenharInimigos();

            // UI Combate
            if (emCombate) {
                DrawRectangle(200, 200, 300, 400, LIGHTGRAY);
                DrawText("COMBATE!", 250, 220, 20, BLACK);
                DrawText(TextFormat("Vida Jogador: %d", p.vida), 250, 260, 20, DARKGREEN);
                DrawText(TextFormat("Vida Inimigo: %d", inimigos[inimigoAtual].vida), 250, 290, 20, RED);
                
                if (p.vida > 0 && inimigos[inimigoAtual].vida > 0) {
                    DrawText("1 - Atacar", 250, 350, 20, BLACK);
                    DrawText("2 - Esquivar", 250, 380, 20, BLACK);
                    DrawText("3 - Ultimate (-20 HP)", 250, 410, 20, PURPLE);
                } else if (inimigos[inimigoAtual].vida <= 0) {
                    DrawText("INIMIGO DERROTADO!", 250, 450, 20, GOLD);
                    DrawText("Pressione ENTER para sair", 220, 480, 20, DARKGRAY);
                } else {
                    DrawText("VOCÊ PERDEU!", 250, 450, 20, RED);
                }

                if (feedbackTimer > 0) {
                    const char* msg = "";
                    switch(feedbackTipo) {
                        case 1: msg = "Voce atacou!"; break;
                        case 2: msg = "Voce foi atingido!"; break;
                        case 3: msg = "Esquiva Sucesso!"; break;
                        case 4: msg = "Esquiva Falhou!"; break;
                        case 5: msg = "ULTIMATE!"; break;
                    }
                    DrawText(msg, 400, 250, 20, BLUE);
                }
            }

            // HUD 
            DrawRectangle(0, 0, 150, 80, Fade(GREEN, 0.5f));
            DrawText(TextFormat("Gold: %d", p.gold), 10, 10, 20, BLACK);
            DrawText(TextFormat("XP: %d", p.xp), 10, 30, 20, BLACK);
            DrawText(TextFormat("HP: %d", p.vida), 10, 50, 20, BLACK);

            // DEBUG 
            DrawText(TextFormat("MAPA: %d", map), 900, 10, 20, BLACK);
            DrawText(TextFormat("X: %d Y: %d", p.x, p.y), 200, 600, 20, BLACK);
        }

        EndDrawing();
    }

    DescarregarTexturas();
    CloseWindow();
    return 0;
}