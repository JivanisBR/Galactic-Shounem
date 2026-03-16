#include "raylib.h"
#include <math.h>

#define MAX_TIROS 500

typedef struct Tiro {
    Vector2 pos;
    bool ativo;
} Tiro;

Tiro tiros[MAX_TIROS] = { 0 };

int main() {
    InitWindow(800, 600, "Teste Boss HD - Space Plagyl");
    Texture2D bossSprite = LoadTexture("testbossgrand.png");
    Texture2D bgls = LoadTexture("bglsgrand.png");

    SetTextureFilter(bossSprite, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(bgls, TEXTURE_FILTER_BILINEAR);

    int bossX = (800 - bossSprite.width) / 2;
    int bossY = (600 - bossSprite.height) / 2;
    float bossW = 2048.0f; 

    float angRaboEsq = 0.0f, alvoRaboEsq = 0.0f, timerRaboEsq = 0.0f;
    float angRaboDir = 0.0f, alvoRaboDir = 0.0f, timerRaboDir = 0.0f;
    float angPresaEsq = 0.0f, alvoPresaEsq = 0.0f, timerPresaEsq = 0.0f;
    float angPresaDir = 0.0f, alvoPresaDir = 0.0f, timerPresaDir = 0.0f;

    Vector2 posPontas[8] = {
        {130, 1810}, {300, 1810}, {485, 1790}, {782, 1536}, 
        {1150, 1536}, {1454, 1766}, {1643, 1798}, {1806, 1825}
    };
    float recuoPonta[8] = { 0.0f };

    Vector2 posLuzes[5] = { {205, 1249}, {512, 1229}, {1044, 1106}, {1556, 1219}, {1833, 1249} };
    float intensidadeLuz[5] = { 0.0f }; 

    float tempoCarga[8] = { 0.0f };
    bool atirando[8] = { false };

    // Controle dos canhões do meio
    float offsetCanhoesMeioY = -200.0f; // -82.0f = escondido, 0.0f = revelado
    bool mostrarCanhoesMeio = false;

    float distEspinhoEsq = 0.0f, alvoEspinhoEsq = 0.0f, timerEspinhoEsq = 0.0f;
    float distEspinhoDir = 0.0f, alvoEspinhoDir = 0.0f, timerEspinhoDir = 0.0f;

    float tempoDor = 0.0f;
    float tempoRaiva = 0.0f;

    // Variáveis dos Padrões de Tiro
    int padraoAtual = 0; 
    float cadenciaTiro = 0.9f; // Tempo entre cada disparo (ajuste como quiser)
    float timerPadrao = 0.0f;
    int passoPadrao = 0;

    bool iaAtiva = false;
    float timerIA = 0.0f;
    bool loopConcluido = false;

    SetTargetFPS(60);

    float escalaAleatoria = (float)GetRandomValue(15, 25) / 100.0f; 

    Camera2D camera = { 0 };
    camera.target = { (float)bossX + (bossW / 2.0f), (float)bossY + (bossSprite.height / 2.0f) }; 
    camera.offset = { 400.0f, 300.0f }; 
    camera.rotation = 0.0f;
    camera.zoom = escalaAleatoria;

    while (!WindowShouldClose()) {
        
        // Ativa/Desativa os canhões do meio
        if (IsKeyPressed(KEY_C)) mostrarCanhoesMeio = !mostrarCanhoesMeio;

        // Animação de descida/subida dos canhões do meio
        if (mostrarCanhoesMeio) {
            offsetCanhoesMeioY += 300.0f * GetFrameTime(); 
            if (offsetCanhoesMeioY > 0.0f) offsetCanhoesMeioY = 0.0f;
        } else {
            offsetCanhoesMeioY -= 300.0f * GetFrameTime(); 
            if (offsetCanhoesMeioY < -200.0f) offsetCanhoesMeioY = -200.0f;
        }

        for (int j = 0; j < 5; j++) {
            if (intensidadeLuz[j] > 0.0f) {
                intensidadeLuz[j] -= 2.0f * GetFrameTime(); 
                if (intensidadeLuz[j] < 0.0f) intensidadeLuz[j] = 0.0f;
            }
        }
        
        float oscilacaoSup = (sin(GetTime() * 4.0f) + 1.0f) / 2.0f; 
        float anguloBracoEsq = -oscilacaoSup * 20.0f; 
        float anguloAsaEsq = anguloBracoEsq + (sin(GetTime() * 8.0f) * 25.0f); 
        float anguloBracoDir = -anguloBracoEsq; 
        float anguloAsaDir = -anguloAsaEsq;

        float radEsq = anguloBracoEsq * PI / 180.0f; 
        float radDir = anguloBracoDir * PI / 180.0f;

        float dxAsaEsq = 614.0f - 799.0f, dyAsaEsq = 676.0f - 778.0f;
        float novoPivoAsaEsqX = 799.0f + (dxAsaEsq * cos(radEsq) - dyAsaEsq * sin(radEsq));
        float novoPivoAsaEsqY = 778.0f + (dxAsaEsq * sin(radEsq) + dyAsaEsq * cos(radEsq));

        float dxJuntaEsq = 604.0f - 799.0f, dyJuntaEsq = 676.0f - 778.0f;
        float novaJuntaEsqX = 799.0f + (dxJuntaEsq * cos(radEsq) - dyJuntaEsq * sin(radEsq));
        float novaJuntaEsqY = 778.0f + (dxJuntaEsq * sin(radEsq) + dyJuntaEsq * cos(radEsq));

        float baseBracoDirX = bossW - 799.0f, baseAsaDirX = bossW - 614.0f, baseJuntaDirX = bossW - 604.0f;
        float dxAsaDir = baseAsaDirX - baseBracoDirX, dyAsaDir = 676.0f - 778.0f;
        float novoPivoAsaDirX = baseBracoDirX + (dxAsaDir * cos(radDir) - dyAsaDir * sin(radDir));
        float novoPivoAsaDirY = 778.0f + (dxAsaDir * sin(radDir) + dyAsaDir * cos(radDir));

        float dxJuntaDir = baseJuntaDirX - baseBracoDirX, dyJuntaDir = 676.0f - 778.0f;
        float novaJuntaDirX = baseBracoDirX + (dxJuntaDir * cos(radDir) - dyJuntaDir * sin(radDir));
        float novaJuntaDirY = 778.0f + (dxJuntaDir * sin(radDir) + dyJuntaDir * cos(radDir));

        float oscilacaoInf = (sin(GetTime() * 3.0f) + 1.0f) / 2.0f; 
        float anguloBracoInfEsq = oscilacaoInf * 12.0f; 
        float anguloBracoInfDir = -anguloBracoInfEsq;
        float movimentoCanhaoY = oscilacaoInf * 82.0f; 

        // --- CONTROLE DE ESTADOS E IA ---
        if (IsKeyPressed(KEY_SEVEN)) { 
            iaAtiva = !iaAtiva; 
            if (iaAtiva) { padraoAtual = 1; timerIA = 3.0f; } 
            else { padraoAtual = 0; }
        }

        if (IsKeyPressed(KEY_D)) tempoDor = 1.0f;
        if (IsKeyPressed(KEY_R)) { 
            tempoRaiva = 4.0f; 
            cadenciaTiro -= 0.1f; // Reduz o tempo de espera (atira mais rápido)
            if (cadenciaTiro < 0.1f) cadenciaTiro = 0.1f; // Limite máximo
        }

        if (tempoDor > 0.0f) tempoDor -= GetFrameTime();
        if (tempoRaiva > 0.0f) tempoRaiva -= GetFrameTime();

        bool acelerado = (tempoDor > 0.0f || tempoRaiva > 0.0f);
        bool raiva = (tempoRaiva > 0.0f);

        // LÓGICA DA INTELIGÊNCIA ARTIFICIAL
        if (iaAtiva) {
            if (raiva) {
                padraoAtual = 6; // Força todos os canhões na raiva
            } else {
                if (padraoAtual == 6) {
                    padraoAtual = 1; // Fim da raiva, volta pro caos aleatório
                    timerIA = (float)GetRandomValue(50, 200) / 10.0f;
                } else if (padraoAtual == 1) {
                    timerIA -= GetFrameTime();
                    if (timerIA <= 0.0f) {
                        padraoAtual = GetRandomValue(2, 5); // Sorteia o próximo ataque
                        passoPadrao = 0;
                        timerPadrao = cadenciaTiro;
                        loopConcluido = false;
                    }
                } else if (padraoAtual >= 2 && padraoAtual <= 5) {
                    if (loopConcluido) {
                        padraoAtual = 1; // Terminou o loop afunilado/escada, volta pro 1
                        timerIA = (float)GetRandomValue(50, 200) / 10.0f; // Sorteia de 2 a 4 segundos
                        loopConcluido = false;
                    }
                }
            }
        }

        // Tremor do Boss
        if (raiva) {
            camera.offset.x = 400.0f + GetRandomValue(-3, 3);
            camera.offset.y = 300.0f + GetRandomValue(-3, 3);
        } else {
            camera.offset.x = 400.0f;
            camera.offset.y = 300.0f;
        }

        // Variáveis dinâmicas de velocidade baseadas no estado
        int minT = acelerado ? 1 : 5;
        int maxT = acelerado ? 10 : 30;
        float velRabo = acelerado ? 30.0f : 5.0f;
        float velPresa = acelerado ? 45.0f : 15.0f;
        float velEspinho = acelerado ? 50.0f : 15.0f;

        // Ampliação da força dos movimentos APENAS na Raiva
        float ampRabo = raiva ? 65.0f : 35.0f;    
        float ampPresa = raiva ? 45.0f : 25.0f;   
        float ampEspinho = raiva ? 140.0f : 90.0f; 

        // --- ANIMAÇÃO DOS RABINHOS ---
        timerRaboEsq -= GetFrameTime();
        if (timerRaboEsq <= 0.0f) {
            alvoRaboEsq = (float)GetRandomValue(0, (int)ampRabo); 
            timerRaboEsq = (float)GetRandomValue(minT, maxT) / 100.0f; 
        }
        angRaboEsq += (alvoRaboEsq - angRaboEsq) * velRabo * GetFrameTime(); 

        timerRaboDir -= GetFrameTime();
        if (timerRaboDir <= 0.0f) {
            alvoRaboDir = (float)GetRandomValue(0, -(int)ampRabo);
            timerRaboDir = (float)GetRandomValue(minT, maxT) / 100.0f;
        }
        angRaboDir += (alvoRaboDir - angRaboDir) * velRabo * GetFrameTime();

        // --- ANIMAÇÃO DAS PRESAS ---
        timerPresaEsq -= GetFrameTime();
        if (timerPresaEsq <= 0.0f) {
            alvoPresaEsq = (float)GetRandomValue(-(int)ampPresa, 0); 
            timerPresaEsq = (float)GetRandomValue(minT, maxT) / 100.0f;
        }
        angPresaEsq += (alvoPresaEsq - angPresaEsq) * velPresa * GetFrameTime(); 

        timerPresaDir -= GetFrameTime();
        if (timerPresaDir <= 0.0f) {
            alvoPresaDir = (float)GetRandomValue(0, (int)ampPresa);
            timerPresaDir = (float)GetRandomValue(minT, maxT) / 100.0f;
        }
        angPresaDir += (alvoPresaDir - angPresaDir) * velPresa * GetFrameTime();

        // --- ANIMAÇÃO DOS ESPINHOS ---
        timerEspinhoEsq -= GetFrameTime();
        if (timerEspinhoEsq <= 0.0f) {
            alvoEspinhoEsq = (float)GetRandomValue(0, (int)ampEspinho); 
            timerEspinhoEsq = (float)GetRandomValue(minT, maxT + 10) / 100.0f; 
        }
        distEspinhoEsq += (alvoEspinhoEsq - distEspinhoEsq) * velEspinho * GetFrameTime(); 

        timerEspinhoDir -= GetFrameTime();
        if (timerEspinhoDir <= 0.0f) {
            alvoEspinhoDir = (float)GetRandomValue(0, (int)ampEspinho);
            timerEspinhoDir = (float)GetRandomValue(minT, maxT + 10) / 100.0f;
        }
        distEspinhoDir += (alvoEspinhoDir - distEspinhoDir) * velEspinho * GetFrameTime();

        float radEspinho = -50.0f * PI / 180.0f;
        float movEspinhoEsqX = distEspinhoEsq * cos(radEspinho);
        float movEspinhoEsqY = distEspinhoEsq * sin(radEspinho);
        float movEspinhoDirX = distEspinhoDir * cos(radEspinho);
        float movEspinhoDirY = distEspinhoDir * sin(radEspinho);

        for (int t = 0; t < MAX_TIROS; t++) {
            if (tiros[t].ativo) {
                tiros[t].pos.y += 1200.0f * GetFrameTime(); 
                if (tiros[t].pos.y > bossY + 4000.0f) {
                    tiros[t].ativo = false;
                }
            }
        }

        // --- SELEÇÃO DE PADRÕES (Teclas 1 a 5, 0 para parar) ---
        if (IsKeyPressed(KEY_ONE))   { padraoAtual = 1; timerPadrao = cadenciaTiro; passoPadrao = 0; }
        if (IsKeyPressed(KEY_TWO))   { padraoAtual = 2; timerPadrao = cadenciaTiro; passoPadrao = 0; }
        if (IsKeyPressed(KEY_THREE)) { padraoAtual = 3; timerPadrao = cadenciaTiro; passoPadrao = 0; }
        if (IsKeyPressed(KEY_FOUR))  { padraoAtual = 4; timerPadrao = cadenciaTiro; passoPadrao = 0; }
        if (IsKeyPressed(KEY_FIVE))  { padraoAtual = 5; timerPadrao = cadenciaTiro; passoPadrao = 0; }
        if (IsKeyPressed(KEY_SIX))   { padraoAtual = 6; timerPadrao = cadenciaTiro; passoPadrao = 0; }
        if (IsKeyPressed(KEY_ZERO))  { padraoAtual = 0; }

        bool ativarCarga[8] = { false };

        // --- EXECUÇÃO DO PADRÃO ---
        if (padraoAtual != 0) {
            timerPadrao -= GetFrameTime();
            if (timerPadrao <= 0.0f) {
                timerPadrao = cadenciaTiro;

                switch (padraoAtual) {
                    case 1: { 
                        int c = GetRandomValue(0, 7);
                        if (!((c == 3 || c == 4) && offsetCanhoesMeioY < 0.0f)) ativarCarga[c] = true;
                        break;
                    }
                    case 2: { 
                        int c1 = passoPadrao;
                        int c2 = 7 - passoPadrao;
                        if (!((c1 == 3 || c1 == 4) && offsetCanhoesMeioY < 0.0f)) {
                            ativarCarga[c1] = true; ativarCarga[c2] = true;
                        }
                        passoPadrao++;
                        if (passoPadrao > 3) { passoPadrao = 0; loopConcluido = true; }
                        break;
                    }
                    case 3: { 
                        int c1 = 3 - passoPadrao;
                        int c2 = 4 + passoPadrao;
                        if (!((c1 == 3 || c1 == 4) && offsetCanhoesMeioY < 0.0f)) {
                            ativarCarga[c1] = true; ativarCarga[c2] = true;
                        }
                        passoPadrao++;
                        if (passoPadrao > 3) { passoPadrao = 0; loopConcluido = true; }
                        break;
                    }
                    case 4: { 
                        if (!((passoPadrao == 3 || passoPadrao == 4) && offsetCanhoesMeioY < 0.0f)) ativarCarga[passoPadrao] = true;
                        passoPadrao++;
                        if (passoPadrao > 7) { passoPadrao = 0; loopConcluido = true; }
                        break;
                    }
                    case 5: { 
                        int c = 7 - passoPadrao;
                        if (!((c == 3 || c == 4) && offsetCanhoesMeioY < 0.0f)) ativarCarga[c] = true;
                        passoPadrao++;
                        if (passoPadrao > 7) { passoPadrao = 0; loopConcluido = true; }
                        break;
                    }
                    case 6: {
                        for (int c = 0; c < 8; c++) {
                            if ((c == 3 || c == 4) && offsetCanhoesMeioY < 0.0f) continue;
                            ativarCarga[c] = true;
                        }
                        break;
                    }
                }
            }
        }

        // --- ATIVAÇÃO DAS CARGAS E TIROS ---
        for (int i = 0; i < 8; i++) {
            // Se o padrão mandou atirar, inicia a carga
            if (ativarCarga[i] && tempoCarga[i] <= 0.0f && !atirando[i]) {
                tempoCarga[i] = 0.5f; 
                atirando[i] = true;
                intensidadeLuz[2] = 1.0f; 
                if (i <= 1) intensidadeLuz[0] = 1.0f;      
                else if (i == 2) intensidadeLuz[1] = 1.0f; 
                else if (i == 5) intensidadeLuz[3] = 1.0f; 
                else if (i >= 6) intensidadeLuz[4] = 1.0f; 
            }

            if (tempoCarga[i] > 0.0f) {
                tempoCarga[i] -= GetFrameTime();
                
                if (tempoCarga[i] <= 0.0f && atirando[i]) {
                    recuoPonta[i] = 123.0f;
                    atirando[i] = false;

                    for (int t = 0; t < MAX_TIROS; t++) {
                        if (!tiros[t].ativo) {
                            tiros[t].ativo = true;
                            float offsetY = (i < 3 || i > 4) ? movimentoCanhaoY : offsetCanhoesMeioY;
                            tiros[t].pos.x = (float)bossX + posPontas[i].x + 56.0f; 
                            tiros[t].pos.y = (float)bossY + posPontas[i].y - offsetY;
                            break; 
                        }
                    }
                }
            }

            if (recuoPonta[i] > 0.0f) {
                recuoPonta[i] -= 307.0f * GetFrameTime();
                if (recuoPonta[i] < 0.0f) recuoPonta[i] = 0.0f;
            }
        }

        BeginDrawing();
        ClearBackground(WHITE);

        BeginMode2D(camera);

        Color corJunta = { 67, 30, 22, 255 };

        for (int t = 0; t < MAX_TIROS; t++) {
            if (tiros[t].ativo) {
                DrawEllipse((int)tiros[t].pos.x, (int)tiros[t].pos.y, 15.0f, 60.0f, RED);
                DrawEllipse((int)tiros[t].pos.x, (int)tiros[t].pos.y, 5.0f, 40.0f, ORANGE);
            }
        }
        
        DrawCircle((float)bossX + 840.0f, (float)bossY + 911.0f, 133.0f, corJunta);
        DrawCircle((float)bossX + 1188.0f, (float)bossY + 911.0f, 133.0f, corJunta);
        DrawCircle((float)bossX+583.0f, (float)bossY + 1074.0f, 60.0f, corJunta);
        DrawCircle((float)bossX+1460.0f, (float)bossY + 1074.0f, 60.0f, corJunta);

        DrawTexturePro(bgls, { 576, 0, 279, 327 }, { (float)bossX + 799.0f, (float)bossY + 778.0f, 246, 307 }, { 236, 154 }, anguloBracoEsq, WHITE);
        DrawTexturePro(bgls, { 576, 0, -279, 327 }, { (float)bossX + baseBracoDirX, (float)bossY + 778.0f, 246, 307 }, { 10, 154 }, anguloBracoDir, WHITE);

        DrawTexturePro(bgls, { 1679, 604, 143, 297 }, { (float)bossX + 973.0f, (float)bossY + 532.0f, 143, 297 }, { 123, 164 }, -angRaboEsq, WHITE);
        DrawTexturePro(bgls, { 1679, 604, -143, 297 }, { (float)bossX + 1075.0f, (float)bossY + 532.0f, 143, 297 }, { 20, 164 }, -angRaboDir, WHITE);

        DrawTexturePro(bgls, { 932, 49, 573, 852 }, { (float)bossX + 100.0f, (float)bossY + 1000.0f - movimentoCanhaoY, 543, 850 }, { 0, 0 }, 0.0f, WHITE);
        DrawTexturePro(bgls, { 932, 49, -573, 852 }, { (float)bossX + (bossW - 100.0f), (float)bossY + 1000.0f - movimentoCanhaoY, 543, 850 }, { 543, 0 }, 0.0f, WHITE);
        
        DrawTexturePro(bgls, { 1529, 0, 471, 492 }, { (float)bossX + 737.0f, (float)bossY + 1075.0f, 430, 369 }, { 236, 307 }, anguloBracoInfEsq, WHITE);
        DrawTexturePro(bgls, { 1529, 0, -471, 492 }, { (float)bossX + (bossW - 737.0f), (float)bossY + 1075.0f, 430, 369 }, { 195, 307 }, anguloBracoInfDir, WHITE);

        // Canhões do meio (desenhados ANTES do Boss para esconder a base)
        DrawTexturePro(bgls, { 1361, 777, 73, 83 }, { (float)bossX + 800.0f, (float)bossY + 1471.0f + offsetCanhoesMeioY, 73, 83 }, { 0, 0 }, 0.0f, WHITE);
        DrawTexturePro(bgls, { 1361, 777, -73, 83 }, { (float)bossX + 1167.0f, (float)bossY + 1471.0f + offsetCanhoesMeioY, 73, 83 }, { 0, 0 }, 0.0f, WHITE);

        DrawTexture(bossSprite, bossX, bossY, WHITE);
        
        for (int i = 0; i < 8; i++) {
            float offsetOscilacaoY = 0.0f;
            float offsetMeioY = 0.0f;

            if (i < 3 || i > 4) {
                offsetOscilacaoY = movimentoCanhaoY;
            } else {
                // Aplica o movimento de esconder nas pontas 4 e 5
                offsetMeioY = offsetCanhoesMeioY; 
            }

            Rectangle sourcePonta = { 440, 0, 113, 154 };
            Rectangle destPonta = { 
                (float)bossX + posPontas[i].x, 
                (float)bossY + posPontas[i].y - recuoPonta[i] - offsetOscilacaoY + offsetMeioY, 
                113, 154 
            };
            
            DrawTexturePro(bgls, sourcePonta, destPonta, { 0, 0 }, 0.0f, WHITE);
        }

        DrawTexturePro(bgls, { 635, 461, 215, 440 }, { (float)bossX + 765.0f, (float)bossY + 1516.0f, 215, 440 }, { 123, 20 }, angPresaEsq, WHITE);
        DrawTexturePro(bgls, { 635, 461, -215, 440 }, { (float)bossX + 1274.0f, (float)bossY + 1516.0f, 215, 440 }, { 92, 20 }, angPresaDir, WHITE);

        DrawTexture(bossSprite, bossX, bossY, WHITE);
        
        DrawTexturePro(bgls, { 0, 0, 389, 819 }, { (float)bossX + novoPivoAsaEsqX, (float)bossY + novoPivoAsaEsqY, 389, 819 }, { 358, 625 }, anguloAsaEsq, WHITE);
        DrawTexturePro(bgls, { 0, 0, -389, 819 }, { (float)bossX + novoPivoAsaDirX, (float)bossY + novoPivoAsaDirY, 389, 819 }, { 31, 625 }, anguloAsaDir, WHITE);

        // ESPINHOS LATERAIS
        // Esquerdo
        DrawTexturePro(bgls, { 1898, 619, 298, 256 }, { (float)bossX + 648.0f + movEspinhoEsqX, (float)bossY + 1130.0f + movEspinhoEsqY, 198, 256 }, { 0, 0 }, 0.0f, WHITE);

        // Direito
        DrawTexturePro(bgls, { 1898, 619, -298, 256 }, { (float)bossX + (bossW - 648.0f) - movEspinhoDirX, (float)bossY + 1130.0f + movEspinhoDirY, 198, 256 }, { 198, 0 }, 0.0f, WHITE);

        // TAMPAS DOS BURACOS
        // Esquerda
        DrawTexturePro(bgls, { 2099, 0, 149, 237 }, { (float)bossX + 807.0f, (float)bossY + 1018.0f, 149, 237 }, { 0, 0 }, 0.0f, WHITE);

        // Direita (Espelhada usando bossW - 807)
        DrawTexturePro(bgls, { 2099, 0, -149, 237 }, { (float)bossX + (bossW - 807.0f), (float)bossY + 1018.0f, 149, 237 }, { 149, 0 }, 0.0f, WHITE);

       //DrawText(TextFormat("Timer IA: %.2f", timerIA), 20, 100, 100, BLACK);
      // DrawText(TextFormat("Padrao Atual: %d", padraoAtual), 20, 400, 100, BLACK);
       
        BeginBlendMode(BLEND_ADDITIVE);
        for (int j = 0; j < 5; j++) {
            if (intensidadeLuz[j] > 0.0f) {
                float raio = 154.0f * intensidadeLuz[j]; 
                
                Color corCentro = { 255, 50, 50, (unsigned char)(255 * intensidadeLuz[j]) };
                Color corBorda = { 255, 0, 0, 0 }; 
                
                float offsetY = (j == 2) ? 0.0f : movimentoCanhaoY;

                DrawCircleGradient(
                    (int)(bossX + posLuzes[j].x), 
                    (int)(bossY + posLuzes[j].y - offsetY), 
                    raio, corCentro, corBorda
                );
            }
        }
        EndBlendMode();
        EndMode2D();
        EndDrawing();
    }

    UnloadTexture(bossSprite);
    UnloadTexture(bgls);
    CloseWindow();
    return 0;
}