#include "Boss.h"

Boss::Boss() {
    escalaBoss = 0.15f;
    x = 400; y = -600; vida = 50;
    mortoFlag = false; timerMorte = 0.0f;
    bossDir = 1; bossMoveTimer = 2.0f;
    tirosBoss.resize(MAX_TIROS_BOSS);
    
    tempoDor = 0.0f; tempoRaiva = 0.0f; cadenciaTiro = 0.5f;
    padraoAtual = 1; passoPadrao = 0; timerPadrao = 0.0f; timerIA = 3.0f;
    iaAtiva = true; loopConcluido = false;
    
    angRaboEsq = 0.0f; alvoRaboEsq = 0.0f; timerRaboEsq = 0.0f;
    angRaboDir = 0.0f; alvoRaboDir = 0.0f; timerRaboDir = 0.0f;
    angPresaEsq = 0.0f; alvoPresaEsq = 0.0f; timerPresaEsq = 0.0f;
    angPresaDir = 0.0f; alvoPresaDir = 0.0f; timerPresaDir = 0.0f;
    distEspinhoEsq = 0.0f; alvoEspinhoEsq = 0.0f; timerEspinhoEsq = 0.0f;
    distEspinhoDir = 0.0f; alvoEspinhoDir = 0.0f; timerEspinhoDir = 0.0f;
    offsetCanhoesMeioY = -200.0f;
    
    posPontas[0] = {130, 1810}; posPontas[1] = {300, 1810}; posPontas[2] = {485, 1790};
    posPontas[3] = {808, 1536}; posPontas[4] = {1132, 1536}; posPontas[5] = {1454, 1766};
    posPontas[6] = {1643, 1798}; posPontas[7] = {1806, 1825};
    
    posLuzes[0] = {205, 1249}; posLuzes[1] = {512, 1229}; posLuzes[2] = {1044, 1106};
    posLuzes[3] = {1556, 1219}; posLuzes[4] = {1833, 1249};
    
    for (int i=0; i<8; i++) { recuoPonta[i] = 0.0f; tempoCarga[i] = 0.0f; atirando[i] = false; }
    for (int i=0; i<5; i++) { intensidadeLuz[i] = 0.0f; }

    bossSpriteHD = LoadTexture("testbossgrand.png");
    bglsHD = LoadTexture("bglsgrand.png");
    SetTextureFilter(bossSpriteHD, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(bglsHD, TEXTURE_FILTER_BILINEAR);
    telaBoss = LoadRenderTexture(2048, 2048);
}

Boss::~Boss() {
    UnloadTexture(bossSpriteHD);
    UnloadTexture(bglsHD);
    UnloadRenderTexture(telaBoss);
}

void Boss::Resetar() {
    y = -600; vida = 50;
    mortoFlag = false; timerMorte = 0.0f;
    cadenciaTiro = 0.5f;
    deathSoundDelay = 0; 
    bombCount = 0;
    pecasDestruidas.clear();
    for(auto& t : tirosBoss) t.ativo = false;
}

void Boss::AtualizarEDesenhar() {
    float dt = GetFrameTime();
    
    if (tempoDor > 0.0f) tempoDor -= dt;
    if (tempoRaiva > 0.0f) tempoRaiva -= dt;

    bool acelerado = (tempoDor > 0.0f || tempoRaiva > 0.0f);
    bool raiva = (tempoRaiva > 0.0f);

    if (iaAtiva) {
        if (raiva) {
            padraoAtual = 6; 
        } else {
            if (padraoAtual == 6) {
                padraoAtual = 1; 
                timerIA = (float)GetRandomValue(20, 40) / 10.0f;
            } else if (padraoAtual == 1) {
                timerIA -= dt;
                if (timerIA <= 0.0f) {
                    padraoAtual = GetRandomValue(2, 5); 
                    passoPadrao = 0; timerPadrao = cadenciaTiro; loopConcluido = false;
                }
            } else if (padraoAtual >= 2 && padraoAtual <= 5) {
                if (loopConcluido) {
                    padraoAtual = 1; 
                    timerIA = (float)GetRandomValue(20, 40) / 10.0f; 
                    loopConcluido = false;
                }
            }
        }
    }

    bool ativarCarga[8] = { false };
    if (padraoAtual != 0) {
        timerPadrao -= dt;
        if (timerPadrao <= 0.0f) {
            timerPadrao = cadenciaTiro;
            switch (padraoAtual) {
                case 1: { int c = GetRandomValue(0, 7); if (!((c == 3 || c == 4) && offsetCanhoesMeioY < 0.0f)) ativarCarga[c] = true; break; }
                case 2: { int c1 = passoPadrao, c2 = 7 - passoPadrao; if (!((c1 == 3 || c1 == 4) && offsetCanhoesMeioY < 0.0f)) { ativarCarga[c1] = true; ativarCarga[c2] = true; } passoPadrao++; if (passoPadrao > 3) { passoPadrao = 0; loopConcluido = true; } break; }
                case 3: { int c1 = 3 - passoPadrao, c2 = 4 + passoPadrao; if (!((c1 == 3 || c1 == 4) && offsetCanhoesMeioY < 0.0f)) { ativarCarga[c1] = true; ativarCarga[c2] = true; } passoPadrao++; if (passoPadrao > 3) { passoPadrao = 0; loopConcluido = true; } break; }
                case 4: { if (!((passoPadrao == 3 || passoPadrao == 4) && offsetCanhoesMeioY < 0.0f)) ativarCarga[passoPadrao] = true; passoPadrao++; if (passoPadrao > 7) { passoPadrao = 0; loopConcluido = true; } break; }
                case 5: { int c = 7 - passoPadrao; if (!((c == 3 || c == 4) && offsetCanhoesMeioY < 0.0f)) ativarCarga[c] = true; passoPadrao++; if (passoPadrao > 7) { passoPadrao = 0; loopConcluido = true; } break; }
                case 6: { for (int c = 0; c < 8; c++) { if ((c == 3 || c == 4) && offsetCanhoesMeioY < 0.0f) continue; ativarCarga[c] = true; } break; }
            }
        }
    }

    float oscilacaoSup = (sin(GetTime() * 4.0f) + 1.0f) / 2.0f; 
    float anguloBracoEsq = -oscilacaoSup * 20.0f; 
    float anguloAsaEsq = anguloBracoEsq + (sin(GetTime() * 8.0f) * 25.0f); 
    float anguloBracoDir = -anguloBracoEsq; 
    float anguloAsaDir = -anguloAsaEsq;
    float radEsq = anguloBracoEsq * PI / 180.0f; 
    float radDir = anguloBracoDir * PI / 180.0f;

    float oscilacaoInf = (sin(GetTime() * 3.0f) + 1.0f) / 2.0f; 
    float anguloBracoInfEsq = oscilacaoInf * 12.0f; 
    float anguloBracoInfDir = -anguloBracoInfEsq;
    float movimentoCanhaoY = oscilacaoInf * 82.0f; 

    if (vida <= 20) { 
        offsetCanhoesMeioY += 300.0f * dt; if (offsetCanhoesMeioY > 0.0f) offsetCanhoesMeioY = 0.0f;
    } else {
        offsetCanhoesMeioY -= 300.0f * dt; if (offsetCanhoesMeioY < -200.0f) offsetCanhoesMeioY = -200.0f;
    }

    for (int i = 0; i < 8; i++) {
        if (ativarCarga[i] && tempoCarga[i] <= 0.0f && !atirando[i]) {
            tempoCarga[i] = 0.5f; atirando[i] = true; intensidadeLuz[2] = 1.0f; 
            if (i <= 1) intensidadeLuz[0] = 1.0f; else if (i == 2) intensidadeLuz[1] = 1.0f; 
            else if (i == 5) intensidadeLuz[3] = 1.0f; else if (i >= 6) intensidadeLuz[4] = 1.0f; 
        }

        if (tempoCarga[i] > 0.0f) {
            tempoCarga[i] -= dt;
            if (tempoCarga[i] <= 0.0f && atirando[i]) {
                recuoPonta[i] = 123.0f; atirando[i] = false;
                for (int t = 0; t < MAX_TIROS_BOSS; t++) {
                    if (!tirosBoss[t].ativo) {
                        tirosBoss[t].ativo = true;
                        float posYTiro = posPontas[i].y;
                        if (i == 3 || i == 4) posYTiro += offsetCanhoesMeioY; else posYTiro -= movimentoCanhaoY; 
                        tirosBoss[t].pos.x = (float)x + ((posPontas[i].x + 56.0f) * escalaBoss); 
                        tirosBoss[t].pos.y = (float)y + (posYTiro * escalaBoss);
                        break; 
                    }
                }
            }
        }
        if (recuoPonta[i] > 0.0f) { recuoPonta[i] -= 307.0f * dt; if (recuoPonta[i] < 0.0f) recuoPonta[i] = 0.0f; }
    }

    for (int j = 0; j < 5; j++) {
        if (intensidadeLuz[j] > 0.0f) { intensidadeLuz[j] -= 2.0f * dt; if (intensidadeLuz[j] < 0.0f) intensidadeLuz[j] = 0.0f; }
    }

    float dxAsaEsq = 614.0f - 799.0f, dyAsaEsq = 676.0f - 778.0f;
    float novoPivoAsaEsqX = 799.0f + (dxAsaEsq * cos(radEsq) - dyAsaEsq * sin(radEsq));
    float novoPivoAsaEsqY = 778.0f + (dxAsaEsq * sin(radEsq) + dyAsaEsq * cos(radEsq));
    float dxJuntaEsq = 604.0f - 799.0f, dyJuntaEsq = 676.0f - 778.0f;
    float novaJuntaEsqX = 799.0f + (dxJuntaEsq * cos(radEsq) - dyJuntaEsq * sin(radEsq));
    float novaJuntaEsqY = 778.0f + (dxJuntaEsq * sin(radEsq) + dyJuntaEsq * cos(radEsq));

    float bossWLocal = 2048.0f;
    float baseBracoDirX = bossWLocal - 799.0f, baseAsaDirX = bossWLocal - 614.0f, baseJuntaDirX = bossWLocal - 604.0f;
    float dxAsaDir = baseAsaDirX - baseBracoDirX, dyAsaDir = 676.0f - 778.0f;
    float novoPivoAsaDirX = baseBracoDirX + (dxAsaDir * cos(radDir) - dyAsaDir * sin(radDir));
    float novoPivoAsaDirY = 778.0f + (dxAsaDir * sin(radDir) + dyAsaDir * cos(radDir));
    float dxJuntaDir = baseJuntaDirX - baseBracoDirX, dyJuntaDir = 676.0f - 778.0f;
    float novaJuntaDirX = baseBracoDirX + (dxJuntaDir * cos(radDir) - dyJuntaDir * sin(radDir));
    float novaJuntaDirY = 778.0f + (dxJuntaDir * sin(radDir) + dyJuntaDir * cos(radDir));

    int minT = acelerado ? 1 : 5, maxT = acelerado ? 10 : 30;
    float velRabo = acelerado ? 30.0f : 5.0f, velPresa = acelerado ? 45.0f : 15.0f, velEspinho = acelerado ? 50.0f : 15.0f;
    float ampRabo = raiva ? 65.0f : 35.0f, ampPresa = raiva ? 45.0f : 25.0f, ampEspinho = raiva ? 140.0f : 90.0f; 

    timerRaboEsq -= dt; if (timerRaboEsq <= 0.0f) { alvoRaboEsq = (float)GetRandomValue(0, (int)ampRabo); timerRaboEsq = (float)GetRandomValue(minT, maxT) / 100.0f; }
    angRaboEsq += (alvoRaboEsq - angRaboEsq) * velRabo * dt; 
    timerRaboDir -= dt; if (timerRaboDir <= 0.0f) { alvoRaboDir = (float)GetRandomValue(0, -(int)ampRabo); timerRaboDir = (float)GetRandomValue(minT, maxT) / 100.0f; }
    angRaboDir += (alvoRaboDir - angRaboDir) * velRabo * dt;

    timerPresaEsq -= dt; if (timerPresaEsq <= 0.0f) { alvoPresaEsq = (float)GetRandomValue(-(int)ampPresa, 0); timerPresaEsq = (float)GetRandomValue(minT, maxT) / 100.0f; }
    angPresaEsq += (alvoPresaEsq - angPresaEsq) * velPresa * dt; 
    timerPresaDir -= dt; if (timerPresaDir <= 0.0f) { alvoPresaDir = (float)GetRandomValue(0, (int)ampPresa); timerPresaDir = (float)GetRandomValue(minT, maxT) / 100.0f; }
    angPresaDir += (alvoPresaDir - angPresaDir) * velPresa * dt;

    timerEspinhoEsq -= dt; if (timerEspinhoEsq <= 0.0f) { alvoEspinhoEsq = (float)GetRandomValue(0, (int)ampEspinho); timerEspinhoEsq = (float)GetRandomValue(minT, maxT + 10) / 100.0f; }
    distEspinhoEsq += (alvoEspinhoEsq - distEspinhoEsq) * velEspinho * dt; 
    timerEspinhoDir -= dt; if (timerEspinhoDir <= 0.0f) { alvoEspinhoDir = (float)GetRandomValue(0, (int)ampEspinho); timerEspinhoDir = (float)GetRandomValue(minT, maxT + 10) / 100.0f; }
    distEspinhoDir += (alvoEspinhoDir - distEspinhoDir) * velEspinho * dt;
    
    float radEspinho = -50.0f * PI / 180.0f;
    float movEspinhoEsqX = distEspinhoEsq * cos(radEspinho), movEspinhoEsqY = distEspinhoEsq * sin(radEspinho);
    float movEspinhoDirX = distEspinhoDir * cos(radEspinho), movEspinhoDirY = distEspinhoDir * sin(radEspinho);

    BeginTextureMode(telaBoss);
    ClearBackground(BLANK);

    auto DrawOrSpawn = [&](Rectangle src, Rectangle dst, Vector2 orig, float rot) {
        if (vida > 0) {
            DrawTexturePro(bglsHD, src, dst, orig, rot, WHITE);
        } else if (!mortoFlag) {
            PecaBoss p;
            p.src = src;
            p.dst = { (float)x + (dst.x * escalaBoss), (float)y + (dst.y * escalaBoss), dst.width * escalaBoss, dst.height * escalaBoss };
            p.orig = { orig.x * escalaBoss, orig.y * escalaBoss };
            p.rot = rot;
            
            float centerX = x + (1024.0f * escalaBoss);
            float centerY = y + (1024.0f * escalaBoss);
            float dx = p.dst.x - centerX; float dy = p.dst.y - centerY;
            float angle = atan2(dy, dx);
            float speed = (float)GetRandomValue(50, 250); 
            p.vel = { cosf(angle) * speed, sinf(angle) * speed }; 
            p.rotVel = (float)GetRandomValue(-150, 150) / 10.0f; 
            p.active = true;
            pecasDestruidas.push_back(p);
        }
    };

    Color corJunta = { 67, 30, 22, 255 };
    if (vida > 0) { 
        DrawCircle(novaJuntaEsqX, novaJuntaEsqY, 46.0f, corJunta); DrawCircle(novaJuntaDirX, novaJuntaDirY, 46.0f, corJunta); 
        DrawCircle(840.0f, 911.0f, 133.0f, corJunta); DrawCircle(1188.0f, 911.0f, 133.0f, corJunta);
        DrawCircle(583.0f, 1074.0f, 60.0f, corJunta); DrawCircle(1460.0f, 1074.0f, 60.0f, corJunta);
    }

    DrawOrSpawn({ 576, 0, 279, 327 }, { 799.0f, 778.0f, 246, 307 }, { 236, 154 }, anguloBracoEsq);
    DrawOrSpawn({ 576, 0, -279, 327 }, { baseBracoDirX, 778.0f, 246, 307 }, { 10, 154 }, anguloBracoDir);
    DrawOrSpawn({ 1679, 604, 143, 297 }, { 973.0f, 532.0f, 143, 297 }, { 123, 164 }, -angRaboEsq);
    DrawOrSpawn({ 1679, 604, -143, 297 }, { 1075.0f, 532.0f, 143, 297 }, { 20, 164 }, -angRaboDir);
    DrawOrSpawn({ 932, 49, 573, 852 }, { 100.0f, 1000.0f - movimentoCanhaoY, 543, 850 }, { 0, 0 }, 0.0f);
    DrawOrSpawn({ 932, 49, -573, 852 }, { (bossWLocal - 100.0f), 1000.0f - movimentoCanhaoY, 543, 850 }, { 543, 0 }, 0.0f);
    DrawOrSpawn({ 1529, 0, 471, 492 }, { 737.0f, 1075.0f, 430, 369 }, { 236, 307 }, anguloBracoInfEsq);
    DrawOrSpawn({ 1529, 0, -471, 492 }, { (bossWLocal - 737.0f), 1075.0f, 430, 369 }, { 195, 307 }, anguloBracoInfDir);
    DrawOrSpawn({ 1361, 777, 73, 83 }, { 827.0f, 1471.0f + offsetCanhoesMeioY, 73, 83 }, { 0, 0 }, 0.0f);
    DrawOrSpawn({ 1361, 777, -73, 83 }, { 1148.0f, 1471.0f + offsetCanhoesMeioY, 73, 83 }, { 0, 0 }, 0.0f);

    DrawOrSpawn({ 635, 461, 215, 440 }, { 765.0f, 1516.0f, 215, 440 }, { 123, 20 }, angPresaEsq);
    DrawOrSpawn({ 635, 461, -215, 440 }, { 1274.0f, 1516.0f, 215, 440 }, { 92, 20 }, angPresaDir);
    
    for (int i = 0; i < 8; i++) {
        float posYPonta = posPontas[i].y - recuoPonta[i];
        if (i == 3 || i == 4) posYPonta += offsetCanhoesMeioY; else posYPonta -= movimentoCanhaoY; 
        DrawOrSpawn({ 440, 0, 113, 154 }, { posPontas[i].x, posYPonta, 113, 154 }, { 0, 0 }, 0.0f);
    }

    DrawTexture(bossSpriteHD, 0, 0, WHITE);

    DrawOrSpawn({ 1898, 619, 298, 256 }, { 648.0f + movEspinhoEsqX, 1130.0f + movEspinhoEsqY, 198, 256 }, { 0, 0 }, 0.0f);
    DrawOrSpawn({ 1898, 619, -298, 256 }, { (bossWLocal - 648.0f) - movEspinhoDirX, 1130.0f + movEspinhoDirY, 198, 256 }, { 198, 0 }, 0.0f);
    DrawOrSpawn({ 2099, 0, 149, 237 }, { 807.0f, 1018.0f, 149, 237 }, { 0, 0 }, 0.0f);
    DrawOrSpawn({ 2099, 0, -149, 237 }, { (bossWLocal - 807.0f), 1018.0f, 149, 237 }, { 149, 0 }, 0.0f);
    DrawOrSpawn({ 0, 0, 389, 819 }, { novoPivoAsaEsqX, novoPivoAsaEsqY, 389, 819 }, { 358, 625 }, anguloAsaEsq);
    DrawOrSpawn({ 0, 0, -389, 819 }, { novoPivoAsaDirX, novoPivoAsaDirY, 389, 819 }, { 31, 625 }, anguloAsaDir);

    BeginBlendMode(BLEND_ADDITIVE);
    for (int j = 0; j < 5; j++) {
        if (vida > 0 && intensidadeLuz[j] > 0.0f) { 
            float raio = 154.0f * intensidadeLuz[j]; 
            Color corCentro = { 255, 50, 50, (unsigned char)(255 * intensidadeLuz[j]) };
            DrawCircleGradient((int)posLuzes[j].x, (int)(posLuzes[j].y - ((j == 2) ? 0 : movimentoCanhaoY)), raio, corCentro, { 255, 0, 0, 0 });
        }
    }
    EndBlendMode();

    if (vida <= 0) mortoFlag = true;

    EndTextureMode();
}

void Boss::ComportamentoVivo(float mult, float dt, int& vidaPlayer, int death_x, int death_y, bool& tocaSomDano) {
    AtualizarEDesenhar();

    if (y < 0) y += (int)(2 * mult);

    bossMoveTimer -= dt;
    float velAtualBoss = 150.0f; 
    if (vida <= 30) velAtualBoss = 220.0f;
    if (vida <= 10) velAtualBoss = 350.0f;

    x += bossDir * (int)(velAtualBoss * dt);

    float larguraBoss = 2048.0f * escalaBoss; 
    if (x + larguraBoss >= 1200) { x = 1200 - larguraBoss; bossDir = -1; bossMoveTimer = (float)GetRandomValue(2, 20) / 10.0f; } 
    else if (x <= 0) { x = 0; bossDir = 1; bossMoveTimer = (float)GetRandomValue(2, 20) / 10.0f; } 
    else if (bossMoveTimer <= 0.0f) { bossDir = (GetRandomValue(0, 1) == 0) ? -1 : 1; bossMoveTimer = (float)GetRandomValue(2, 20) / 10.0f; }

    float tremorX = (tempoRaiva > 0.0f) ? GetRandomValue(-3, 3) : 0;
    float tremorY = (tempoRaiva > 0.0f) ? GetRandomValue(-3, 3) : 0;

    Rectangle source = { 0.0f, 0.0f, 2048.0f, -2048.0f };
    Rectangle dest = { (float)x + tremorX, (float)y + tremorY, 2048.0f * escalaBoss, 2048.0f * escalaBoss };
    DrawTexturePro(telaBoss.texture, source, dest, {0, 0}, 0.0f, WHITE);

    DrawText(TextFormat("%d", vida), x + 130, y - 20, 20, RED);

    BeginBlendMode(BLEND_ADDITIVE);
    for (int t = 0; t < MAX_TIROS_BOSS; t++) {
        if (tirosBoss[t].ativo) {
            tirosBoss[t].pos.y += 400.0f * dt; 
            DrawEllipse((int)tirosBoss[t].pos.x, (int)tirosBoss[t].pos.y, 4.0f, 15.0f, RED);
            DrawEllipse((int)tirosBoss[t].pos.x, (int)tirosBoss[t].pos.y, 2.0f, 10.0f, ORANGE);

            // Verifica colisão do tiro do Boss com o Player
            if (vidaPlayer > 0 && tirosBoss[t].pos.y >= death_y && tirosBoss[t].pos.y <= death_y + 80 && 
                tirosBoss[t].pos.x >= death_x && tirosBoss[t].pos.x <= death_x + 50) {
                tocaSomDano = true; // Avisa o jogo principal para tocar o som
                vidaPlayer = 0;
                tirosBoss[t].ativo = false;
            }

            if (tirosBoss[t].pos.y > 750) tirosBoss[t].ativo = false;
        }
    }
    EndBlendMode();
}

void Boss::ComportamentoMorto(float mult, float dt, bool& boss_defeated, bool& winn, int dist_traveled, int dist_total, bool& engatilhaExplosao) {
    float alphaFade = (120.0f - timerMorte) / 120.0f;
    if (alphaFade < 0.0f) alphaFade = 0.0f;

    Rectangle source = { 0.0f, 0.0f, 2048.0f, -2048.0f };
    Rectangle dest = { (float)x + GetRandomValue(-6, 6), (float)y + GetRandomValue(-6, 6), 2048.0f * escalaBoss, 2048.0f * escalaBoss };
    DrawTexturePro(telaBoss.texture, source, dest, {0, 0}, 0.0f, ColorAlpha(WHITE, alphaFade));

    for (auto& p : pecasDestruidas) {
        if (p.active) {
            p.dst.x += p.vel.x * dt;
            p.dst.y += p.vel.y * dt;
            p.vel.y += 50.0f * dt; 
            p.rot += p.rotVel * dt; 
            DrawTexturePro(bglsHD, p.src, p.dst, p.orig, p.rot, WHITE);
        }
    }

    if (deathSoundDelay > 0) deathSoundDelay -= (1.0f * mult);
    if (deathSoundDelay <= 0 && bombCount < 8) {
        engatilhaExplosao = true; // Avisa o jogo principal para estourar a partícula e som
        bombCount++;
        deathSoundDelay = 15.0f; 
    }
    
    timerMorte += (1.0f * mult);
    if (timerMorte >= 1300.0f) { 
        boss_defeated = true; 
        if (dist_traveled >= dist_total) { winn = true; }
        Resetar(); 
    }
}