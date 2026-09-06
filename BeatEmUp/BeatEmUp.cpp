#include <string>       // Manipulação de textos dinâmicos e caminhos
#include <filesystem>   // Leitura e validação de diretorios/arquivos do SO
#include <map>          // Estrutura de dicionário

// 1. Mágica de macros para proteger o Raylib (SEM NOGDI e NOUSER)
#define Rectangle WinRectangle
#define CloseWindow WinCloseWindow
#define ShowCursor WinShowCursor
#define DrawText WinDrawText
#define DrawTextEx WinDrawTextEx
#define PlaySound WinPlaySound

// 2. Inclui a API do Windows e ferramentas de pasta com todos os privilégios
#include <windows.h>    // API base do Windows
#include <commdlg.h>    // Caixas de diálogo nativas do Windows para salvar/abrir arquivos
#include <shobjidl.h>   // Interface avançada para seleção de pastas no Windows

// 3. Limpa as macros renomeadas para o Raylib
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef DrawTextEx
#undef PlaySound
#undef near
#undef far
#undef LoadImage

// 4. Inclui Raylib e as suas classes
#include "raylib.h"
#include "Physics.h" 
#include "Animator.h" 
#include <math.h>       

std::string SaveFileDialog() {
    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileNameA(&ofn) == TRUE) {
        std::string path = ofn.lpstrFile;
        if (path.find(".txt") == std::string::npos) path += ".txt";
        return path;
    }
    return "";
}

std::string OpenFileDialog() {
    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    return "";
}

std::string OpenFolderDialog() {
    std::string folderPath = "";
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog *pFileOpen;
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        if (SUCCEEDED(hr)) {
            DWORD dwOptions;
            pFileOpen->GetOptions(&dwOptions);
            pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
            hr = pFileOpen->Show(NULL);
            if (SUCCEEDED(hr)) {
                IShellItem *pItem;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    if (SUCCEEDED(hr)) {
                        std::wstring ws(pszFilePath);
                        folderPath = std::string(ws.begin(), ws.end());
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
    return folderPath;
}

struct DragTarget {
    std::shared_ptr<Particle> p;
    std::shared_ptr<Particle> parent;
    int angleIdx;
};

struct Character {
    PhysicsWorld world;
    Animator animator;
    
    enum Mode { ANIMATOR, RAGDOLL, GAMEPLAY };
    Mode currentMode = ANIMATOR;
    float velocityY = 0.0f;
    float facingDir = 1.0f; // 1.0f = Direita, -1.0f = Esquerda

    bool isTransformed = false;
    bool isTransforming = false;
    float transformCooldown = 0.0f;
    bool isAttacking = false;
    float lastAnimX = 0.0f;
    int comboStep = 0;        // Guarda qual ataque está tocando (1 a 4)
    bool inputBuffer = false; // Guarda se o jogador apertou E antecipadamente
    bool isHit = false;
    float stunTimer = 0.0f;
    bool hasDealtDamage = false;
    bool isBlocking = false;

    void TakeDamage(std::string hitAnim, float attackerDir, bool guardBreak = false) {
        isHit = true;
        isAttacking = false;     
        isTransforming = false;  
        comboStep = 0;
        inputBuffer = false;
        hasDealtDamage = false; 

        if (guardBreak) isBlocking = false; // Quebra a guarda forçadamente no stun

        animator.currentAnim = hitAnim;
        animator.currentFrame = 0.0f;
        animator.isBlending = false;
        animator.animFinished = false;

        if (animator.animations.find(hitAnim) != animator.animations.end()) {
            float totalFrames = (float)animator.animations[hitAnim].size();
            stunTimer = totalFrames / animator.playSpeed;
        } else {
            stunTimer = 0.5f; 
        }

        facingDir = attackerDir * -1.0f; 
    }
    
    void Control(float dt, float groundY) {
        if (isHit) {
            stunTimer -= dt;
            if (stunTimer <= 0.0f) isHit = false; // Sai do stun
        } 
        else {
            bool isMoving = false;
            bool isRunning = IsKeyDown(KEY_LEFT_SHIFT);
            float speed = isRunning ? 1200.0f : 600.0f;

            isBlocking = (IsKeyDown(KEY_Q) && !isAttacking && !isTransforming && !isTransformed);

            // 1. Cooldown
            if (transformCooldown > 0.0f) transformCooldown -= dt;

            // 2. Input de Transformação
            if (IsKeyPressed(KEY_F) && transformCooldown <= 0.0f && !isAttacking) {
                isTransforming = true;
                isTransformed = !isTransformed;
                transformCooldown = 2.0f; 
                
                if (isTransformed) {
                    animator.Play("transformation", currentAngles);
                    animator.playDirection = 1.0f;
                } else {
                    animator.playDirection = -1.0f;
                    animator.currentFrame = 5.0f;
                }
            }

            // 3. Input de Ataque (com Input Buffer para o Combo)
            if (IsKeyPressed(KEY_E) && !isTransformed && !isTransforming && !isBlocking) {
                if (!isAttacking) {
                    isAttacking = true;
                    comboStep = 1;
                    hasDealtDamage = false; // NOVO: Libera o dano
                    animator.Play("attack1", currentAngles);
                    lastAnimX = animator.animations["attack1"][0].rootPos.x; 
                    inputBuffer = false;
                } else if (comboStep < 5) {
                    inputBuffer = true;
                }
            }

            // 4. Movimento Lateral
            if (!isAttacking && !isTransforming && !isBlocking) {
                if (IsKeyDown(KEY_A)) { pPelvis->position.x -= speed * dt; isMoving = true; facingDir = -1.0f; }
                if (IsKeyDown(KEY_D)) { pPelvis->position.x += speed * dt; isMoving = true; facingDir = 1.0f; }
            }

            // 5. Checagem de fim da Transformação
            if (isTransforming) {
                if (isTransformed && animator.currentFrame >= 5.0f) {
                    isTransforming = false; 
                } else if (!isTransformed && animator.currentFrame <= 0.0f) {
                    isTransforming = false; 
                    animator.playDirection = 1.0f; 
                }
            }

            // 6. Checagem de fim do Ataque e Transição de Combo
            if (isAttacking && animator.animFinished) {
                if (inputBuffer && comboStep < 5) {
                    comboStep++;
                    hasDealtDamage = false; // NOVO: Libera o dano do próximo golpe
                    std::string nextAttack = "attack" + std::to_string(comboStep); 
                    animator.Play(nextAttack, currentAngles);
                    lastAnimX = animator.animations[nextAttack][0].rootPos.x; 
                    inputBuffer = false; 
                } else {
                    isAttacking = false;
                    comboStep = 0;
                    inputBuffer = false;
                }
            }

            // 7. Máquina de Estados de Animação
            if (!isTransforming && !isAttacking) {
                if (isBlocking) {
                    animator.Play("block", currentAngles); // Roda o bloqueio e trava no último frame naturalmente
                } else if (isTransformed) {
                    animator.currentAnim = "transformation";
                    animator.currentFrame = 5.0f;
                } else {
                    if (isMoving) {
                        if (isRunning) animator.Play("running", currentAngles);
                        else animator.Play("walking", currentAngles);
                    } else {
                        animator.Play("idle", currentAngles);
                    }
                }
            }
        }

        // 1. Posição Y base de quando o boneco está em pé no chão
        float basePelvisY = groundY - 422.5f;

        // 2. Calcula a flexão/salto interpolado da animação para evitar flicks
        float animOffsetY = 0.0f;
        if (animator.animations.find(animator.currentAnim) != animator.animations.end()) {
            auto& track = animator.animations[animator.currentAnim];
            
            int idx1 = (int)animator.currentFrame;
            int idx2 = (idx1 + 1) % track.size();
            float blend = animator.currentFrame - idx1;

            // Extrai a posição Y exata calculando a transição entre um frame e outro
            float currentAnimY = track[idx1].rootPos.y + (track[idx2].rootPos.y - track[idx1].rootPos.y) * blend;
            
            // 482.5f é a altura padrão da pélvis no modo ANIMATOR
            animOffsetY = currentAnimY - 482.5f; 
        }

        // 3. Aplica o Root Motion X exclusivo para o Ataque
        // O find() verifica se o nome da animação atual contém a palavra "attack"
        if (isAttacking && animator.currentAnim.find("attack") != std::string::npos && !animator.isBlending) {
            auto& track = animator.animations[animator.currentAnim];
            int idx1 = (int)animator.currentFrame;
            int idx2 = (idx1 + 1) % track.size();
            float blend = animator.currentFrame - idx1;
            
            float currentAnimX = track[idx1].rootPos.x + (track[idx2].rootPos.x - track[idx1].rootPos.x) * blend;
            float deltaX = (currentAnimX - lastAnimX) * facingDir; 
            
            pPelvis->position.x += deltaX; 
            lastAnimX = currentAnimX;      
        }

        // 4. Aplica o movimento vertical da animação direto no corpo
        pPelvis->position.y = basePelvisY + animOffsetY;
        velocityY = 0.0f;
    }

    bool showInitialFrame = false;
    
    float currentAngles[14];
    float boneLengths[14];
    int draggedJointIndex = -1; 
    DragTarget activeDragTarget;

    std::shared_ptr<Particle> pHead, pNeck, pPelvis;
    std::shared_ptr<Particle> pLElbow, pLHand, pRElbow, pRHand;
    std::shared_ptr<Particle> pLKnee, pLFoot, pRKnee, pRFoot;
    std::shared_ptr<Particle> pLToe, pRToe;
    std::shared_ptr<Particle> pLFist, pRFist;

    Texture2D texTorso = {0}, texHead = {0}, texArm = {0}, texForearm = {0}, texThigh = {0}, texCalf = {0}, texElbow = {0}, texKnee = {0}, texFoot = {0}, texFist = {0};

    void Init(Vector2 startPos) {
        // Tenta carregar a skin padrão da pasta
        if (!LoadSkin("Textures\\Default")) {
            // Fallback: Gera texturas brancas na memória se a pasta não for encontrada
            TraceLog(LOG_WARNING, "Skin padrao nao encontrada. Gerando texturas base.");
            Image imgTorso = GenImageColor(160, 320, BLUE); texTorso = LoadTextureFromImage(imgTorso); UnloadImage(imgTorso);
            Image imgHead = GenImageColor(160, 160, LIGHTGRAY); texHead = LoadTextureFromImage(imgHead); UnloadImage(imgHead);
            Image imgArm = GenImageColor(80, 160, YELLOW); texArm = LoadTextureFromImage(imgArm); UnloadImage(imgArm);
            Image imgForearm = GenImageColor(80, 160, YELLOW); texForearm = LoadTextureFromImage(imgForearm); UnloadImage(imgForearm);
            Image imgThigh = GenImageColor(75, 160, GREEN); texThigh = LoadTextureFromImage(imgThigh); UnloadImage(imgThigh);
            Image imgCalf = GenImageColor(75, 160, GREEN); texCalf = LoadTextureFromImage(imgCalf); UnloadImage(imgCalf);
            Image imgElbow = GenImageColor(80, 80, BLACK); ImageDrawCircle(&imgElbow, 40, 40, 40, WHITE); texElbow = LoadTextureFromImage(imgElbow); UnloadImage(imgElbow);
            Image imgKnee = GenImageColor(75, 75, BLACK); ImageDrawCircle(&imgKnee, 37, 37, 37, WHITE); texKnee = LoadTextureFromImage(imgKnee); UnloadImage(imgKnee);
            Image imgFoot = GenImageColor(45, 150, WHITE); texFoot = LoadTextureFromImage(imgFoot); UnloadImage(imgFoot);
            // Image imgAnkle = GenImageColor(35, 35, GREEN); texAnkle = LoadTextureFromImage(imgAnkle); UnloadImage(imgAnkle);
            Image imgFist = GenImageColor(90, 90, RED); texFist = LoadTextureFromImage(imgFist); UnloadImage(imgFist);
        }

        pNeck = std::make_shared<Particle>((Vector2){startPos.x, startPos.y + 20.0f}, 60.0f);
        pHead = std::make_shared<Particle>((Vector2){startPos.x, startPos.y - 160.0f}, 60.0f);
        
        pPelvis = std::make_shared<Particle>((Vector2){startPos.x, startPos.y + 282.5f}, 60.0f);
        
        pLElbow = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 140.0f}, 30.0f, 1.0f, false, GROUP_ARM);
        pLHand = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 260.0f}, 30.0f, 1.0f, false, GROUP_ARM);
        pRElbow = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 140.0f}, 30.0f, 1.0f, false, GROUP_ARM);
        pRHand = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 260.0f}, 30.0f, 1.0f, false, GROUP_ARM);

        pLKnee = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 482.5f}, 35.0f, 1.0f, false, GROUP_LEG);
        pLFoot = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 682.5f}, 22.5f, 1.0f, false, GROUP_LEG);
        pRKnee = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 482.5f}, 35.0f, 1.0f, false, GROUP_LEG);
        pRFoot = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 682.5f}, 22.5f, 1.0f, false, GROUP_LEG);
        
        pLToe = std::make_shared<Particle>((Vector2){startPos.x + 140.0f, startPos.y + 682.5f}, 22.5f, 1.0f, false, GROUP_LEG);
        pRToe = std::make_shared<Particle>((Vector2){startPos.x + 140.0f, startPos.y + 682.5f}, 22.5f, 1.0f, false, GROUP_LEG);

        pLFist = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 350.0f}, 30.0f, 1.0f, false, GROUP_ARM);
        pRFist = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 350.0f}, 30.0f, 1.0f, false, GROUP_ARM);

        world.particles = { pHead, pNeck, pPelvis, pLElbow, pLHand, pRElbow, pRHand, pLKnee, pLFoot, pRKnee, pRFoot, pLToe, pRToe, pLFist, pRFist };

        auto addBone = [&](std::shared_ptr<Particle> p1, std::shared_ptr<Particle> p2) {
            float dx = p2->position.x - p1->position.x;
            float dy = p2->position.y - p1->position.y;
            world.constraints.push_back(std::make_shared<Constraint>(p1, p2, sqrt(dx*dx + dy*dy)));
        };

        addBone(pNeck, pHead); addBone(pNeck, pPelvis);
        addBone(pNeck, pLElbow); addBone(pLElbow, pLHand);
        addBone(pNeck, pRElbow); addBone(pRElbow, pRHand);
        addBone(pPelvis, pLKnee); addBone(pLKnee, pLFoot);
        addBone(pPelvis, pRKnee); addBone(pRKnee, pRFoot);
        addBone(pLFoot, pLToe);
        addBone(pRFoot, pRToe);
        addBone(pLHand, pLFist);
        addBone(pRHand, pRFist);

        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pNeck, pLElbow, pLHand, -0.5f, 2.5f)); 
        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pNeck, pRElbow, pRHand, -0.5f, 2.5f)); 
        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pPelvis, pRKnee, pRFoot, -2.5f, 0.5f));
        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pPelvis, pLKnee, pLFoot, -2.5f, 0.5f));
        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pLKnee, pLFoot, pLToe, -1.0f, 1.0f));
        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pRKnee, pRFoot, pRToe, -1.0f, 1.0f));
        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pLElbow, pLHand, pLFist, -1.0f, 1.0f));
        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pRElbow, pRHand, pRFist, -1.0f, 1.0f));

        auto calcDistAngle = [](std::shared_ptr<Particle> p1, std::shared_ptr<Particle> p2, float& len, float& ang) {
            float dx = p2->position.x - p1->position.x;
            float dy = p2->position.y - p1->position.y;
            len = sqrt(dx*dx + dy*dy);
            ang = atan2(dy, dx);
        };

        calcDistAngle(pPelvis, pNeck, boneLengths[0], currentAngles[0]);
        calcDistAngle(pNeck, pHead, boneLengths[1], currentAngles[1]);
        calcDistAngle(pNeck, pLElbow, boneLengths[2], currentAngles[2]);
        calcDistAngle(pLElbow, pLHand, boneLengths[3], currentAngles[3]);
        calcDistAngle(pNeck, pRElbow, boneLengths[4], currentAngles[4]);
        calcDistAngle(pRElbow, pRHand, boneLengths[5], currentAngles[5]);
        calcDistAngle(pPelvis, pLKnee, boneLengths[6], currentAngles[6]);
        calcDistAngle(pLKnee, pLFoot, boneLengths[7], currentAngles[7]);
        calcDistAngle(pPelvis, pRKnee, boneLengths[8], currentAngles[8]);
        calcDistAngle(pRKnee, pRFoot, boneLengths[9], currentAngles[9]);
        calcDistAngle(pLFoot, pLToe, boneLengths[10], currentAngles[10]);
        calcDistAngle(pRFoot, pRToe, boneLengths[11], currentAngles[11]);
        calcDistAngle(pLHand, pLFist, boneLengths[12], currentAngles[12]);
        calcDistAngle(pRHand, pRFist, boneLengths[13], currentAngles[13]);

        animator.LoadToLibrary("animations\\idle.txt", "idle");
        animator.LoadToLibrary("animations\\walking.txt", "walking");
        animator.LoadToLibrary("animations\\running.txt", "running");
        animator.Play("idle", currentAngles); // Estado inicial
        animator.LoadToLibrary("animations\\transformation.txt", "transformation", false);
        if (animator.animations["transformation"].size() != 6) {
            TraceLog(LOG_ERROR, "ERRO: O arquivo transformation.txt deve ter exatamente 6 frames!");
        }

        // ATTACKS
        animator.LoadToLibrary("animations\\attack1.txt", "attack1", false);
        animator.LoadToLibrary("animations\\attack2.txt", "attack2", false);
        animator.LoadToLibrary("animations\\attack3.txt", "attack3", false);
        animator.LoadToLibrary("animations\\attack4.txt", "attack4", false);
        animator.LoadToLibrary("animations\\attack5.txt", "attack5", false);

        // BLOCKS
        animator.LoadToLibrary("animations\\block.txt", "block", false);
        animator.LoadToLibrary("animations\\blockReactHigh.txt", "blockReactHigh", false);
        animator.LoadToLibrary("animations\\blockReactMedium.txt", "blockReactMedium", false);
        animator.LoadToLibrary("animations\\blockReactLow.txt", "blockReactLow", false);
        animator.LoadToLibrary("animations\\blockReactStrong.txt", "blockReactStrong", false);

        // REACTIONS
        animator.LoadToLibrary("animations\\hit1.txt", "hit1", false); // Alto
        animator.LoadToLibrary("animations\\hit2.txt", "hit2", false); // Médio
        animator.LoadToLibrary("animations\\hit3.txt", "hit3", false); // Baixo
        animator.LoadToLibrary("animations\\bighitAndFalling.txt", "bighit", false); // Forte

    }

    bool LoadSkin(const std::string& folderPath) {
        namespace fs = std::filesystem;
        std::string files[10] = {"texTorso.png", "texHead.png", "texArm.png", "texForearm.png", "texThigh.png", "texCalf.png", "texElbow.png", "texKnee.png", "texFoot.png", "texFist.png"};
        
        // 1. Valida se todas as imagens existem na pasta
        for (const auto& file : files) {
            if (!fs::exists(folderPath + "\\" + file)) return false;
        }

        // 2. Descarrega as texturas antigas da GPU para evitar memory leak
        UnloadTexture(texTorso);
        UnloadTexture(texHead);
        UnloadTexture(texArm);
        UnloadTexture(texForearm);
        UnloadTexture(texThigh);
        UnloadTexture(texCalf);
        UnloadTexture(texElbow);
        UnloadTexture(texKnee);
        UnloadTexture(texFoot);
        UnloadTexture(texFist);

        // 3. Carrega as novas texturas
        texTorso = LoadTexture((folderPath + "\\texTorso.png").c_str());
        texHead = LoadTexture((folderPath + "\\texHead.png").c_str());
        texArm = LoadTexture((folderPath + "\\texArm.png").c_str());
        texForearm = LoadTexture((folderPath + "\\texForearm.png").c_str());
        texThigh = LoadTexture((folderPath + "\\texThigh.png").c_str());
        texCalf = LoadTexture((folderPath + "\\texCalf.png").c_str());
        texElbow = LoadTexture((folderPath + "\\texElbow.png").c_str());
        texKnee = LoadTexture((folderPath + "\\texKnee.png").c_str());
        texFoot = LoadTexture((folderPath + "\\texFoot.png").c_str());
        texFist = LoadTexture((folderPath + "\\texFist.png").c_str()); 

        return true;
    }

    void ApplyFK() {
        auto setPos = [&](std::shared_ptr<Particle> p, std::shared_ptr<Particle> parent, int angleIdx) {
            // Multiplica apenas o X (cos) pela direção
            p->position.x = parent->position.x + (cos(currentAngles[angleIdx]) * boneLengths[angleIdx]) * facingDir;
            p->position.y = parent->position.y + sin(currentAngles[angleIdx]) * boneLengths[angleIdx];
            p->previousPosition = p->position;
        };

        pPelvis->previousPosition = pPelvis->position;
        setPos(pNeck, pPelvis, 0);
        setPos(pHead, pNeck, 1);
        setPos(pLElbow, pNeck, 2);
        setPos(pLHand, pLElbow, 3);
        setPos(pRElbow, pNeck, 4);
        setPos(pRHand, pRElbow, 5);
        setPos(pLKnee, pPelvis, 6);
        setPos(pLFoot, pLKnee, 7);
        setPos(pRKnee, pPelvis, 8);
        setPos(pRFoot, pRKnee, 9);
        setPos(pLToe, pLFoot, 10);
        setPos(pRToe, pRFoot, 11);
        setPos(pLFist, pLHand, 12);
        setPos(pRFist, pRHand, 13);
    }

    void Update(float groundY, Vector2 mouseWorld, Vector2 mouseScreen) {
        Rectangle uiPanel = { 10, 10, 250, 480 }; 

        if (currentMode == RAGDOLL) {
            // Checa a UI com o mouse da TELA
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mouseScreen, uiPanel)) {
                for (auto& p : world.particles) {
                    // Checa a colisão física com o mouse do MUNDO
                    if (CheckCollisionPointCircle(mouseWorld, p->position, p->radius * 1.5f)) {
                        activeDragTarget.p = p;
                        draggedJointIndex = 99; 
                        break;
                    }
                }
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggedJointIndex == 99) {
                activeDragTarget.p->position = mouseWorld;
                activeDragTarget.p->previousPosition = mouseWorld;
            }
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) draggedJointIndex = -1;

            world.Update(0.0f, 1600.0f, 0.0f, groundY); 
        } 
        else if (currentMode == ANIMATOR) {
            animator.Update(GetFrameTime(), currentAngles, pPelvis->position);

            if (!animator.isPlaying) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mouseScreen, uiPanel)) {
                    std::vector<DragTarget> targets = {
                        {pNeck, pPelvis, 0}, {pHead, pNeck, 1},
                        {pLElbow, pNeck, 2}, {pLHand, pLElbow, 3},
                        {pRElbow, pNeck, 4}, {pRHand, pRElbow, 5},
                        {pLKnee, pPelvis, 6}, {pLFoot, pLKnee, 7},
                        {pRKnee, pPelvis, 8}, {pRFoot, pRKnee, 9},
                        {pLToe, pLFoot, 10}, {pRToe, pRFoot, 11},
                        {pLFist, pLHand, 12}, {pRFist, pRHand, 13}
                    };
                    
                    if (CheckCollisionPointCircle(mouseWorld, pPelvis->position, 25.0f)) {
                        draggedJointIndex = -2;
                    } else {
                        for (auto& t : targets) {
                            if (CheckCollisionPointCircle(mouseWorld, t.p->position, 25.0f)) {
                                activeDragTarget = t;
                                draggedJointIndex = t.angleIdx;
                                break;
                            }
                        }
                    }
                }
                
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    if (draggedJointIndex == -2) {
                        pPelvis->position = mouseWorld;
                    } else if (draggedJointIndex >= 0) {
                        float dx = mouseWorld.x - activeDragTarget.parent->position.x;
                        float dy = mouseWorld.y - activeDragTarget.parent->position.y;
                        currentAngles[draggedJointIndex] = atan2(dy, dx);
                    }
                }
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) draggedJointIndex = -1;
            }
            ApplyFK();
        }
        else if (currentMode == GAMEPLAY) {
            Control(GetFrameTime(), groundY);
            animator.UpdateGameplay(GetFrameTime(), currentAngles);
            ApplyFK();
        }
    }

    void DrawGhostFrame(Pose& pose, Color tint) {
        Vector2 gPelvis = pose.rootPos;
        auto calcFK = [&](int angleIdx, Vector2 parentPos) -> Vector2 {
            float angle = pose.angles[angleIdx];
            float len = boneLengths[angleIdx];
            return { parentPos.x + (cos(angle) * len) * facingDir, parentPos.y + sin(angle) * len }; // X alterado
        };
        
        Vector2 gNeck = calcFK(0, gPelvis);
        Vector2 gHead = calcFK(1, gNeck);
        Vector2 gLElbow = calcFK(2, gNeck);
        Vector2 gLHand = calcFK(3, gLElbow);
        Vector2 gRElbow = calcFK(4, gNeck);
        Vector2 gRHand = calcFK(5, gRElbow);
        Vector2 gLKnee = calcFK(6, gPelvis);
        Vector2 gLFoot = calcFK(7, gLKnee);
        Vector2 gRKnee = calcFK(8, gPelvis);
        Vector2 gRFoot = calcFK(9, gRKnee);
        Vector2 gLToe = calcFK(10, gLFoot);
        Vector2 gRToe = calcFK(11, gRFoot);
        Vector2 gLFist = calcFK(12, gLHand);
        Vector2 gRFist = calcFK(13, gRHand);

        auto DrawGhostBone = [&](Texture2D tex, Vector2 p1, Vector2 p2, float width, float lengthBonus = 0.0f, float offsetBack = 0.0f) {
            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            float angle = atan2(dy, dx) * RAD2DEG;
            float length = sqrt(dx*dx + dy*dy) + lengthBonus + offsetBack; // Aumenta o tamanho total
            Rectangle source = { 0.0f, 0.0f, (float)tex.width * facingDir, (float)tex.height };
            Rectangle dest = { p1.x, p1.y, width, length };
            Vector2 origin = { width / 2.0f, offsetBack }; // Move o eixo de rotação para frente, criando o calcanhar atrás
            DrawTexturePro(tex, source, dest, origin, angle - 90.0f, tint);
        };

        auto DrawGhostJointTex = [&](Texture2D tex, Vector2 pos, float size) {
            Rectangle source = { 0.0f, 0.0f, (float)tex.width * facingDir, (float)tex.height };
            Rectangle dest = { pos.x, pos.y, size, size };
            Vector2 origin = { size / 2.0f, size / 2.0f };
            DrawTexturePro(tex, source, dest, origin, 0.0f, tint);
        };

        DrawGhostJointTex(texElbow, gRElbow, 80.0f);
        DrawGhostJointTex(texKnee, gRKnee, 75.0f);
        DrawGhostJointTex(texElbow, gLElbow, 80.0f);
        DrawGhostJointTex(texKnee, gLKnee, 75.0f);

        DrawGhostBone(texArm, gNeck, gRElbow, 80.0f); DrawGhostBone(texForearm, gRElbow, gRHand, 80.0f, 30.0f);
        DrawGhostBone(texThigh, gPelvis, gRKnee, 75.0f); DrawGhostBone(texCalf, gRKnee, gRFoot, 75.0f);
        DrawGhostBone(texTorso, gNeck, gPelvis, 160.0f, 37.5f);
        DrawGhostBone(texHead, gNeck, gHead, 160.0f);
        DrawGhostBone(texArm, gNeck, gLElbow, 80.0f); DrawGhostBone(texForearm, gLElbow, gLHand, 80.0f, 30.0f);

        // Tornozelos Quadrados (Fundo)
        DrawRectangle(gLFoot.x - 22.5f, gLFoot.y - 22.5f, 45.0f, 45.0f, tint);
        DrawRectangle(gRFoot.x - 22.5f, gRFoot.y - 22.5f, 45.0f, 45.0f, tint);

        DrawGhostBone(texThigh, gPelvis, gLKnee, 75.0f); DrawGhostBone(texCalf, gLKnee, gLFoot, 75.0f);
        DrawGhostBone(texFoot, gLFoot, gLToe, 45.0f, 0.0f, 30.0f); 
        DrawGhostBone(texFoot, gRFoot, gRToe, 45.0f, 0.0f, 30.0f);

        DrawGhostBone(texFist, gLHand, gLFist, 90.0f);
        DrawGhostBone(texFist, gRHand, gRFist, 90.0f);
    }

    void Draw() {
        if (!animator.track.empty()) {
            if (showInitialFrame) {
                // Pulsa a opacidade entre 0.1 e 0.5
                float pulse = 0.3f + 0.2f * sin(GetTime() * 6.0f); 
                DrawGhostFrame(animator.track[0], Fade(BLUE, pulse));
            }
            if (!animator.isPlaying && currentMode != RAGDOLL) {
                DrawGhostFrame(animator.track.back(), Fade(DARKGRAY, 0.4f));
            }
        }

        auto DrawBone = [&](Texture2D tex, std::shared_ptr<Particle> p1, std::shared_ptr<Particle> p2, float width, Color tint, float lengthBonus = 0.0f, float offsetBack = 0.0f) {
            float dx = p2->position.x - p1->position.x;
            float dy = p2->position.y - p1->position.y;
            float angle = atan2(dy, dx) * RAD2DEG;
            float length = sqrt(dx*dx + dy*dy) + lengthBonus + offsetBack;
            Rectangle source = { 0.0f, 0.0f, (float)tex.width * facingDir, (float)tex.height };
            Rectangle dest = { p1->position.x, p1->position.y, width, length };
            Vector2 origin = { width / 2.0f, offsetBack }; 
            DrawTexturePro(tex, source, dest, origin, angle - 90.0f, tint);
        };

        auto DrawJointTex = [&](Texture2D tex, Vector2 pos, float size, Color tint) {
            Rectangle source = { 0.0f, 0.0f, (float)tex.width * facingDir, (float)tex.height };
            Rectangle dest = { pos.x, pos.y, size, size };
            Vector2 origin = { size / 2.0f, size / 2.0f };
            DrawTexturePro(tex, source, dest, origin, 0.0f, tint);
        };

        DrawJointTex(texElbow, pLElbow->position, 80.0f, WHITE); // Junta do Cotovelo Esquerdo (Braço de trás)
        DrawJointTex(texKnee, pLKnee->position, 75.0f, WHITE);   // Junta do Joelho Esquerdo (Perna de trás)

        DrawBone(texArm, pNeck, pLElbow, 80.0f, WHITE); DrawBone(texForearm, pLElbow, pLHand, 80.0f, WHITE, 30.0f);   // Braço e Antebraço Esquerdos (Trás)
        DrawBone(texFist, pLHand, pLFist, 90.0f, WHITE);

        DrawBone(texThigh, pPelvis, pLKnee, 75.0f, WHITE); DrawBone(texCalf, pLKnee, pLFoot, 75.0f, WHITE);    // Coxa e Canela Esquerdas (Trás)
        DrawBone(texFoot, pLFoot, pLToe, 45.0f, WHITE, 0.0f, 30.0f);
        
        DrawBone(texTorso, pNeck, pPelvis, 160.0f, WHITE, 37.5f); // Tronco
        
        DrawBone(texHead, pNeck, pHead, 160.0f, WHITE);           // Cabeça

        DrawJointTex(texKnee, pRKnee->position, 75.0f, WHITE);   // Junta do Joelho Direito (Perna da frente)
        DrawBone(texThigh, pPelvis, pRKnee, 75.0f, WHITE); DrawBone(texCalf, pRKnee, pRFoot, 75.0f, WHITE);    // Coxa e Canela Direitas (Frente)
        DrawBone(texFoot, pRFoot, pRToe, 45.0f, WHITE, 0.0f, 38.0f);

        DrawJointTex(texElbow, pRElbow->position, 80.0f, WHITE); // Junta do Cotovelo Direito (Braço da frente)
        DrawBone(texFist, pRHand, pRFist, 90.0f, WHITE);
        DrawBone(texArm, pNeck, pRElbow, 80.0f, WHITE); DrawBone(texForearm, pRElbow, pRHand, 80.0f, WHITE, 30.0f); // Braço e Antebraço Direitos (Frente)

        if (currentMode != RAGDOLL) {
            auto drawJoint = [](std::shared_ptr<Particle> p) {
                DrawCircleV(p->position, 15.0f, Fade(DARKGRAY, 0.7f));
                DrawCircleLines(p->position.x, p->position.y, 15.0f, BLACK);
            };
            drawJoint(pPelvis); drawJoint(pNeck); drawJoint(pHead);
            drawJoint(pLElbow); drawJoint(pLHand); drawJoint(pRElbow); drawJoint(pRHand);
            drawJoint(pLKnee); drawJoint(pLFoot); drawJoint(pRKnee); drawJoint(pRFoot);
            drawJoint(pLToe); drawJoint(pRToe);
            drawJoint(pLFist); drawJoint(pRFist);
        }
    }
};

#include "Bot.h"

void DrawButton(Rectangle rect, const char* text, bool& flag) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), rect);
    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) flag = !flag;
    DrawRectangleRec(rect, flag ? GREEN : LIGHTGRAY);
    DrawRectangleLinesEx(rect, 2, BLACK);
    DrawText(text, rect.x + 10, rect.y + 10, 20, BLACK);
}

void ProcessCombat(Character& attacker, Character& defender) {
    if (!attacker.isAttacking || attacker.hasDealtDamage || attacker.animator.isBlending) return;

    std::string anim = attacker.animator.currentAnim;
    
    if (!attacker.animator.animIsAttack[anim]) return;

    if (attacker.animator.currentFrame >= attacker.animator.animActiveFrame[anim]) {
        
        Rectangle hb = attacker.animator.animHitbox[anim];
        
        float hbX = attacker.pPelvis->position.x + (attacker.facingDir > 0 ? hb.x : -(hb.x + hb.width));
        float hbY = attacker.pPelvis->position.y + hb.y;
        Rectangle attackBox = { hbX, hbY, hb.width, hb.height };
        
        // HURTBOX DINÂMICA
        // Cobre do topo da cabeça (pHead - raio) até passar da cintura (pPelvis + margem)
        float defTopY = defender.pHead->position.y - 80.0f; 
        float defBottomY = defender.pPelvis->position.y + 100.0f;
        float defHeight = defBottomY - defTopY;
        
        // Caixa com 120px de largura, acompanhando perfeitamente a física do corpo
        Rectangle defendBox = { defender.pPelvis->position.x - 60.0f, defTopY, 120.0f, defHeight };

        if (CheckCollisionRecs(attackBox, defendBox)) {
            attacker.hasDealtDamage = true; 
            int hitType = attacker.animator.animHitType[anim];
            
            // Intercepta o golpe se estiver bloqueando de frente para o atacante
            if (defender.isBlocking && defender.facingDir != attacker.facingDir) {
                std::string blockAnim = "blockReactHigh"; 
                bool guardBreak = false;

                if (hitType == 1) blockAnim = "blockReactMedium";       
                else if (hitType == 2) blockAnim = "blockReactLow";  
                else if (hitType == 3) {
                    blockAnim = "blockReactStrong";
                    guardBreak = true; // Quebra a defesa deixando o boneco vulnerável
                }

                defender.TakeDamage(blockAnim, attacker.facingDir, guardBreak);
                return; // Encerra aqui para não aplicar a animação de dano real
            }
            
            // DANO REAL (Caso não esteja bloqueando)
            std::string hitAnim = "hit1"; 
            if (hitType == 1) hitAnim = "hit2";       
            else if (hitType == 2) hitAnim = "hit3";  
            else if (hitType == 3) hitAnim = "bighit";

            defender.TakeDamage(hitAnim, attacker.facingDir, false);
        }
    }
}

int main() {
    InitWindow(1600, 1000, "Galactic Shounen - Animator Sandbox");
    SetTargetFPS(60);

    Character player;
    player.Init({ 800.0f, 200.0f });

    Bot enemy;
    enemy.Init({ 1300.0f, 200.0f });

    const float groundY = 900.0f;

    // NOVO: Inicializa a Câmera
    Camera2D camera = { 0 };
    camera.target = { 800.0f, groundY - 350.0f }; // Foca no centro da arena
    camera.offset = { 1600.0f / 2.0f, 1000.0f / 2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;
    bool botActive = true;
    bool botAggressive = true;
    bool showHitboxes = false;

    bool typingFrame = false;
    std::string frameText = "";
    bool creatingHitbox = false;
    bool drawingHitbox = false;
    Vector2 dragStart = {0,0}, dragEnd = {0,0};

    while (!WindowShouldClose()) {
        
        // Controle de Zoom com o Scroll
        camera.zoom += GetMouseWheelMove() * 0.1f;
        if (camera.zoom < 0.2f) camera.zoom = 0.2f;
        if (camera.zoom > 3.0f) camera.zoom = 3.0f;

        // Panorâmica (arrastar a tela) segurando o botão DIREITO do mouse
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta();
            camera.target.x -= delta.x / camera.zoom;
            camera.target.y -= delta.y / camera.zoom;
        }

        Vector2 mouseScreen = GetMousePosition();
        Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, camera);
        
        if (IsKeyPressed(KEY_R) && player.currentMode != Character::RAGDOLL) {
            player.pPelvis->position = { 800.0f, 482.5f }; 
            player.ApplyFK();
            camera.target = { 800.0f, groundY - 350.0f }; // Reseta a câmera também
            camera.zoom = 1.0f;
        }
        
        // Passa as coordenadas calculadas para o Update
        player.Update(groundY, mouseWorld, mouseScreen);

        if (player.currentMode == Character::GAMEPLAY) {
            if (botActive) {
                enemy.UpdateAI(GetFrameTime(), groundY, player, botAggressive);
            }
            // Processa as trocas de socos
            ProcessCombat(player, enemy);
            if (botActive && botAggressive) ProcessCombat(enemy, player);
        }

        // Lógica de desenho de Hitbox no modo ANIMATOR
        Rectangle uiPanel = { 10, 10, 250, 620 }; // Aumente a altura do painel para caber os botões
        if (player.currentMode == Character::ANIMATOR && player.animator.editIsAttack && creatingHitbox) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mouseScreen, uiPanel)) {
                drawingHitbox = true;
                dragStart = mouseWorld;
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && drawingHitbox) dragEnd = mouseWorld;
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && drawingHitbox) {
                drawingHitbox = false;
                float minX = fmin(dragStart.x, dragEnd.x);
                float minY = fmin(dragStart.y, dragEnd.y);
                // Grava a hitbox baseada na distância para o quadril do boneco
                player.animator.editHitbox = { minX - player.pPelvis->position.x, minY - player.pPelvis->position.y, fabs(dragEnd.x - dragStart.x), fabs(dragEnd.y - dragStart.y) };
            }
        }

        BeginDrawing();
        ClearBackground(WHITE);

        // Inicia o modo 2D: Tudo aqui sofre zoom e pan
        BeginMode2D(camera);
            DrawRectangle(0, (int)groundY, 1600, 1000 - (int)groundY, DARKGRAY);
            player.Draw();
            if (player.currentMode == Character::ANIMATOR && player.animator.editIsAttack) {
                if (drawingHitbox) {
                    DrawRectangle(fmin(dragStart.x, dragEnd.x), fmin(dragStart.y, dragEnd.y), fabs(dragEnd.x - dragStart.x), fabs(dragEnd.y - dragStart.y), Fade(RED, 0.5f));
                } else if (player.animator.editHitbox.width > 0) {
                    // SÓ DESENHA A HITBOX SE ESTIVER NO FRAME CONFIGURADO
                    if ((int)player.animator.currentFrame == player.animator.editActiveFrame) {
                        DrawRectangle(player.pPelvis->position.x + player.animator.editHitbox.x, player.pPelvis->position.y + player.animator.editHitbox.y, player.animator.editHitbox.width, player.animator.editHitbox.height, Fade(RED, 0.5f));
                    }
                }
            }

            if (botActive) enemy.Draw();

            // Debug de Hitbox e Hurtbox no GAMEPLAY
            if (player.currentMode == Character::GAMEPLAY && showHitboxes) {
                auto DrawDebugBoxes = [](Character& c) {
                    // Hurtbox do Corpo (Azul) - A mesma usada para receber dano
                    float defTopY = c.pHead->position.y - 80.0f; 
                    float defBottomY = c.pPelvis->position.y + 100.0f;
                    DrawRectangleLinesEx({ c.pPelvis->position.x - 60.0f, defTopY, 120.0f, defBottomY - defTopY }, 2, BLUE);
                    
                    // Hitbox de Ataque (Vermelha) - Só renderiza se passou do active frame
                    if (c.isAttacking && c.animator.animIsAttack[c.animator.currentAnim]) {
                        if (c.animator.currentFrame >= c.animator.animActiveFrame[c.animator.currentAnim]) {
                            Rectangle hb = c.animator.animHitbox[c.animator.currentAnim];
                            float hbX = c.pPelvis->position.x + (c.facingDir > 0 ? hb.x : -(hb.x + hb.width));
                            float hbY = c.pPelvis->position.y + hb.y;
                            DrawRectangle(hbX, hbY, hb.width, hb.height, Fade(RED, 0.6f));
                        }
                    }
                };
                
                DrawDebugBoxes(player);
                if (botActive) DrawDebugBoxes(enemy);
            }

        EndMode2D();

        // UI Panel (Fica de fora da câmera para não sofrer zoom)
        DrawRectangleRec(uiPanel, Fade(LIGHTGRAY, 0.8f));
        DrawText("SISTEMA", 20, 20, 20, BLACK);

        // 1. Botões de Arquivo
        Rectangle btnSave = { 20, 50, 100, 30 };
        Rectangle btnLoad = { 125, 50, 95, 30 };
        
        if (CheckCollisionPointRec(GetMousePosition(), btnSave) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            std::string path = SaveFileDialog();
            if (!path.empty()) player.animator.SaveToFile(path);
        }
        if (CheckCollisionPointRec(GetMousePosition(), btnLoad) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            std::string path = OpenFileDialog();
            if (!path.empty()) {
                player.animator.LoadFromFile(path);
                player.showInitialFrame = true; 
            }
        }
        
        DrawRectangleRec(btnSave, GRAY); DrawRectangleLinesEx(btnSave, 2, BLACK); DrawText("SAVE", btnSave.x + 25, btnSave.y + 5, 20, WHITE);
        DrawRectangleRec(btnLoad, GRAY); DrawRectangleLinesEx(btnLoad, 2, BLACK); DrawText("LOAD", btnLoad.x + 25, btnLoad.y + 5, 20, WHITE);

        Rectangle btnSkin = { 20, 90, 200, 30 };
        if (CheckCollisionPointRec(GetMousePosition(), btnSkin) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            std::string path = OpenFolderDialog();
            if (!path.empty()) {
                bool success = player.LoadSkin(path);
                if (!success) TraceLog(LOG_ERROR, "Arquivos PNG obrigatorios nao encontrados na pasta.");
            }
        }
        DrawRectangleRec(btnSkin, GRAY); DrawRectangleLinesEx(btnSkin, 2, BLACK); DrawText("IMPORTAR SKIN", btnSkin.x + 20, btnSkin.y + 5, 20, WHITE);

        Rectangle btnMode = { 20, 130, 200, 40 }; 
        const char* modeText = player.currentMode == Character::ANIMATOR ? "Modo: ANIMATOR" : 
                              (player.currentMode == Character::RAGDOLL ? "Modo: RAGDOLL" : "Modo: GAMEPLAY");
        
        bool hoverMode = CheckCollisionPointRec(GetMousePosition(), btnMode);
        if (hoverMode && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            player.currentMode = (Character::Mode)((player.currentMode + 1) % 3);
            if (player.currentMode == Character::ANIMATOR || player.currentMode == Character::GAMEPLAY) {
                player.ApplyFK();
            }
        }
        DrawRectangleRec(btnMode, player.currentMode == Character::GAMEPLAY ? ORANGE : (player.currentMode == Character::RAGDOLL ? RED : LIGHTGRAY));
        DrawRectangleLinesEx(btnMode, 2, BLACK);
        DrawText(modeText, btnMode.x + 10, btnMode.y + 10, 20, BLACK);

        // A UI do Animador só aparece no modo ANIMATOR
        if (player.currentMode == Character::ANIMATOR) {
            DrawText("ANIMATOR", 20, 185, 20, BLACK);
            
            Rectangle btnFrame = { 20, 215, 200, 40 };
            if (CheckCollisionPointRec(GetMousePosition(), btnFrame) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                player.animator.AddPose(player.currentAngles, player.pPelvis->position);
            }
            DrawRectangleRec(btnFrame, DARKGRAY); DrawRectangleLinesEx(btnFrame, 2, BLACK);
            DrawText("Gravar Frame", btnFrame.x + 10, btnFrame.y + 10, 20, WHITE);

            Rectangle btnPlay = { 20, 265, 200, 40 };
            if (CheckCollisionPointRec(GetMousePosition(), btnPlay) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                player.animator.isPlaying = !player.animator.isPlaying;
            }
            DrawRectangleRec(btnPlay, player.animator.isPlaying ? GREEN : RED); DrawRectangleLinesEx(btnPlay, 2, BLACK);
            DrawText(player.animator.isPlaying ? "PAUSAR" : "PLAY LOOP", btnPlay.x + 10, btnPlay.y + 10, 20, BLACK);

            DrawText(TextFormat("Velocidade: %.1f", player.animator.playSpeed), 20, 320, 20, BLACK);
            Rectangle btnSpeedDown = { 170, 315, 20, 25 };
            Rectangle btnSpeedUp = { 200, 315, 20, 25 };
            if (CheckCollisionPointRec(GetMousePosition(), btnSpeedDown) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) player.animator.playSpeed -= 1.0f;
            if (CheckCollisionPointRec(GetMousePosition(), btnSpeedUp) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) player.animator.playSpeed += 1.0f;
            DrawRectangleRec(btnSpeedDown, GRAY); DrawText("-", 175, 318, 20, WHITE);
            DrawRectangleRec(btnSpeedUp, GRAY); DrawText("+", 203, 318, 20, WHITE);

            if (player.animator.isPlaying) {
                DrawText(TextFormat("Frame Atual: %d / %d", (int)player.animator.currentFrame + 1, (int)player.animator.track.size()), 20, 360, 15, BLACK);
            } else {
                DrawText(TextFormat("Frames Salvos: %d", (int)player.animator.track.size()), 20, 360, 15, BLACK);
            }

            // NOVOS BOTÕES DE METADADOS
            Rectangle btnAttackMode = { 20, 470, 200, 30 };
            DrawButton(btnAttackMode, player.animator.editIsAttack ? "GOLPE: ON" : "GOLPE: OFF", player.animator.editIsAttack);

            if (player.animator.editIsAttack) {
                Rectangle btnFrameInput = { 20, 510, 200, 30 };
                if (CheckCollisionPointRec(mouseScreen, btnFrameInput) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    typingFrame = true; frameText = "";
                } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) typingFrame = false;

                if (typingFrame) {
                    int key = GetCharPressed();
                    while (key > 0) {
                        if (key >= '0' && key <= '9') frameText += (char)key;
                        key = GetCharPressed();
                    }
                    if (IsKeyPressed(KEY_BACKSPACE) && frameText.length() > 0) frameText.pop_back();
                    if (IsKeyPressed(KEY_ENTER)) {
                        player.animator.editActiveFrame = frameText.empty() ? 0 : std::stoi(frameText);
                        typingFrame = false;
                    }
                }
                DrawRectangleRec(btnFrameInput, typingFrame ? LIGHTGRAY : GRAY);
                DrawText(TextFormat("Frame Ativ: %d", player.animator.editActiveFrame), btnFrameInput.x + 5, btnFrameInput.y + 5, 15, BLACK);
                if (typingFrame) DrawText(frameText.c_str(), btnFrameInput.x + 130, btnFrameInput.y + 5, 20, RED);

                Rectangle btnHitbox = { 20, 550, 200, 30 };
                if (CheckCollisionPointRec(mouseScreen, btnHitbox) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) creatingHitbox = !creatingHitbox;
                DrawRectangleRec(btnHitbox, creatingHitbox ? ORANGE : GRAY);
                DrawText(creatingHitbox ? "APLICAR HITBOX" : "CRIAR HITBOX", btnHitbox.x + 10, btnHitbox.y + 5, 20, BLACK);

                Rectangle btnHitType = { 20, 590, 200, 30 };
                if (CheckCollisionPointRec(mouseScreen, btnHitType) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    player.animator.editHitType = (player.animator.editHitType + 1) % 4; // Cicla 0, 1, 2, 3
                }
                const char* typeText = player.animator.editHitType == 0 ? "REACAO: ALTA" :
                                      (player.animator.editHitType == 1 ? "REACAO: MEDIA" :
                                      (player.animator.editHitType == 2 ? "REACAO: BAIXA" : "REACAO: FORTE"));
                DrawRectangleRec(btnHitType, PURPLE);
                DrawRectangleLinesEx(btnHitType, 2, BLACK);
                DrawText(typeText, btnHitType.x + 10, btnHitType.y + 5, 20, WHITE);
            }
            
            Rectangle btnInitial = { 20, 640, 200, 30 };
            DrawButton(btnInitial, "Ver Posicao Inicial", player.showInitialFrame);

            if (!player.animator.isPlaying && !player.animator.track.empty()) {
                Rectangle btnPrev = { 20, 680, 95, 30 };
                Rectangle btnNext = { 125, 680, 95, 30 };
                
                if (CheckCollisionPointRec(GetMousePosition(), btnPrev) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    player.animator.currentFrame -= 1.0f;
                    if (player.animator.currentFrame < 0) player.animator.currentFrame = player.animator.track.size() - 1;
                    
                    int idx = (int)player.animator.currentFrame;
                    for (int i = 0; i < 14; i++) player.currentAngles[i] = player.animator.track[idx].angles[i];
                    player.pPelvis->position = player.animator.track[idx].rootPos;
                    player.ApplyFK();
                }
                if (CheckCollisionPointRec(GetMousePosition(), btnNext) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    player.animator.currentFrame += 1.0f;
                    if (player.animator.currentFrame >= player.animator.track.size()) player.animator.currentFrame = 0;
                    
                    int idx = (int)player.animator.currentFrame;
                    for (int i = 0; i < 14; i++) player.currentAngles[i] = player.animator.track[idx].angles[i];
                    player.pPelvis->position = player.animator.track[idx].rootPos;
                    player.ApplyFK();
                }
                
                DrawRectangleRec(btnPrev, GRAY); DrawRectangleLinesEx(btnPrev, 2, BLACK); DrawText("<", btnPrev.x + 40, btnPrev.y + 5, 20, WHITE);
                DrawRectangleRec(btnNext, GRAY); DrawRectangleLinesEx(btnNext, 2, BLACK); DrawText(">", btnNext.x + 40, btnNext.y + 5, 20, WHITE);
            }
        }

        else if (player.currentMode == Character::GAMEPLAY) {
            DrawText("CONTROLES DO BOT", 20, 185, 20, BLACK);
            
            Rectangle btnBotState = { 20, 215, 200, 40 };
            DrawButton(btnBotState, botActive ? "Bot: LIGADO" : "Bot: DESLIGADO", botActive);
            
            if (botActive) {
                Rectangle btnBotAggro = { 20, 265, 200, 40 };
                DrawButton(btnBotAggro, botAggressive ? "Modo: ATACANDO" : "Modo: PARADO", botAggressive);
            }

            Rectangle btnHitboxes = { 20, 315, 200, 40 };
            DrawButton(btnHitboxes, showHitboxes ? "Hitboxes: ON" : "Hitboxes: OFF", showHitboxes);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}