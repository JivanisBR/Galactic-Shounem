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
    std::vector<std::string> comboList; // Guarda os 5 golpes equipados

    std::string comboFilePath = "animations\\playerCombo\\loadout.txt";
    
    void LoadCombo() {
        comboList.clear();
        std::ifstream file(comboFilePath);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) comboList.push_back(line);
        }
        file.close();
        
        // Aumentado para 8 slots! (0 a 4 = Combo, 5 = Tecla Q, 6 = W+Click, 7 = S+Click)
        while (comboList.size() < 6) comboList.push_back("jab"); 
        
        // Fallbacks exclusivos
        if (comboList.size() < 7) comboList.push_back("levantadynha");
        if (comboList.size() < 8) comboList.push_back("canelasso");
    }
    
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
    bool isDead = false;
    float maxHealth = 300.0f, health = 300.0f;
    float maxShield = 200.0f, shield = 200.0f;
    float pdl = 100.0f, atkBonus = 10.0f;
    bool shieldBroken = false;
    float parryWindow = 0.0f;

    struct PendingHit {
        bool active = false;
        std::string hitAnim;
        float attackerDir;
        float dmgAmount;
        bool isBlockHit;
        Vector2 deathDir;
        int deathTarget;
        int hitType;
        bool forceParryStun;
    } pendingHit;

    // Variáveis neurais de decisão da IA
    int consecutiveHitsTaken = 0;
    int hitsToBlockThreshold = 3; // Começa precisando de 3 acertos
    float blockTimer = 0.0f;
    int consecutiveBlocks = 0;
    int blocksToParryThreshold = 3; // Começa precisando de 3 defesas
    bool wantsToParry = false;

    float slideVelocity = 0.0f;
    bool isDowned = false; 
    bool isStandingUp = false;
    bool isParryStunned = false;
    bool isParrying = false;

    bool isStunRagdoll = false;
    float ragdollDamageMultiplier = 0.05f; // AJUSTE AQUI A LETALIDADE DAS PAREDES

    // animaçoes procedurais
    bool isExecuting = false;
    int executionType = 0; // 1 = Agachar, 2 = Puxada Magnética
    bool isRemoteGrab = false;
    Vector2 executionStartPos = {0, 0}; // Guarda a posição inicial do ragdoll
    float executionTimer = 0.0f;
    std::shared_ptr<Particle> executionTarget = nullptr;

    float magneticForce = 25.0f; // Força para puxar para a mão
    float tkControlSpeed = 60.0f; // Força para perseguir o mouse
    float tkElasticity = 500.0f; // Controla a tensão da mola (O valor fixo que usamos antes)

    // Controle do Arremesso
    bool isChargingThrow = false;
    float throwChargeTimer = 0.0f;

    // Controle de Força Contínua (Kamehameha)
    bool beamActive = false;
    float beamTimer = 0.0f;
    Vector2 beamDir = {0.0f, 0.0f};
    float beamForce = 0.0f;
    float beamThickness = 0.0f;
    bool isVisibleBeam = false;
    float beamBaseForce = 1500.0f;

    bool isKnockbackActive = false;
    float knockbackTimer = 0.0f;
    Vector2 knockbackDir = {0.0f, 0.0f};
    float knockbackForce = 0.0f;
    int knockbackTarget = 1;
    float targetOriginalMass = 0.0f;

    void HandleWallCollisions() {
        float wallLeft = -9900.0f;
        float wallRight = 9900.0f;
        bool hitWall = false;
        float impactVelocity = 0.0f;
        float wallDir = 0.0f;

        // NOVO: Mapeia as pontas do corpo (braço esticado, perna, cabeça, etc)
        float minX = 999999.0f;
        float maxX = -999999.0f;
        for (auto& p : world.particles) {
            float r = p->radius * 0.5f; // Metade do raio para permitir contato visual sem parecer de plástico
            if (p->position.x - r < minX) minX = p->position.x - r;
            if (p->position.x + r > maxX) maxX = p->position.x + r;
        }

        // Empurra o personagem baseado no osso mais esticado
        if (minX <= wallLeft) {
            pPelvis->position.x += (wallLeft - minX);
            ApplyFK(); // Atualiza toda a cadeia de ossos instantaneamente
            if (slideVelocity < 0.0f) { 
                hitWall = true;
                impactVelocity = fabs(slideVelocity);
                wallDir = 1.0f; 
            }
        }
        else if (maxX >= wallRight) {
            pPelvis->position.x -= (maxX - wallRight);
            ApplyFK();
            if (slideVelocity > 0.0f) {
                hitWall = true;
                impactVelocity = fabs(slideVelocity);
                wallDir = -1.0f; 
            }
        }

        // LEI DA AÇÃO E REAÇÃO: Só aciona o estrago se bater rápido o suficiente
        if (hitWall && impactVelocity > 500.0f) {
            float damage = impactVelocity * 0.015f; 
            health -= damage;
            
            if (health <= 0) {
                health = 0;
                isDead = true;
                
                isKnockbackActive = true;
                knockbackTimer = 0.1f; 
                knockbackDir = { wallDir, -0.8f }; 
                knockbackForce = impactVelocity * 0.04f; 
                if (knockbackForce > 1000.0f) knockbackForce = 1000.0f; 
            } else {
                isHit = true;
                isAttacking = false;
                isTransforming = false;
                isDowned = false;
                isParryStunned = false;
                comboStep = 0;
                inputBuffer = false;
                
                animator.Play("collisionReact", currentAngles);
                animator.playDirection = 1.0f;
                
                if (animator.animations.find("collisionReact") != animator.animations.end()) {
                    float totalFrames = (float)animator.animations["collisionReact"].size();
                    stunTimer = totalFrames / animator.playSpeed;
                } else {
                    stunTimer = 0.5f;
                }
                
                slideVelocity = impactVelocity * 0.3f * wallDir; 
            }
        } else if (hitWall) {
            slideVelocity = 0.0f; 
        }
    }

    void SolveArmIK(Vector2 target, int shoulderIdx, int elbowIdx) {
        float L1 = boneLengths[shoulderIdx];
        float L2 = boneLengths[elbowIdx];
        
        // Vetor do ombro até o alvo corrigido pela direção visual
        float dx = (target.x - pNeck->position.x) * facingDir; 
        float dy = target.y - pNeck->position.y;
        float dist = sqrt(dx*dx + dy*dy);
        
        // Trava para o braço não desconectar se o alvo estiver longe demais
        dist = fmin(dist, L1 + L2 - 0.1f); 
        
        // Lei dos Cossenos
        float angle1 = atan2(dy, dx);
        float cosAngle2 = (dist*dist + L1*L1 - L2*L2) / (2.0f * dist * L1);
        float angle2 = acos(fmax(-1.0f, fmin(1.0f, cosAngle2)));
        
        float cosAngle3 = (L1*L1 + L2*L2 - dist*dist) / (2.0f * L1 * L2);
        float angle3 = acos(fmax(-1.0f, fmin(1.0f, cosAngle3)));
        
        // Substitui os ângulos do FK atual pelos ângulos calculados procedimentalmente
        currentAngles[shoulderIdx] = angle1 - angle2; 
        currentAngles[elbowIdx] = currentAngles[shoulderIdx] + PI - angle3;
    }

    // 1. A nova TakeDamage apenas enfileira o impacto (Não cancela o ataque na hora!)
    void TakeDamage(std::string hitAnim, float attackerDir, float dmgAmount, bool isBlockHit = false, Vector2 deathDir = {0,0}, int deathTarget = 1, int hitType = 0, bool forceParryStun = false) {
        if (pendingHit.active) return; // Evita bugar se tomar 2 hits de fontes diferentes no exato mesmo frame
        pendingHit = { true, hitAnim, attackerDir, dmgAmount, isBlockHit, deathDir, deathTarget, hitType, forceParryStun };
    }

    // 2. O TakeDamage original inteiro agora se chama ResolveDamage
    void ResolveDamage() {
        if (!pendingHit.active) return;
        pendingHit.active = false;

        // Desempacota as variáveis guardadas no buffer
        std::string hitAnim = pendingHit.hitAnim;
        float attackerDir = pendingHit.attackerDir;
        float dmgAmount = pendingHit.dmgAmount;
        bool isBlockHit = pendingHit.isBlockHit;
        Vector2 deathDir = pendingHit.deathDir;
        int deathTarget = pendingHit.deathTarget;
        int hitType = pendingHit.hitType;

        isHit = true;
        isAttacking = false;     
        isTransforming = false;  
        comboStep = 0;
        inputBuffer = false;
        hasDealtDamage = false; 

        isDowned = false;     
        isStandingUp = false; 
        isParryStunned = false; 
        isParrying = false; 
        animator.playDirection = 1.0f;

        if (isBlockHit) {
            shield -= dmgAmount;
            if (shield <= 0) {
                shield = 0;
                shieldBroken = true;
                isBlocking = false;
                hitAnim = "blockReactStrong"; 
                
                // NOVO: Se o escudo quebrou por causa de um Parry do inimigo, fica atordoado!
                if (pendingHit.forceParryStun) isParryStunned = true;
            } 
            slideVelocity = dmgAmount * 8.0f * attackerDir;

            consecutiveBlocks++;
            consecutiveHitsTaken = 0;
            if (consecutiveBlocks >= blocksToParryThreshold) {
                wantsToParry = true;
                consecutiveBlocks = 0;
                blocksToParryThreshold = GetRandomValue(1, 4);
                blockTimer = 0.0f; 
            }
        } else {
            isBlocking = false;
            health -= dmgAmount;
            if (health <= 0) { 
                health = 0; 
                isDead = true; 
                
                if (dmgAmount > 20.0f) {
                    isKnockbackActive = true;
                    knockbackTimer = 0.2f;
                    knockbackDir = deathDir;
                    knockbackDir.x *= attackerDir; 
                    knockbackForce = dmgAmount * 10.0f; 
                    knockbackTarget = deathTarget;
                }
                else{
                    float forceMultiplier = dmgAmount * 10.0f; 
                    std::shared_ptr<Particle> tParticle = pPelvis; 
                    if (deathTarget == 0) tParticle = pHead;
                    else if (deathTarget == 2) tParticle = pRFoot; 
                    
                    tParticle->previousPosition.x = tParticle->position.x - (deathDir.x * attackerDir * forceMultiplier);
                    tParticle->previousPosition.y = tParticle->position.y - (deathDir.y * forceMultiplier);
                }
            }   
            if (!isDead) {
                consecutiveHitsTaken++;
                consecutiveBlocks = 0;
                wantsToParry = false; 
                if (consecutiveHitsTaken >= hitsToBlockThreshold) {
                    blockTimer = 2.0f; 
                    consecutiveHitsTaken = 0;
                    hitsToBlockThreshold = GetRandomValue(2, 6);
                }
                
                if (hitType == 3) {
                    slideVelocity = dmgAmount * 300.0f * attackerDir; 
                } else {
                    slideVelocity = dmgAmount * 4.0f * attackerDir; 
                }
            }
        }

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

    void SolveMouseIK(Vector2 mouseWorld) {
        float segLength = boneLengths[4]; // 80.0f
        
        // 1. Extrai a posição atual do cotovelo (o 'x' e 'y' do algoritmo original).
        // Convertido para o espaço local para a matemática não quebrar quando o boneco inverter de lado.
        float elbowX = (pRElbow->position.x - pNeck->position.x) * facingDir;
        float elbowY = pRElbow->position.y - pNeck->position.y;
        
        // 2. Converte o alvo (mouseX, mouseY do algoritmo original) para o espaço local.
        float targetX = (mouseWorld.x - pNeck->position.x) * facingDir;
        float targetY = mouseWorld.y - pNeck->position.y;
        
        // 3. Aplica a lógica iterativa de Reach
        float dx = targetX - elbowX;
        float dy = targetY - elbowY;
        float angle1 = atan2(dy, dx);  
        
        float tx = targetX - cos(angle1) * segLength;
        float ty = targetY - sin(angle1) * segLength;
        
        // O ombro é a base ('x2' e 'y2' do original), que no espaço local equivale ao ponto 0,0.
        dx = tx - 0.0f;
        dy = ty - 0.0f;
        float angle2 = atan2(dy, dx);  
        
        // 4. Aplica nas juntas correspondentes
        currentAngles[4] = angle2; // Ombro
        currentAngles[5] = angle1; // Cotovelo
    }
    
    void Control(float dt, float groundY, Vector2 mouseWorld) {
        if (shieldBroken && !isStunRagdoll) {
            shield += (maxShield / 5.0f) * dt; 
            if (shield >= maxShield) {
                shield = maxShield;
                shieldBroken = false;
            }
        }
        
        //Sequestro da Animação Procedimental
        if (isExecuting) {
            executionTimer += dt;
            
            // Trava pélvis no lugar
            velocityY = 0.0f;
            pPelvis->position.y = groundY - 422.5f;

            if (executionType == 1) {
                float dx = (executionTarget->position.x - pPelvis->position.x);
                facingDir = (dx > 0) ? 1.0f : -1.0f; 
                
                // REDUZIDO PARA 0.3s: O bote da mão para o chão será quase instantâneo
                float progress = fmin(executionTimer / 0.3f, 1.0f);
                
                if (progress < 1.0f) {
                    // FASE 1 (0s a 0.3s): Jogador curva o corpo e desce a mão guiada pelo IK
                    float targetNeckAngle = atan2(executionTarget->position.y - pPelvis->position.y, dx * facingDir);
                    currentAngles[0] += (targetNeckAngle - currentAngles[0]) * dt * 15.0f; // Curva o corpo
                    
                    Vector2 startHand = { pNeck->position.x + 40.0f * facingDir, pNeck->position.y + 60.0f };
                    Vector2 currentHandTarget = {
                        startHand.x + (executionTarget->position.x - startHand.x) * progress,
                        startHand.y + (executionTarget->position.y - startHand.y) * progress
                    };
                    
                    // Roda o cálculo IK para buscar o pescoço SÓ NA DESCIDA
                    SolveArmIK(currentHandTarget, 4, 5); 
                } else {
                    // Ancoragem Física de transição: Garante a captura no exato frame
                    executionTarget->position = pRHand->position;
                    executionTarget->previousPosition = pRHand->position;
                }
                
                // [NOVO TEMPO] Instantaneamente ao bater 0.35s, libera pro mouse! (Antes era 1.5s)
                if (executionTimer > 0.35f) {
                    executionType = 2; // Entra na física do mouse
                    executionTimer = 0.0f;
                }
                return; // Bloqueia inputs, ataques e movimentos normais
            }

            else if (executionType == 2) {
                // TIPO 2: TELECINESE E ARREMESSO (Controlada pelo Mouse)
                facingDir = (mouseWorld.x > pPelvis->position.x) ? 1.0f : -1.0f;
                
                // CORREÇÃO DA POSTURA: Ergue as costas suavemente de volta para a posição ereta (-90 graus) enquanto a mão busca o mouse
                currentAngles[0] += (-1.57f - currentAngles[0]) * dt * 12.0f; 
                
                SolveMouseIK(mouseWorld);
                
                // 1. Processa o Input de Arremesso
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    isChargingThrow = true;
                    throwChargeTimer += dt;
                    if (throwChargeTimer > 3.0f) throwChargeTimer = 3.0f;
                } 
                else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && isChargingThrow) {
                    float chargeRatio = throwChargeTimer / 3.0f;
                    
                    float dirX = mouseWorld.x - pNeck->position.x;
                    float dirY = mouseWorld.y - pNeck->position.y;
                    float length = sqrt(dirX*dirX + dirY*dirY);
                    if (length > 0.01f) { dirX /= length; dirY /= length; }
                    
                    beamActive = true;
                    isVisibleBeam = false;
                    beamTimer = 1.0f; 
                    beamDir = {dirX, dirY};
                    beamForce = 500.0f * chargeRatio; 
                    
                    // NOVO: Libera a massa apenas se tiver alvo preso!
                    if (executionTarget != nullptr) executionTarget->mass = targetOriginalMass; 
                    
                    isExecuting = false;
                    executionType = 0;
                    isChargingThrow = false;
                    throwChargeTimer = 0.0f;
                    animator.ForceBlend("idle", currentAngles);
                    return;
                }
                else {
                    isChargingThrow = false;
                    throwChargeTimer = 0.0f;
                }

                Vector2 currentTarget;
                if (isChargingThrow || !isRemoteGrab) currentTarget = pRHand->position; 
                else currentTarget = mouseWorld;

                // NOVO: Apenas processa a mola física se estiver segurando o pescoço de alguém
                if (executionTarget != nullptr) {
                    float dx = currentTarget.x - executionTarget->position.x;
                    float dy = currentTarget.y - executionTarget->position.y;
                    float dist = sqrt(dx*dx + dy*dy);
                    
                    if (!isChargingThrow && isRemoteGrab) {
                        if (dist > 0.1f) {
                            float springForce = dist * (tkControlSpeed / tkElasticity); 
                            if (springForce > 100.0f) springForce = 100.0f; 
                            if (springForce > dist) springForce = dist;
                            
                            executionTarget->position.x += (dx / dist) * springForce;
                            executionTarget->position.y += (dy / dist) * springForce;
                            
                            executionTarget->previousPosition.x += (executionTarget->position.x - executionTarget->previousPosition.x) * 0.02f;
                            executionTarget->previousPosition.y += (executionTarget->position.y - executionTarget->previousPosition.y) * 0.02f;
                        }
                    } else {
                        if (dist > 40.0f) {
                            float pullSpeed = log10(dist + 1.0f) * magneticForce;
                            if (pullSpeed > dist) pullSpeed = dist;
                            executionTarget->position.x += (dx / dist) * pullSpeed;
                            executionTarget->position.y += (dy / dist) * pullSpeed;
                            executionTarget->previousPosition = executionTarget->position; 
                        } else {
                            executionTarget->position = currentTarget;
                            executionTarget->previousPosition = currentTarget; 
                        }
                    }
                }
                return;
            }

            else if (executionType == 3) {
                // TIPO 3: MIRA DO KAMEHAMEHA
                facingDir = (mouseWorld.x > pPelvis->position.x) ? 1.0f : -1.0f;
                
                animator.UpdateGameplay(dt, currentAngles);
                
                // LÊ A ALTURA: Puxa o Root Motion Y da animação para a base descer
                float animOffsetY = 0.0f;
                if (animator.animations.find(animator.currentAnim) != animator.animations.end()) {
                    auto& track = animator.animations[animator.currentAnim];
                    int idx1 = (int)animator.currentFrame;
                    int idx2 = (idx1 + 1) % track.size();
                    float blend = animator.currentFrame - idx1;
                    float currentAnimY = track[idx1].rootPos.y + (track[idx2].rootPos.y - track[idx1].rootPos.y) * blend;
                    animOffsetY = currentAnimY - 482.5f; 
                }
                pPelvis->position.y = (groundY - 422.5f) + animOffsetY;

                SolveMouseIK(mouseWorld);
                
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    isChargingThrow = true;
                    throwChargeTimer += dt;
                    if (throwChargeTimer > 3.0f) throwChargeTimer = 3.0f;
                } 
                else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && isChargingThrow) {
                    float chargeRatio = throwChargeTimer / 3.0f;
                    if (chargeRatio < 0.2f) chargeRatio = 0.2f; 
                    
                    // CORREÇÃO: Pega o vetor exato partindo da MÃO para o mouse
                    float dirX = mouseWorld.x - pRHand->position.x;
                    float dirY = mouseWorld.y - pRHand->position.y;
                    float length = sqrt(dirX*dirX + dirY*dirY);
                    if (length > 0.01f) { dirX /= length; dirY /= length; }
                    
                    beamActive = true;
                    isVisibleBeam = true; 
                    beamTimer = 1.0f; 
                    beamDir = {dirX, dirY};
                    beamForce = beamBaseForce * chargeRatio; 
                    beamThickness = 250.0f * chargeRatio; 
                    
                    isChargingThrow = false;
                    throwChargeTimer = 0.0f;
                    executionType = 4; // Trava o braço!
                    return;
                }
                
                if (IsKeyPressed(KEY_E) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    isExecuting = false;
                    executionType = 0;
                    isChargingThrow = false;
                    throwChargeTimer = 0.0f;
                    animator.ForceBlend("idle", currentAngles);
                }
                return;
            }
            
            else if (executionType == 4) {
                // TIPO 4: DISPARO DO KAMEHAMEHA (Trava o braço no raio)
                facingDir = (beamDir.x > 0) ? 1.0f : -1.0f;
                
                animator.UpdateGameplay(dt, currentAngles);
                
                // LÊ A ALTURA: Puxa o Root Motion Y da animação para a base descer
                float animOffsetY = 0.0f;
                if (animator.animations.find(animator.currentAnim) != animator.animations.end()) {
                    auto& track = animator.animations[animator.currentAnim];
                    int idx1 = (int)animator.currentFrame;
                    int idx2 = (idx1 + 1) % track.size();
                    float blend = animator.currentFrame - idx1;
                    float currentAnimY = track[idx1].rootPos.y + (track[idx2].rootPos.y - track[idx1].rootPos.y) * blend;
                    animOffsetY = currentAnimY - 482.5f; 
                }
                pPelvis->position.y = (groundY - 422.5f) + animOffsetY;

                Vector2 fireTarget = { pNeck->position.x + beamDir.x * 1000.0f, pNeck->position.y + beamDir.y * 1000.0f };
                SolveMouseIK(fireTarget);
                
                // O temporizador é descontado no Game Loop, solta o braço quando acaba
                if (beamTimer <= 0.0f) {
                    isExecuting = false;
                    executionType = 0;
                    animator.ForceBlend("idle", currentAngles);
                }
                return;
            }

            else if (executionType == 5) { // ANIMAÇÃO CONJUNTA
                animator.UpdateGameplay(dt, currentAngles); // O Player roda a FK base dele
                
                float targetY = groundY - 422.5f;
                float targetX = executionStartPos.x;
                
                if (animator.animations.find(animator.currentAnim) != animator.animations.end()) {
                    auto& track = animator.animations[animator.currentAnim];
                    int idx1 = (int)animator.currentFrame;
                    int idx2 = (idx1 + 1) % track.size();
                    float blend = animator.currentFrame - idx1;
                    
                    float currentAnimY = track[idx1].rootPos.y + (track[idx2].rootPos.y - track[idx1].rootPos.y) * blend;
                    targetY = (groundY - 422.5f) + (currentAnimY - 482.5f); 
                    
                    // Root motion X a partir do ponto de captura
                    float currentAnimX = track[idx1].rootPos.x + (track[idx2].rootPos.x - track[idx1].rootPos.x) * blend;
                    float firstX = track[0].rootPos.x;
                    targetX = executionStartPos.x + (currentAnimX - firstX) * facingDir;
                }

                if (animator.isBlending) {
                    float t = animator.blendTimer / animator.BLEND_DURATION;
                    pPelvis->position.x = executionStartPos.x + (targetX - executionStartPos.x) * t;
                    pPelvis->position.y = executionStartPos.y + (targetY - executionStartPos.y) * t;
                } else {
                    pPelvis->position.x = targetX;
                    pPelvis->position.y = targetY;
                }

                // REMOVIDO: O encerramento automático foi transferido para a main() para não "atropelar" a Hitbox!
                return;
            }
        }
        
        if (isHit) {
            stunTimer -= dt;
            pPelvis->position.x += slideVelocity * dt;
            slideVelocity -= slideVelocity * 3.0f * dt;
            
            if (stunTimer <= 0.0f) {
                isHit = false; 
                if (shieldBroken && (animator.currentAnim == "strong" || animator.currentAnim == "strong2")) {
                    isDowned = true;
                }
            }
        }
        else if (isParryStunned) {
            if (animator.currentAnim != "stun") {
                animator.Play("stun", currentAngles);
                animator.playDirection = 1.0f;
            } else {
                // Loop Boomerang (Balança organicamente de um lado pro outro)
                float maxFrame = animator.animations["stun"].size() - 1.01f;
                if (animator.currentFrame >= maxFrame && animator.playDirection > 0.0f) animator.playDirection = -1.0f;
                else if (animator.currentFrame <= 0.01f && animator.playDirection < 0.0f) animator.playDirection = 1.0f;
            }
            
            // Fica vulnerável e rodando a animação de stun até o vigor carregar 100%
            if (!shieldBroken) {
                isParryStunned = false;
            }
        }
        else if (isDowned) {
            // FASE 1: Boneco no chão (Loop Boomerang)
            if (animator.currentAnim != "deitado") {
                animator.Play("deitado", currentAngles);
                animator.playDirection = 1.0f;
            } else {
                float maxFrame = animator.animations["deitado"].size() - 1.01f;
                // Bate e volta
                if (animator.currentFrame >= maxFrame && animator.playDirection > 0.0f) animator.playDirection = -1.0f;
                else if (animator.currentFrame <= 0.01f && animator.playDirection < 0.0f) animator.playDirection = 1.0f;
            }
            
            // Quando a guarda recarrega lá em cima, shieldBroken vira false
            if (!shieldBroken) {
                isDowned = false;
                isStandingUp = true;
                animator.Play("standup", currentAngles);
                animator.playDirection = 1.0f; // Tira o ping-pong
            }
        }
        else if (isStandingUp) {
            // FASE 2: Levantando (Animação Standup)
            if (animator.animFinished) {
                isStandingUp = false; // Acabou, deixa a Engine voltar pro Idle abaixo
            }
        }

        else if (isParrying) {
            if (animator.currentAnim != "parry") {
                animator.Play("parry", currentAngles);
                animator.playDirection = 1.0f;
            } else {
                // Garante que o frame final não quebre caso a animação seja curta
                float maxFrame = fmax(0.0f, (float)animator.animations["parry"].size() - 1.01f);
                
                // BOOMERANG: Bate no final e inverte pra tocar de trás pra frente
                if (animator.currentFrame >= maxFrame && animator.playDirection > 0.0f) {
                    animator.playDirection = -1.0f;
                } 
                // Bate no início voltando, acaba o estado e volta pro Idle
                else if (animator.currentFrame <= 0.01f && animator.playDirection < 0.0f) {
                    isParrying = false;
                    animator.playDirection = 1.0f;
                }
            }
        }

        else {
            bool isMoving = false;
            bool isRunning = IsKeyDown(KEY_LEFT_SHIFT);
            float speed = isRunning ? 3000.0f : 1000.0f;

            isBlocking = (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !shieldBroken && !isTransforming && !isTransformed);

            // Cancelamento de Animação (Block Cancel)
            // Se o jogador puxar a defesa no meio de um soco, a engine aborta o ataque na hora!
            if (isBlocking && isAttacking) {
                isAttacking = false;
                comboStep = 0;
                inputBuffer = false;
                hasDealtDamage = false;
            }

            // Se acabou de apertar o botão de defesa, ganha 0.5s de janela de Parry

            // Se acabou de apertar o botão de defesa, ganha 0.5s de janela de Parry
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && isBlocking) {
                parryWindow = 0.1f; // 100 milissegundos (ajuste)
            }
            if (parryWindow > 0.0f) parryWindow -= dt;

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

            // Input do Poderzão (Kamehameha - Tecla E)
            if (IsKeyPressed(KEY_E) && !isExecuting && !isTransformed && !isTransforming && !isBlocking && !isAttacking) {
                isExecuting = true;
                executionType = 3; // Estado exclusivo para Mira de Poder
                isChargingThrow = false;
                throwChargeTimer = 0.0f;
                animator.ForceBlend("power1", currentAngles); // Impõe a base do poderzão
                return; // CORREÇÃO: Sai da função agora para o "idle" não sobrescrever a pose!
            }

            // Input da Tecla Q (Golpe Único Extra)
            if (IsKeyPressed(KEY_Q) && !isExecuting && !isTransformed && !isTransforming && !isBlocking && !isAttacking) {
                isAttacking = true;
                comboStep = 96; // ID exclusivo do Golpe Q
                hasDealtDamage = false;
                inputBuffer = false;
                animator.Play(comboList[5], currentAngles); // Toca o 6º slot do loadout
                if (!animator.animations[comboList[5]].empty()) lastAnimX = animator.animations[comboList[5]][0].rootPos.x;
            }

            // 3. Input de Ataque (com Input Buffer Rigoroso e Golpes Especiais)
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !isTransformed && !isTransforming && !isBlocking) {
                if (!isAttacking) {
                    isAttacking = true;
                    hasDealtDamage = false; 
                    inputBuffer = false;
                    
                    if (IsKeyDown(KEY_LEFT_SHIFT)) {
                        if (IsKeyDown(KEY_S)) {
                            // RASTEIRA/CARRINHO (Shift + S + Click)
                            comboStep = 97;
                            animator.Play("carrinho", currentAngles);
                            if (!animator.animations["carrinho"].empty()) lastAnimX = animator.animations["carrinho"][0].rootPos.x;
                        } else {
                            // GOLPE CORRENDO NORMAL (Shift + Click)
                            comboStep = 98;
                            animator.Play("runningAttack1", currentAngles);
                            if (!animator.animations["runningAttack1"].empty()) lastAnimX = animator.animations["runningAttack1"][0].rootPos.x;
                        }
                    }
                    else if (IsKeyDown(KEY_S)) {
                        // GOLPE BAIXO EXTRA (S + Click) - Slot 7 (Índice 7)
                        comboStep = 99; 
                        animator.Play(comboList[7], currentAngles);
                        if (!animator.animations[comboList[7]].empty()) lastAnimX = animator.animations[comboList[7]][0].rootPos.x;
                    } 
                    else if (IsKeyDown(KEY_W)) {
                        // GOLPE ALTO (W + Click) - Slot 6 (Índice 6)
                        comboStep = 95; 
                        animator.Play(comboList[6], currentAngles);
                        if (!animator.animations[comboList[6]].empty()) lastAnimX = animator.animations[comboList[6]][0].rootPos.x;
                    }
                    else {
                        // COMBO NORMAL (Click)
                        comboStep = 1;
                        animator.Play(comboList[0], currentAngles);
                        if (!animator.animations[comboList[0]].empty()) lastAnimX = animator.animations[comboList[0]][0].rootPos.x;
                    }
                } 
                else if (comboStep >= 1 && comboStep < 5) { 
                    // BUFFER 
                    // Aceita o buffer a partir da METADE do tempo de preparo do golpe (startup).
                    // Isso permite a "spamadinha leve e ritmada" de forma muito orgânica, 
                    // mas continua ignorando aquele spam de clique no primeiro milissegundo.
                    float activeFrame = animator.animActiveFrame[animator.currentAnim];
                    if (animator.currentFrame >= (activeFrame * 0.5f)) {
                        inputBuffer = true;
                    }
                }
            }

            // 4. Movimento Lateral (Liberado durante o runningAttack e carrinho)
            if ((!isAttacking || comboStep == 98 || comboStep == 97) && !isTransforming && !isBlocking) {
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
                // Apenas transita o combo base (1 a 4). Golpes especiais param no frame.
                if (inputBuffer && comboStep >= 1 && comboStep < 5) {
                    comboStep++;
                    hasDealtDamage = false; 
                    std::string nextAttack = comboList[comboStep - 1];
                    animator.Play(nextAttack, currentAngles);
                    if (!animator.animations[nextAttack].empty()) {
                        lastAnimX = animator.animations[nextAttack][0].rootPos.x; 
                    }
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

        // O find() verifica se o nome da animação atual contém a palavra "attack"
        // 3. Aplica o Root Motion X exclusivo para o Ataque (Sem depender da palavra "attack")
        if (isAttacking && comboStep != 98 && comboStep != 97 && animator.animType[animator.currentAnim] > 0 && !animator.isBlending) {
            auto& track = animator.animations[animator.currentAnim];
            if (!track.empty()) {
                int idx1 = (int)animator.currentFrame;
                int idx2 = (idx1 + 1) % track.size();
                float blend = animator.currentFrame - idx1;
                
                float currentAnimX = track[idx1].rootPos.x + (track[idx2].rootPos.x - track[idx1].rootPos.x) * blend;
                float deltaX = (currentAnimX - lastAnimX) * facingDir; 
                
                pPelvis->position.x += deltaX; 
                lastAnimX = currentAnimX;      
            }
        }

        // 4. Aplica o movimento vertical da animação direto no corpo
        pPelvis->position.y = basePelvisY + animOffsetY;
        velocityY = 0.0f;

        //paredes
        // Delega os limites para o sistema de física orgânica
        HandleWallCollisions();
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


    void DrawStatusBars() {
        if (currentMode != GAMEPLAY || (isDead && !isStunRagdoll)) return;
        float barW = 80.0f;
        float barH = 6.0f;
        float posX = pHead->position.x - barW / 2.0f;
        float posY = pHead->position.y - 45.0f;

        // Escudo (Azul)
        DrawRectangle(posX, posY, barW, barH, Fade(DARKGRAY, 0.7f));
        DrawRectangle(posX, posY, barW * (fmax(0, shield) / maxShield), barH, BLUE);
        DrawRectangleLines(posX, posY, barW, barH, BLACK);

        // Vida (Verde)
        posY += 8.0f;
        DrawRectangle(posX, posY, barW, barH, Fade(DARKGRAY, 0.7f));
        DrawRectangle(posX, posY, barW * (fmax(0, health) / maxHealth), barH, GREEN);
        DrawRectangleLines(posX, posY, barW, barH, BLACK);
    }

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

        // Parâmetros do construtor: (posição, raio, massa, isPinned, grupo)
        
        // Membros pesados (O centro de gravidade e inércia)
        pNeck = std::make_shared<Particle>((Vector2){startPos.x, startPos.y + 20.0f}, 60.0f, 20.0f);
        pHead = std::make_shared<Particle>((Vector2){startPos.x, startPos.y - 160.0f}, 60.0f, 16.0f);
        pPelvis = std::make_shared<Particle>((Vector2){startPos.x, startPos.y + 282.5f}, 60.0f, 50.0f); // O mais pesado
        
        // Braços (Leves)
        pLElbow = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 140.0f}, 30.0f, 8.0f, false, GROUP_ARM);
        pLHand = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 260.0f}, 30.0f, 4.0f, false, GROUP_ARM);
        pRElbow = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 140.0f}, 30.0f, 8.0f, false, GROUP_ARM);
        pRHand = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 260.0f}, 30.0f, 4.0f, false, GROUP_ARM);
        pLFist = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 350.0f}, 30.0f, 6.0f, false, GROUP_ARM);
        pRFist = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 350.0f}, 30.0f, 6.0f, false, GROUP_ARM);

        // Pernas (Médias)
        pLKnee = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 482.5f}, 35.0f, 24.0f, false, GROUP_LEG);
        pLFoot = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 682.5f}, 22.5f, 16.0f, false, GROUP_LEG);
        pRKnee = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 482.5f}, 35.0f, 22.0f, false, GROUP_LEG);
        pRFoot = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 682.5f}, 22.5f, 16.0f, false, GROUP_LEG);
        pLToe = std::make_shared<Particle>((Vector2){startPos.x + 140.0f, startPos.y + 682.5f}, 22.5f, 4.0f, false, GROUP_LEG);
        pRToe = std::make_shared<Particle>((Vector2){startPos.x + 140.0f, startPos.y + 682.5f}, 22.5f, 4.0f, false, GROUP_LEG);

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
        // Cria as pastas se não existirem
        std::filesystem::create_directories("animations\\attacks");
        std::filesystem::create_directories("animations\\playerCombo");
        std::filesystem::create_directories("animations\\botCombo");
        std::filesystem::create_directories("animations\\reactions");

        // Carrega TODOS os ataques da pasta de uma vez para a memória
        for (const auto& entry : std::filesystem::directory_iterator("animations\\attacks")) {
            if (entry.path().extension() == ".txt") {
                animator.LoadToLibrary(entry.path().string(), entry.path().stem().string(), false);
            }
        }
        LoadCombo(); // Lê o loadout.txt

        // CARREGA TODAS AS REAÇÕES E GOLPES CUSTOMIZADOS
        for (const auto& entry : std::filesystem::directory_iterator("animations\\reactions")) {
            if (entry.path().extension() == ".txt") {
                animator.LoadToLibrary(entry.path().string(), entry.path().stem().string(), false);
            }
        }

        // BLOCKS
        animator.LoadToLibrary("animations\\block.txt", "block", false);
        animator.LoadToLibrary("animations\\blockReactHigh.txt", "blockReactHigh", false);
        animator.LoadToLibrary("animations\\blockReactMedium.txt", "blockReactMedium", false);
        animator.LoadToLibrary("animations\\blockReactLow.txt", "blockReactLow", false);
        animator.LoadToLibrary("animations\\blockReactStrong.txt", "blockReactStrong", false);

      

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

    void Update(float dt, float groundY, Vector2 mouseWorld, Vector2 mouseScreen) {
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

            world.Update(dt, -10000.0f, 10000.0f, -10000.0f, groundY);
        } 
        else if (currentMode == ANIMATOR) {
            animator.Update(GetFrameTime(), currentAngles, pPelvis->position);

            if (!animator.isPlaying) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mouseScreen, uiPanel)) {
                    // Mapeia os ossos do Player (0 a 13)
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
                        draggedJointIndex = -2; // Pélvis do Player
                    } else {
                        bool found = false;
                        for (auto& t : targets) {
                            if (CheckCollisionPointCircle(mouseWorld, t.p->position, 25.0f)) {
                                activeDragTarget = t;
                                draggedJointIndex = t.angleIdx;
                                found = true;
                                break;
                            }
                        }
                        
                        //  Mapeia a Pélvis e Ossos do Bot se não achou nada no Player
                        // E só permite se a animação for do tipo Conjunta!
                        if (!found && animator.editAnimType == 2) {
                            // Gambiarra necessária para achar as posições do Bot, 
                            // já que estamos dentro do escopo do Character (Player).
                            // Como o Bot atualiza a posição e os currentAngles na tela, 
                            // nós testamos contra o mouse direto na Main.
                            // Mas para o arrasto funcionar com IK 1 bone por vez:
                            draggedJointIndex = -99; // Usaremos -99 como Flag para avisar a main
                        }
                    }
                }
                
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    if (draggedJointIndex == -2) {
                        pPelvis->position = mouseWorld;
                    } else if (draggedJointIndex >= 0 && draggedJointIndex < 14) {
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
            if (isDead) {
                // KNOCKBACK DO GOLPE FINAL
                if (isKnockbackActive) {
                    knockbackTimer -= dt; // 2. Trocou
                    if (knockbackTimer <= 0.0f) isKnockbackActive = false;
                    
                    std::shared_ptr<Particle> tParticle = pPelvis;
                    if (knockbackTarget == 0) tParticle = pHead;
                    else if (knockbackTarget == 2) tParticle = pRFoot;
                    
                    for (auto& p : world.particles) {
                        float force = (p == tParticle) ? (knockbackForce * 2.0f) : knockbackForce;
                        p->position.x += knockbackDir.x * force * dt; // 3. Trocou
                        p->position.y += knockbackDir.y * force * dt; // 4. Trocou
                    }
                }
                world.Update(dt, -10000.0f, 10000.0f, -10000.0f, groundY);
            } else {
                // Se estiver vivo e consciente, roda os controles normais
                Control(dt, groundY, mouseWorld); // 5. Trocou
                if (!isExecuting) {
                    animator.UpdateGameplay(dt, currentAngles); // 6. Trocou
                }
                ApplyFK();
            }
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

class ComboMenu {
public:
    bool isOpen = false;
    int editingSlot = -1; 
    std::vector<std::string> availableAttacks;
    std::string previewAnim = "";
    bool editingBot = false; // Flag para alternar entre Player e Bot
    
    int scrollOffset = 0; // Guarda a posição da lista
    const int MAX_VISIBLE_ITEMS = 12; // Quantos itens cabem na janela

    void Open(Character& player, Character& bot) { 
        isOpen = true;
        editingSlot = -1;
        editingBot = false; // Sempre abre focado no jogador
        previewAnim = player.comboList[0];
        scrollOffset = 0; // Reseta a lista pro topo toda vez que abrir o menu
        
        availableAttacks.clear();
        for (const auto& entry : std::filesystem::directory_iterator("animations\\attacks")) {
            if (entry.path().extension() == ".txt") {
                std::string animName = entry.path().stem().string();
                if (player.animator.animType[animName] != 2) { // Oculta as conjuntas da roleta!
                    availableAttacks.push_back(animName);
                }
            }
        }
    }

    void UpdateAndDraw(Character& player, Character& bot) { 
        DrawRectangle(0, 0, 1600, 1000, Fade(BLACK, 0.7f)); 
        DrawText("CREATE YOUR COMBO", 50, 50, 40, WHITE);

        // NOVO: Descobre quem é o alvo das edições atuais
        Character& activeChar = editingBot ? bot : player; 

        // NOVO: Botão de Alternância
        Rectangle btnToggle = { 50.0f, 100.0f, 300.0f, 40.0f };
        bool hoverToggle = CheckCollisionPointRec(GetMousePosition(), btnToggle);
        if (hoverToggle && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            editingBot = !editingBot;
            editingSlot = -1; // Fecha a lista da direita ao trocar
            previewAnim = activeChar.comboList[0]; 
        }
        DrawRectangleRec(btnToggle, editingBot ? RED : BLUE);
        DrawRectangleLinesEx(btnToggle, 2, BLACK);
        DrawText(editingBot ? "EDITANDO: BOT" : "EDITANDO: JOGADOR", btnToggle.x + 10, btnToggle.y + 10, 20, WHITE);

        // O preview de animação continua renderizando visualmente no player
        if (player.animator.currentAnim != previewAnim) {
            player.animator.Play(previewAnim, player.currentAngles);
        }
        if (player.animator.animFinished) {
            player.animator.currentFrame = 0.0f; 
            player.animator.animFinished = false;
        }

        Vector2 mouse = GetMousePosition();

        // 1. Coluna de Slots Equipados
        for (int i = 0; i < 8; i++) { // Alterado para 8 slots
            // Reduzimos o espaçamento pra 55 e altura pra 45 pra caber tudo bonito
            Rectangle slotRec = { 50.0f, 150.0f + (i * 55.0f), 300.0f, 45.0f };
            
            // Dá um respiro de 15 pixels para separar a sessão do Combo da sessão de "Especiais"
            if (i >= 5) slotRec.y += 15.0f; 

            bool hover = CheckCollisionPointRec(mouse, slotRec);
            
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                editingSlot = i;
                previewAnim = activeChar.comboList[i]; 
            }

            DrawRectangleRec(slotRec, editingSlot == i ? ORANGE : (hover ? LIGHTGRAY : GRAY));
            DrawRectangleLinesEx(slotRec, 2, BLACK);
            
            // Estilização colorida pra você bater o olho e saber qual é qual
            if (i < 5) {
                DrawText(TextFormat("Hit %d: %s", i + 1, activeChar.comboList[i].c_str()), slotRec.x + 10, slotRec.y + 12, 20, BLACK);
            } else if (i == 5) {
                DrawText(TextFormat("Tecla Q: %s", activeChar.comboList[i].c_str()), slotRec.x + 10, slotRec.y + 12, 20, MAROON);
            } else if (i == 6) {
                DrawText(TextFormat("W+Click: %s", activeChar.comboList[i].c_str()), slotRec.x + 10, slotRec.y + 12, 20, DARKBLUE);
            } else if (i == 7) {
                DrawText(TextFormat("S+Click: %s", activeChar.comboList[i].c_str()), slotRec.x + 10, slotRec.y + 12, 20, DARKGREEN);
            }
        }

        // 2. Coluna de Todos os Ataques (Com Paginação Interativa)
        if (editingSlot != -1) {
            DrawRectangle(400, 150, 350, 600, Fade(DARKGRAY, 0.9f));
            DrawText("SELECIONE O GOLPE:", 420, 170, 20, WHITE);

            // BÔNUS: Lê a "rodinha" do mouse se o ponteiro estiver em cima da lista
            Rectangle listBounds = { 400.0f, 150.0f, 350.0f, 600.0f };
            if (CheckCollisionPointRec(mouse, listBounds)) {
                scrollOffset -= (int)GetMouseWheelMove();
            }

            // Trava matemática de segurança: Impede o scroll de passar do topo ou do fim
            int maxScroll = (int)availableAttacks.size() - MAX_VISIBLE_ITEMS;
            if (maxScroll < 0) maxScroll = 0;
            if (scrollOffset < 0) scrollOffset = 0;
            if (scrollOffset > maxScroll) scrollOffset = maxScroll;

            // BOTAO DE SUBIR (Aparece só se tiver rolado para baixo)
            Rectangle btnUp = { 420.0f, 200.0f, 310.0f, 25.0f };
            if (scrollOffset > 0) {
                bool hoverUp = CheckCollisionPointRec(mouse, btnUp);
                if (hoverUp && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) scrollOffset--;
                DrawRectangleRec(btnUp, hoverUp ? LIGHTGRAY : GRAY);
                
                // Raylib desenha triângulos no sentido anti-horário (Ponta Topo -> Base Esq -> Base Dir)
                DrawTriangle({btnUp.x + 155.0f, btnUp.y + 5.0f}, 
                             {btnUp.x + 140.0f, btnUp.y + 20.0f}, 
                             {btnUp.x + 170.0f, btnUp.y + 20.0f}, BLACK);
            }

            // O Loop Roda apenas os itens que devem ser exibidos na tela atual
            int startIdx = scrollOffset;
            int endIdx = startIdx + MAX_VISIBLE_ITEMS;
            if (endIdx > (int)availableAttacks.size()) endIdx = availableAttacks.size();

            for (int i = startIdx; i < endIdx; i++) {
                int displayIdx = i - startIdx; // Converte o ID real para a posição na tela (0 a 11)
                Rectangle attackRec = { 420.0f, 230.0f + (displayIdx * 40.0f), 310.0f, 35.0f };
                bool hover = CheckCollisionPointRec(mouse, attackRec);
                
                if (hover) {
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        activeChar.comboList[editingSlot] = availableAttacks[i]; 
                        
                        std::ofstream file(activeChar.comboFilePath);
                        for (int j = 0; j < 8; j++) file << activeChar.comboList[j] << "\n";
                        file.close();
                        
                        editingSlot = -1;
                    } else if (previewAnim != availableAttacks[i]) {
                        previewAnim = availableAttacks[i]; 
                    }
                }
                
                DrawRectangleRec(attackRec, hover ? GREEN : LIGHTGRAY);
                DrawText(availableAttacks[i].c_str(), attackRec.x + 10, attackRec.y + 10, 20, BLACK);
            }

            // BOTAO DE DESCER (Some automaticamente se chegar no último item)
            Rectangle btnDown = { 420.0f, 230.0f + (MAX_VISIBLE_ITEMS * 40.0f), 310.0f, 25.0f };
            if (endIdx < (int)availableAttacks.size()) {
                bool hoverDown = CheckCollisionPointRec(mouse, btnDown);
                if (hoverDown && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) scrollOffset++;
                DrawRectangleRec(btnDown, hoverDown ? LIGHTGRAY : GRAY);
                
                // Ponta Base -> Topo Dir -> Topo Esq
                DrawTriangle({btnDown.x + 155.0f, btnDown.y + 20.0f}, 
                             {btnDown.x + 170.0f, btnDown.y + 5.0f}, 
                             {btnDown.x + 140.0f, btnDown.y + 5.0f}, BLACK);
            }
        }

        //3. Coluna de Ajustes Físicos (Lado Direito)
        DrawRectangle(850, 150, 380, 400, Fade(DARKGRAY, 0.9f));
        DrawText("CALIBRAGEM DA FISICA", 870, 170, 20, WHITE);

        float yOffset = 220.0f;
        auto DrawAdjuster = [&](const char* title, float& val, float step, float minVal) {
            DrawText(TextFormat("%s: %.1f", title, val), 870, yOffset, 20, LIGHTGRAY);
            Rectangle btnMinus = { 870, yOffset + 25, 40, 30 };
            Rectangle btnPlus = { 920, yOffset + 25, 40, 30 };
            
            if (CheckCollisionPointRec(mouse, btnMinus)) {
                DrawRectangleRec(btnMinus, LIGHTGRAY);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) val = fmax(minVal, val - step);
            } else DrawRectangleRec(btnMinus, GRAY);

            if (CheckCollisionPointRec(mouse, btnPlus)) {
                DrawRectangleRec(btnPlus, LIGHTGRAY);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) val += step;
            } else DrawRectangleRec(btnPlus, GRAY);

            DrawRectangleLinesEx(btnMinus, 2, BLACK); DrawText("-", btnMinus.x + 15, btnMinus.y + 5, 20, BLACK);
            DrawRectangleLinesEx(btnPlus, 2, BLACK); DrawText("+", btnPlus.x + 13, btnPlus.y + 5, 20, BLACK);
            yOffset += 80.0f;
        };

        // Renderiza os 4 botões editores com seus respectivos passos matemáticos
        DrawAdjuster("Forca Magnetica (Arrasto Mao)", activeChar.magneticForce, 5.0f, 5.0f);
        DrawAdjuster("Velocidade (Arrasto Mouse)", activeChar.tkControlSpeed, 5.0f, 5.0f);
        DrawAdjuster("Tensao Elastica (Menor = Rigido)", activeChar.tkElasticity, 25.0f, 25.0f);
        
        //Calibrador do Poderzão (aumenta/diminui de 50 em 50)
        DrawAdjuster("Forca do Poder (Kamehameha)", activeChar.beamBaseForce, 50.0f, 100.0f);
        
        DrawText("Aperte 'P' para confirmar e sair", 50, 900, 20, YELLOW);
    }
};

// SISTEMA GLOBAL DE EFEITOS VISUAIS
struct ParryVFX {
    Vector2 position;
    float currentSize;
    float maxSize;
    float opacity;
    float speed;
};
std::vector<ParryVFX> parryEffects;

// Sistema de Faíscas e Camera Shake
float screenShakeTimer = 0.0f;
float screenShakeIntensity = 0.0f;

// NOVO: Controladores de Tempo Matrix
float slowMoTimer = 0.0f; 
float slowMoTargetScale = 1.0f;
float globalTimeScale = 1.0f;

bool isDeathCamera = false; // NOVO: Controla se a câmera está no modo filme
int deathTargetId = -1;     // NOVO: Quem a câmera vai focar (0 = Player, 1 = Bot)

struct Spark {
    Vector2 position;
    Vector2 velocity;
    float lifespan;
    float maxLifespan;
    Color color;
    float size;
    bool hasGravity;
    bool isWind;
};
std::vector<Spark> sparks;

// NOVO: Tipos de Partícula (0 = Acerto em Cheio, 1 = Bloqueio Esmirilhadeira, 2 = Sangue Jorrando)
void SpawnSparks(Vector2 pos, float force, Vector2 burstDir, int type = 0) {
    int count = 10;
    float spread = 0.0f;
    float speedMin = 0.0f, speedMax = 0.0f;
    float lifeMin = 0.0f, lifeMax = 0.0f;
    Color c1 = YELLOW, c2 = ORANGE;
    float sizeMin = 2.0f, sizeMax = 5.0f;

    if (type == 0) { // COMPORTAMENTO 3: ACERTO EM CHEIO (Normal Hit)
        count = (int)(force * 4.0f);           // ajuste faisca: Quantidade de particulas
        if (count > 200) count = 200;          // ajuste faisca: Limite maximo
        if (count < 10) count = 10;            // ajuste faisca: Limite minimo
        spread = 60.0f;                        // ajuste faisca: Angulo de abertura do leque
        speedMin = force * 15.0f;              // ajuste faisca: Velocidade minima do estouro
        speedMax = force * 50.0f;              // ajuste faisca: Velocidade maxima do estouro
        lifeMin = 0.2f; lifeMax = 0.6f;        // ajuste faisca: Tempo de vida na tela
        sizeMin = 3.0f; sizeMax = 8.0f;        // ajuste faisca: Tamanho das particulas
    } 
    else if (type == 1) { // COMPORTAMENTO 2: BLOQUEIO (Esmirilhadeira / Grind)
        count = (int)(force * 8.0f); 
        if (count > 300) count = 300;
        if (count < 15) count = 15;
        spread = 120.0f; // Voa pra todo lado
        speedMin = force * 20.0f; 
        speedMax = force * 80.0f; 
        lifeMin = 0.2f; lifeMax = 0.4f; // Morrem rápido simulando atrito
        sizeMin = 4.0f; sizeMax = 12.0f; // Bem gordas
    }
    else if (type == 2) { // COMPORTAMENTO 1: SANGUE JORRANDO (Perfurante)
        count = (int)(force * 2.0f);
        if (count > 100) count = 100;
        if (count < 5) count = 5;
        spread = 15.0f; // Jorro super concentrado (quase uma linha)
        speedMin = force * 5.0f; 
        speedMax = force * 20.0f; // Lento, não explode para longe, ele "vaza"
        lifeMin = 0.5f; lifeMax = 1.5f; // Fica bastante tempo caindo pesadão
        sizeMin = 3.0f; sizeMax = 7.0f;
        c1 = RED; c2 = ORANGE; // Cor de Sangue
    }

    float baseAngle = atan2(burstDir.y, burstDir.x);

    // MÁGICA DA OTIMIZAÇÃO: Escala a geração de partículas baseada no tempo!
    // Mantém a densidade visual idêntica na Câmera Lenta sem estourar a memória.
    count = (int)(count * globalTimeScale);
    
    // Garante que, se a escala for tão pequena que resulte em 0, ele jogue um "dado" para tentar spawnar pelo menos 1
    if (count == 0 && globalTimeScale > 0.0f) {
        if (GetRandomValue(1, 100) <= (int)(globalTimeScale * 100)) count = 1;
    }

    for (int i = 0; i < count; i++) {
        Spark s;
        s.position = pos;
        
        float angle = baseAngle + (float)GetRandomValue(-(int)spread, (int)spread) * DEG2RAD;
        float speed = GetRandomValue((int)speedMin, (int)speedMax);

        s.velocity.x = cos(angle) * speed;
        s.velocity.y = sin(angle) * speed;
        
        s.maxLifespan = (float)GetRandomValue((int)(lifeMin * 100), (int)(lifeMax * 100)) / 100.0f;
        s.lifespan = s.maxLifespan;
        s.size = (float)GetRandomValue((int)sizeMin, (int)sizeMax);
        s.color = (GetRandomValue(0, 1) == 0) ? c1 : c2;
        
        s.hasGravity = true; 
        s.isWind = false;
        sparks.push_back(s);
    }
}

// Explosão de vento/som do Parry (Sonic Boom)
void SpawnWindBurst(Vector2 pos, float force) {
    int count = (int)(force * 6.0f);
    if (count > 200) count = 200; 
    if (count < 15) count = 15;

    for (int i = 0; i < count; i++) {
        Spark s;
        s.position = pos;
        
        // Estoura em 360 graus perfeitos (Círculo de onda de choque)
        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        
        // VELOCIDADE ABSURDA: Simula o estampido do som quebrando no Parry
        float speed = GetRandomValue((int)(force * 50), (int)(force * 150));

        s.velocity.x = cos(angle) * speed;
        s.velocity.y = sin(angle) * speed;
        
        // TEMPO DE VIDA CURTO: É apenas um estalo rápido e brilhante
        s.maxLifespan = (float)GetRandomValue(100, 200) / 100.0f;
        s.lifespan = s.maxLifespan;
        
        s.size = (float)GetRandomValue(3, 8);
        s.color = WHITE; 
        
        s.hasGravity = false;
        s.isWind = false;
        sparks.push_back(s);
    }
}

// NOVO: Rajada de Vento Direcional para o Sopro Telecinético
void SpawnDirectionalWindBurst(Vector2 pos, float force, Vector2 dir) {
    int count = (int)(force * 1.5f); // Quantidade diretamente proporcional à força
    if (count > 300) count = 300; 
    if (count < 20) count = 20;

    float baseAngle = atan2(dir.y, dir.x);
    count = (int)(count * globalTimeScale);
    
    if (count == 0 && globalTimeScale > 0.0f) {
        if (GetRandomValue(1, 100) <= (int)(globalTimeScale * 100)) count = 1;
    }

    for (int i = 0; i < count; i++) {
        Spark s;
        s.position = pos;
        
        // Abre num arco perfeito de 45 graus para cada lado (90º de spread total) em direção ao mouse
        float spread = 45.0f;
        float angle = baseAngle + (float)GetRandomValue(-(int)spread, (int)spread) * DEG2RAD;
        
        // Velocidades altíssimas e variadas para dar profundidade ao jato de ar
        float speed = GetRandomValue((int)(force * 20), (int)(force * 80));

        s.velocity.x = cos(angle) * speed;
        s.velocity.y = sin(angle) * speed;
        
        s.maxLifespan = (float)GetRandomValue(15, 40) / 100.0f; // Vive pouco, "rasgando" a tela e sumindo
        s.lifespan = s.maxLifespan;
        
        s.size = (float)GetRandomValue(2, 6); // Espessura da linha de ar
        s.color = WHITE; 
        
        s.hasGravity = false;
        s.isWind = true; // Ativa o motor de rastro de vento
        
        sparks.push_back(s);
    }
}

void ProcessCombat(Character& attacker, Character& defender) {
    // 1. PERMITE a passagem se for um ataque normal OU uma Execução Conjunta (Tipo 5)
    if ((!attacker.isAttacking && attacker.executionType != 5) || attacker.hasDealtDamage || attacker.animator.isBlending) return;

    std::string anim = attacker.animator.currentAnim;
    
    if (attacker.animator.animType[anim] == 0) return;

    // Extrai todas as variáveis pro escopo global da função
    int hitType = attacker.animator.animHitType[anim];
    float atkDmg = attacker.pdl / attacker.atkBonus;
    float skillDmg = attacker.animator.animDamage[anim];
    float finalDamage = atkDmg + skillDmg; 
    Vector2 atkDir = attacker.animator.animDirection[anim];
    int atkTarget = attacker.animator.animTargetPart[anim];

    // 2. CORREÇÃO DO FRAME 0: Se o frame for 0, mapeia para o ÚLTIMO frame da animação!
    float activeFrame = attacker.animator.animActiveFrame[anim];
    if (activeFrame <= 0.0f && !attacker.animator.animations[anim].empty()) {
        activeFrame = attacker.animator.animations[anim].size() - 1.0f;
    }

    if (attacker.animator.currentFrame >= activeFrame) {
        
        Rectangle hb = attacker.animator.animHitbox[anim];
        float hbX = attacker.pPelvis->position.x + (attacker.facingDir > 0 ? hb.x : -(hb.x + hb.width));
        float hbY = attacker.pPelvis->position.y + hb.y;
        Rectangle attackBox = { hbX, hbY, hb.width, hb.height };
        
        float atkDmg = attacker.pdl / attacker.atkBonus;
        float skillDmg = attacker.animator.animDamage[anim];
        float finalDamage = atkDmg + skillDmg; 
        
        Vector2 atkDir = attacker.animator.animDirection[anim];
        int atkTarget = attacker.animator.animTargetPart[anim];

        if (!defender.isDead && !defender.isStunRagdoll) {
            float defTopY = defender.pHead->position.y - 80.0f; 
            float defBottomY = fmax(defender.pLFoot->position.y, defender.pRFoot->position.y) + 45.0f; 
            float defHeight = defBottomY - defTopY;
            Rectangle defendBox = { defender.pPelvis->position.x - 60.0f, defTopY, 120.0f, defHeight };

            if (CheckCollisionRecs(attackBox, defendBox)) {
                attacker.hasDealtDamage = true; 
                int hitType = attacker.animator.animHitType[anim];

                float overlapX = fmax(attackBox.x, defendBox.x);
                float overlapY = fmax(attackBox.y, defendBox.y);
                float overlapW = fmin(attackBox.x + attackBox.width, defendBox.x + defendBox.width) - overlapX;
                float overlapH = fmin(attackBox.y + attackBox.height, defendBox.y + defendBox.height) - overlapY;
                Vector2 impactPoint = { overlapX + overlapW / 2.0f, overlapY + overlapH / 2.0f };
                
                if (defender.isParryStunned || defender.isDowned) {
                    defender.isParryStunned = false;
                    defender.isDowned = false;
                    defender.isStunRagdoll = true; 
                    defender.isDead = true; 
                    defender.isStandingUp = false;
                    defender.isBlocking = false;
                    
                    defender.health -= finalDamage;
                    if (defender.health <= 0) {
                        defender.health = 0;
                        defender.isStunRagdoll = false; 
                    }

                    // Puxa o 'dt' oficial e estabiliza a matemática de força do Ragdoll!
                    float dt = GetFrameTime() * globalTimeScale;
                    float forceMultiplier = finalDamage * 300.0f * dt; //AJUST O MULT DA FORÇA
                    for (auto& p : defender.world.particles) {
                        p->previousPosition.x -= atkDir.x * attacker.facingDir * forceMultiplier;
                        p->previousPosition.y -= atkDir.y * forceMultiplier;
                    }
                    
                    SpawnSparks(impactPoint, finalDamage, {-attacker.facingDir, 0.0f}, 0); 
                    return; 
                }

                if (defender.isBlocking && !defender.shieldBroken && defender.facingDir != attacker.facingDir) {
                    if (defender.parryWindow > 0.0f) {
                        attacker.TakeDamage("blockReactStrong", defender.facingDir, finalDamage * 2.0f, true, {-atkDir.x, -atkDir.y}, atkTarget, hitType, true);
                        
                        defender.isParrying = true;
                        float overlapX = fmax(attackBox.x, defendBox.x);
                        float overlapY = fmax(attackBox.y, defendBox.y);
                        float overlapW = fmin(attackBox.x + attackBox.width, defendBox.x + defendBox.width) - overlapX;
                        float overlapH = fmin(attackBox.y + attackBox.height, defendBox.y + defendBox.height) - overlapY;
                        
                        Vector2 impactPoint = { overlapX + overlapW / 2.0f, overlapY + overlapH / 2.0f };
                        float maxVfxSize = finalDamage * 15.0f; 
                        
                        parryEffects.push_back({ impactPoint, 1.0f, maxVfxSize, 1.0f, maxVfxSize * 5.0f });
                        
                        // CAMERA SHAKE PROPORCIONAL AO PARRY
                        screenShakeTimer = 0.2f;
                        screenShakeIntensity = finalDamage * 3.0f; 

                        // GATILHO DO SLOW MOTION DINÂMICO!
                        slowMoTimer = (finalDamage * 2.0f) / 100.0f; 
                        if (slowMoTimer > 15.0f) slowMoTimer = 15.0f; 
                        
                        // Define a % da velocidade (10 dano = 0.9 / 50 dano = 0.5 / 90+ dano = 0.1)
                        slowMoTargetScale = fmax(0.1f, 1.0f - (finalDamage / 100.0f));

                        // EXPLOSÃO DO SONIC BOOM (Vento/Som)
                        SpawnWindBurst(impactPoint, finalDamage);

                        defender.parryWindow = 0.0f;
                        return;
                    }

                    std::string blockAnim = "blockReactHigh"; 
                    if (hitType == 1) blockAnim = "blockReactMedium";       
                    else if (hitType == 2) blockAnim = "blockReactLow";  
                    else if (hitType == 3) blockAnim = "blockReactStrong"; 

                    SpawnSparks(impactPoint, finalDamage, {-attacker.facingDir, 0.0f}, 1); // PRESET 1: FAÍSCAS DE DEFESA
                    defender.TakeDamage(blockAnim, attacker.facingDir, finalDamage, true, atkDir, atkTarget, hitType);
                    return;
                }
                
                std::string hitAnim = "high";
                int randReact = GetRandomValue(1, 2); 
                if (hitType == 0) hitAnim = (randReact == 1) ? "high" : "high2";
                else if (hitType == 1) hitAnim = (randReact == 1) ? "medium" : "medium2";       
                else if (hitType == 2) hitAnim = (randReact == 1) ? "low" : "low2";  
                else if (hitType == 3) {
                    if (defender.shieldBroken) hitAnim = (randReact == 1) ? "strong" : "strong2";
                    else hitAnim = (randReact == 1) ? "recovery" : "recovery2";

                    // SHAKE DA PORRADA FORTE
                    screenShakeTimer = 0.2f;
                    screenShakeIntensity = finalDamage * 2.0f;
                }
                else if (hitType == 4) hitAnim = attacker.animator.animCustomReaction[anim];

                SpawnSparks(impactPoint, finalDamage, {-attacker.facingDir, 0.0f}, 0); // FAÍSCAS DE ACERTO EM CHEIO
                defender.TakeDamage(hitAnim, attacker.facingDir, finalDamage, false, atkDir, atkTarget, hitType);
            }
        } else {
            // FÍSICA DE JUGGLE PARA RAGDOLL MORTO (Agora ativado pela Animação Conjunta!)
            bool hitRagdoll = false;
            float dt = GetFrameTime() * globalTimeScale; 
            float forceMultiplier = finalDamage * 50000.0f * dt; 
            
            for (auto& p : defender.world.particles) {
                // Checa Apenas as bolinhas físicas que ENCOSTARAM na Hitbox Vermelha!
                if (CheckCollisionCircleRec(p->position, p->radius, attackBox)) {
                    hitRagdoll = true;
                    p->previousPosition.x -= atkDir.x * attacker.facingDir * forceMultiplier;
                    p->previousPosition.y -= atkDir.y * forceMultiplier;
                    
                    // Traz o tipo de partícula correta do Animator
                    SpawnSparks(p->position, finalDamage * 0.5f, {-attacker.facingDir, 0.0f}, hitType); 
                }
            }
            if (hitRagdoll) {
                attacker.hasDealtDamage = true;
                
                // Shake para o impacto colossal no Ragdoll
                screenShakeTimer = 0.2f;
                screenShakeIntensity = fmin(finalDamage * 2.0f, 100.0f);

                if (defender.isStunRagdoll) {
                    defender.health -= finalDamage;
                    if (defender.health <= 0) {
                        defender.health = 0;
                        defender.isStunRagdoll = false; 
                    }
                }
            }
        }
    }
}

void CollideRagdoll(Character& ragdoll, Character& living) {
    if ((!ragdoll.isDead && !ragdoll.isStunRagdoll) || living.isDead || living.isStunRagdoll) return;

    // Função interna que empurra um ponto genérico para fora de uma cápsula viva
    auto resolvePoint = [&](Vector2& pos, Vector2& prevPos, float radius, Vector2 pA, Vector2 pB, float width) {
        float dx = pB.x - pA.x;
        float dy = pB.y - pA.y;
        float l2 = dx*dx + dy*dy;
        if (l2 == 0.0f) return;

        float t = fmax(0.0f, fmin(1.0f, ((pos.x - pA.x) * dx + (pos.y - pA.y) * dy) / l2));
        Vector2 closest = { pA.x + t * dx, pA.y + t * dy };

        float distX = pos.x - closest.x;
        float distY = pos.y - closest.y;
        float dist = sqrt(distX*distX + distY*distY);

        float targetDist = (width / 2.0f) + radius;

        if (dist > 0 && dist < targetDist) {
            float overlap = targetDist - dist;
            float nx = distX / dist;
            float ny = distY / dist;

            pos.x += nx * overlap;
            pos.y += ny * overlap;
            
            // Simula atrito para o corpo não escorregar como gelo
            prevPos.x = pos.x - (pos.x - prevPos.x) * 0.5f;
            prevPos.y = pos.y - (pos.y - prevPos.y) * 0.5f;
        }
    };

    // Varredura anatômica: Testa o ponto contra o esqueleto do personagem vivo
    auto testBody = [&](Vector2& pos, Vector2& prevPos, float radius) {
        resolvePoint(pos, prevPos, radius, living.pNeck->position, living.pPelvis->position, 160.0f); 
        resolvePoint(pos, prevPos, radius, living.pNeck->position, living.pHead->position, 160.0f);   
        resolvePoint(pos, prevPos, radius, living.pPelvis->position, living.pLKnee->position, 75.0f); 
        resolvePoint(pos, prevPos, radius, living.pLKnee->position, living.pLFoot->position, 75.0f);  
        resolvePoint(pos, prevPos, radius, living.pPelvis->position, living.pRKnee->position, 75.0f); 
        resolvePoint(pos, prevPos, radius, living.pRKnee->position, living.pRFoot->position, 75.0f);  
        resolvePoint(pos, prevPos, radius, living.pNeck->position, living.pLElbow->position, 80.0f);  
        resolvePoint(pos, prevPos, radius, living.pLElbow->position, living.pLHand->position, 80.0f); 
        resolvePoint(pos, prevPos, radius, living.pNeck->position, living.pRElbow->position, 80.0f);  
        resolvePoint(pos, prevPos, radius, living.pRElbow->position, living.pRHand->position, 80.0f); 
    };

    // NOVO: Iteramos a resolução física para garantir que o corpo do ragdoll atue como um bloco sólido
    for (int i = 0; i < 3; i++) {
        // FASE 1: Colisão das Juntas (Resolve as esferas principais)
        for (auto& p : ragdoll.world.particles) {
            testBody(p->position, p->previousPosition, p->radius);
        }

        // FASE 2: Colisão do Recheio (Torna o espaço vazio entre as esferas sólido)
        for (auto& c : ragdoll.world.constraints) {
            // Encontra o meio exato do osso do ragdoll
            Vector2 midPos = { (c->p1->position.x + c->p2->position.x) / 2.0f, (c->p1->position.y + c->p2->position.y) / 2.0f };
            Vector2 midPrev = { (c->p1->previousPosition.x + c->p2->previousPosition.x) / 2.0f, (c->p1->previousPosition.y + c->p2->previousPosition.y) / 2.0f };
            
            Vector2 oldMidPos = midPos;
            float avgRadius = (c->p1->radius + c->p2->radius) / 2.0f;

            // Testa se o meio do osso morto colidiu com o vivo
            testBody(midPos, midPrev, avgRadius);

            // Se o meio precisou ser empurrado, distribui a força igualzinha pras pontas do osso
            float diffX = midPos.x - oldMidPos.x;
            float diffY = midPos.y - oldMidPos.y;

            if (diffX != 0.0f || diffY != 0.0f) {
                c->p1->position.x += diffX; c->p1->position.y += diffY;
                c->p2->position.x += diffX; c->p2->position.y += diffY;
                
                c->p1->previousPosition.x += diffX; c->p1->previousPosition.y += diffY;
                c->p2->previousPosition.x += diffX; c->p2->previousPosition.y += diffY;
            }
        }

        // FASE 3: Cura Estrutural (MÁGICA)
        // Aciona a mola de tensão do ragdoll internamente para curar qualquer osso que a Fase 1 ou 2 tenha esticado.
        // Como isso tá dentro de um loop de 3 passos, o osso inteiro "desliza" para fora do braço do jogador sem esticar!
        ragdoll.world.SolveConstraints();
    }
}

int main() {
    InitWindow(1600, 1000, "Galactic Shounen - Animator Sandbox");
    SetTargetFPS(60);

    Character player;
    player.Init({ 800.0f, 200.0f });

    Bot enemy;
    enemy.Init({ 1300.0f, 200.0f });
    enemy.LoadSkin("Textures\\megatron");
    enemy.currentMode = Character::GAMEPLAY;

    enemy.comboFilePath = "animations\\botCombo\\loadout.txt";
    enemy.LoadCombo();

    const float groundY = 900.0f;

    // NOVO: Inicializa a Câmera
    Camera2D camera = { 0 };
    camera.target = { 800.0f, groundY - 350.0f }; // Foca no centro da arena
    camera.offset = { 1600.0f / 2.0f, 1000.0f / 2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;
    bool botActive = true;
    bool botAggressive = true;
    bool botBlocking = false;
    bool showHitboxes = false;
    bool showUIMenu = true;
    bool enableCinematicDeath = true;
    float userZoomScale = 1.0f;
    ComboMenu comboMenu;

    bool typingFrame = false;
    std::string frameText = "";
    bool creatingHitbox = false;
    bool drawingHitbox = false;
    bool drawingVector = false;
    Vector2 dragStart = {0,0}, dragEnd = {0,0};

    bool typingReaction = false;
    std::string reactionText = "";

    while (!WindowShouldClose()) {

       // Atalho do Menu (Apenas no Gameplay)
        if (IsKeyPressed(KEY_P) && player.currentMode == Character::GAMEPLAY) {
            if (!comboMenu.isOpen) {
                comboMenu.Open(player, enemy);
                player.pPelvis->position = { 800.0f, 482.5f }; 
                
                // Reseta todos os estados para evitar softlock!
                player.isAttacking = false; player.comboStep = 0; player.inputBuffer = false; 
                player.isExecuting = false; player.executionType = 0; player.isChargingThrow = false;
                player.isHit = false; player.isBlocking = false; player.pendingHit.active = false;
                
                enemy.isAttacking = false; enemy.comboStep = 0; enemy.inputBuffer = false; 
                enemy.isHit = false; enemy.isBlocking = false; enemy.pendingHit.active = false;
            } else {
                comboMenu.isOpen = false;
                player.animator.Play("idle", player.currentAngles); 
            }
        }
        
        // Controle de Zoom com o Scroll (Funciona em todos os modos)
        camera.zoom += GetMouseWheelMove() * 0.1f;
        if (camera.zoom < 0.2f) camera.zoom = 0.2f;
        if (camera.zoom > 3.0f) camera.zoom = 3.0f;

        if (IsKeyPressed(KEY_M) && player.currentMode == Character::GAMEPLAY) {
            showUIMenu = !showUIMenu;
        }

        //Câmera Dinâmica de Jogo de Luta
        if (player.currentMode == Character::GAMEPLAY) {
            // 1. O usuário controla o multiplicador do zoom livremente com o scroll
            userZoomScale += GetMouseWheelMove() * 0.1f;
            if (userZoomScale < 0.3f) userZoomScale = 0.3f; 
            if (userZoomScale > 3.0f) userZoomScale = 3.0f; 

            float targetX, targetY, targetZoom;
            
            // NOVO: Verifica se deve travar a câmera na cutscene
            bool lockCamera = (player.isExecuting && player.executionType == 5 && player.animator.animCamFollow[player.animator.currentAnim] == 0);

            if (isDeathCamera) {
                // CÂMERA DE MORTE: Foca no defunto com Zoom fixo e absoluto
                Character& deadChar = (deathTargetId == 0) ? player : enemy;
                targetX = deadChar.pPelvis->position.x;
                targetY = deadChar.pPelvis->position.y - 150.0f;
                targetZoom = 1.0f;      //AJUST AQUI O ZOOM
            } else if (lockCamera) {
                // CÂMERA DE CUTSCENE: Mantém os alvos cravados na posição em que ela parou
                targetX = camera.target.x;
                targetY = camera.target.y;
                targetZoom = camera.zoom;
            } else {
                // CÂMERA NORMAL: Mantém os dois na tela
                float midX = (player.pPelvis->position.x + enemy.pPelvis->position.x) / 2.0f;
                float midY = ((player.pPelvis->position.y - 200.0f) + (enemy.pPelvis->position.y - 200.0f)) / 2.0f;
                float dist = fabs(player.pPelvis->position.x - enemy.pPelvis->position.x);
                
                targetX = midX;
                targetY = midY;
                
                // Margem exata das bordas da tela (Aprox. 3 dedos)
                float screenMargin = 250.0f; 
                
                // Descobre o Zoom MÁXIMO PERMITIDO para que a distância atual caiba nos 1100px restantes da tela
                float maxAllowedZoom = (1600.0f - (screenMargin * 2.0f)) / fmax(dist, 1.0f);
                
                // Zoom que o jogador gostaria de ter
                float desiredZoom = 0.7f * userZoomScale; 
                
                // TRAVA MATEMÁTICA: Usa o zoom do jogador, MAS NUNCA ultrapassa o limite permitido pela tela
                targetZoom = fmin(desiredZoom, maxAllowedZoom); 
                
                if (targetZoom < 0.05f) targetZoom = 0.05f; 
                if (targetZoom > 1.5f) targetZoom = 1.5f; 
            }

            // Move e dá o Zoom de forma interpolada (Suave)
            camera.target.x += (targetX - camera.target.x) * 0.1f;
            camera.target.y += (targetY - camera.target.y) * 0.1f;
            camera.zoom += (targetZoom - camera.zoom) * 0.1f; 
        }
        else {
            // Nos modos ANIMATOR e RAGDOLL, o Pan e Zoom manuais ficam livres e isolados
            camera.zoom += GetMouseWheelMove() * 0.1f;
            if (camera.zoom < 0.2f) camera.zoom = 0.2f;
            if (camera.zoom > 3.0f) camera.zoom = 3.0f;

            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                Vector2 delta = GetMouseDelta();
                camera.target.x -= delta.x / camera.zoom;
                camera.target.y -= delta.y / camera.zoom;
            }
        }

        Vector2 mouseScreen = GetMousePosition();
        Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, camera);

        // A MÁGICA: Calcula o multiplicador de tempo global!
        if (slowMoTimer > 0.0f) {
            slowMoTimer -= GetFrameTime(); 
            globalTimeScale = slowMoTargetScale; // MATRIX! Usa a % dinâmica exata do último Parry
        } else {
            globalTimeScale = 1.0f;
            isDeathCamera = false; // DESLIGA a Câmera de Morte suavemente quando o tempo acaba!
        }
        
        // ESSE É O TEMPO OFICIAL DO JOGO:
        float dt = GetFrameTime() * globalTimeScale;
        
        if (!comboMenu.isOpen) {
            
            if (IsKeyPressed(KEY_R) && player.currentMode != Character::RAGDOLL) {
                
                // Função Faxineira: Reseta o estado completo de qualquer Character
                auto ResetCharacter = [](Character& c, Vector2 startPos, float startDir) {
                    c.pPelvis->position = startPos;
                    c.facingDir = startDir;
                    
                    c.health = c.maxHealth; 
                    c.shield = c.maxShield; 
                    c.shieldBroken = false; 
                    
                    // Reseta Atordoamentos e Ragdoll (Corrige a Intangibilidade)
                    c.isDead = false;
                    c.isStunRagdoll = false;
                    c.isDowned = false;
                    c.isStandingUp = false;
                    c.isParryStunned = false;
                    c.isParrying = false;
                    c.isHit = false;
                    c.stunTimer = 0.0f;
                    c.slideVelocity = 0.0f;
                    
                    // Reseta Ações e Combos
                    c.isAttacking = false;
                    c.comboStep = 0;
                    c.inputBuffer = false;
                    c.hasDealtDamage = false;
                    c.isBlocking = false;
                    
                    // Reseta Execuções e Poderes
                    c.isExecuting = false;
                    c.executionType = 0;
                    c.executionTimer = 0.0f;
                    c.isChargingThrow = false;
                    c.beamActive = false;
                    c.isKnockbackActive = false;
                    
                    // Reseta a Memória de Combate
                    c.pendingHit.active = false;
                    c.consecutiveHitsTaken = 0;
                    c.consecutiveBlocks = 0;
                    c.wantsToParry = false;
                    c.blockTimer = 0.0f;
                    
                    // Reseta a Animação para a pose base inicial
                    c.animator.isBlending = false;
                    c.animator.currentAnim = "idle";
                    c.animator.currentFrame = 0.0f;
                    c.animator.animFinished = false;
                    
                    // O ApplyFK reposiciona todos os ossos E zera a velocidade física do Ragdoll antigo
                    c.ApplyFK(); 
                };

                // Aplica a faxina no Jogador e no Inimigo
                ResetCharacter(player, { 800.0f, 482.5f }, 1.0f);
                ResetCharacter(enemy, { 1300.0f, 482.5f }, -1.0f);
                
                // Limpeza dos Timers exclusivos do Bot (Corrige o "Bot Burro")
                enemy.parryCooldown = 0.0f;
                enemy.opportunistTimer = 0.0f;
                enemy.currentPattern = Bot::PATTERN_PASSIVE;
                
                // Limpa Efeitos Visuais e Câmera Lenta que possam ter sobrado na tela
                slowMoTimer = 0.0f;
                globalTimeScale = 1.0f;
                isDeathCamera = false;
                screenShakeTimer = 0.0f;
                parryEffects.clear();
                sparks.clear();
            }

            // PROCESSADOR GLOBAL DE RAGDOLL (Dano, Paredes, Chão, Shake e Faíscas)
            auto ProcessRagdollPhysics = [&](Character& c) {
                if (c.isDead) { // Roda para o StunRagdoll E para a morte definitiva
                    
                    if (dt > 0.0f) {
                        for (auto& p : c.world.particles) {
                            float vx = p->position.x - p->previousPosition.x;
                            float vy = p->position.y - p->previousPosition.y;
                            
                            // Limita a velocidade bruta antes de calcular o dano de impacto.
                            // Isso impede que o "arranco" inicial do golpe cause dano infinito no chão ou na parede
                            if (vx > 150.0f) vx = 150.0f;
                            if (vx < -150.0f) vx = -150.0f;
                            if (vy > 150.0f) vy = 150.0f;
                            if (vy < -150.0f) vy = -150.0f;

                            float speedX = fabs(vx) / dt;
                            float speedY = fabs(vy) / dt;

                            // PAREDES
                            if ((p->position.x - p->radius <= -9900.0f && vx < 0) || (p->position.x + p->radius >= 9900.0f && vx > 0)) {
                                if (speedX > 400.0f) { 
                                    if (c.isStunRagdoll) c.health -= speedX * c.ragdollDamageMultiplier * dt; 
                                    p->previousPosition.x = p->position.x; 
                                    
                                    if (speedX > 1500.0f) { 
                                        // fmin garante que o tremor nunca ultrapasse 40.0f de intensidade!
                                        screenShakeTimer = 0.15f; 
                                        screenShakeIntensity = fmin(speedX * 0.02f, 100.0f); 
                                        
                                        Vector2 sparkDir = (vx < 0) ? Vector2{1.0f, -0.3f} : Vector2{-1.0f, -0.3f};
                                        SpawnSparks(p->position, speedX * 0.01f, sparkDir, 0); 
                                    } 
                                }
                            }
                            
                            // CHÃO
                            float predictedY = p->position.y + vy;
                            if (predictedY + p->radius >= groundY && vy > 0) {
                                if (speedY > 400.0f) {
                                    if (c.isStunRagdoll) c.health -= speedY * c.ragdollDamageMultiplier * dt; 
                                    p->previousPosition.y = p->position.y; 
                                    
                                    if (speedY > 1500.0f) { 
                                        // Limite de tremor no chão também
                                        screenShakeTimer = 0.15f; 
                                        screenShakeIntensity = fmin(speedY * 0.02f, 100.0f); 
                                        
                                        SpawnSparks(p->position, speedY * 0.01f, {0.0f, -1.0f}, 0);
                                    } 
                                }
                            }
                        }
                    }

                    // Lógica exclusiva do Stun Temporário
                    if (c.isStunRagdoll) {
                        if (c.health <= 0) {
                            c.health = 0;
                            c.isStunRagdoll = false; // Virou morte definitiva
                        } else {
                            c.shield += (c.maxShield / 400.0f) * dt;
                            if (c.shield >= c.maxShield) {
                                c.shield = c.maxShield;
                                c.shieldBroken = false;
                                c.isStunRagdoll = false;
                                c.isDead = false; 
                                c.isStandingUp = true;
                                
                                c.pPelvis->position.y = groundY - 422.5f; 
                                c.velocityY = 0.0f;
                                c.slideVelocity = 0.0f;
                                
                                c.animator.Play("recoveryRag", c.currentAngles);
                                c.animator.playDirection = 1.0f;
                            }
                        }
                    }
                }
            };

            ProcessRagdollPhysics(player);
            ProcessRagdollPhysics(enemy);
            
            // NOVO: Gatilho da Execução Conjunta (V) Processado ANTES do Update do Jogador
            if (player.currentMode == Character::GAMEPLAY && (enemy.isParryStunned || enemy.isDowned) && !player.isExecuting && !enemy.isDead) {
                float dist = fabs(player.pPelvis->position.x - enemy.pPelvis->position.x);
                if (dist <=350.0f && IsKeyPressed(KEY_V)) {
                    std::vector<std::string> jointAnims;
                    for (auto const& [name, type] : player.animator.animType) {
                        if (type == 2) jointAnims.push_back(name);
                    }
                    if (!jointAnims.empty()) {
                        std::string chosen = jointAnims[GetRandomValue(0, jointAnims.size() - 1)];
                            player.isExecuting = true;
                            player.executionType = 5; // Estado Conjunto
                            player.executionTimer = 0.0f;
                            player.hasDealtDamage = false; // NOVO: Zera o dano para a hitbox bater!
                            player.executionStartPos = player.pPelvis->position; // SALVA POS DO ATACANTE
                            player.animator.ForceBlend(chosen, player.currentAngles);
                        
                        enemy.isExecuting = true;
                        enemy.executionType = 5;
                        enemy.isParryStunned = false; enemy.isDowned = false;
                        enemy.shieldBroken = false; 
                        enemy.isDead = false;          // Ressuscita visualmente pra ser executado
                        enemy.isStunRagdoll = false;   // Tira da física da parede temporariamente
                        enemy.executionStartPos = enemy.pPelvis->position; // SALVA POS DO DEFENSOR
                        
                        // CORREÇÃO MESTRE DO ESPELHAMENTO:
                        // Se o bot vai virar de lado para encarar o player,
                        // invertemos matematicamente todos os ossos para eles não darem um giro de 180 graus visuais!
                        if (enemy.facingDir != -player.facingDir) {
                            enemy.facingDir = -player.facingDir;
                            for (int i = 0; i < 14; i++) {
                                enemy.currentAngles[i] = PI - enemy.currentAngles[i];
                            }
                        }
                        
                        // Inicia o blend suave do defensor
                        for (int i = 0; i < 14; i++) enemy.animator.savedAngles[i] = enemy.currentAngles[i];
                        enemy.animator.blendTimer = 0.0f;
                        enemy.animator.isBlending = true;
                    }
                }
            }

            // CORREÇÃO 1: Passar o 'dt' como primeiro parâmetro para o jogador!
            player.Update(dt, groundY, mouseWorld, mouseScreen);

            if (player.currentMode == Character::GAMEPLAY) {
                // Variáveis estáticas salvam a força antes do Player voltar pro Idle
                static float lastJointDmg = 0.0f;
                static Vector2 lastJointDir = {0,0};
                static float lastJointPwr = 0.0f;

                // NOVO: Controle de Sincronia do Defensor na Animação Conjunta
                if (player.isExecuting && player.executionType == 5 && enemy.isExecuting) {
                    enemy.facingDir = -player.facingDir; // O Bot SEMPRE encara o oposto do Player!
                    
                    if (enemy.animator.isBlending) {
                        enemy.animator.blendTimer += dt;
                        float t = enemy.animator.blendTimer / enemy.animator.BLEND_DURATION;
                        if (t >= 1.0f) { t = 1.0f; enemy.animator.isBlending = false; }
                        
                        float targetAngles[14];
                        Vector2 targetRoot;
                        player.animator.UpdateDefender(targetAngles, targetRoot, player.pPelvis->position, player.facingDir);
                        
                        for (int i = 0; i < 14; i++) enemy.currentAngles[i] = enemy.animator.LerpAngle(enemy.animator.savedAngles[i], targetAngles[i], t);
                        
                        enemy.pPelvis->position.x = enemy.executionStartPos.x + (targetRoot.x - enemy.executionStartPos.x) * t;
                        enemy.pPelvis->position.y = enemy.executionStartPos.y + (targetRoot.y - enemy.executionStartPos.y) * t;
                    } else {
                        player.animator.UpdateDefender(enemy.currentAngles, enemy.pPelvis->position, player.pPelvis->position, player.facingDir);
                    }
                    enemy.ApplyFK();

                    // LÓGICA DE SOLTURA DO RAGDOLL (Delega o impacto físico para o ProcessCombat!)
                    float activeFrame = player.animator.animActiveFrame[player.animator.currentAnim];
                    if (activeFrame <= 0.0f && !player.animator.animations[player.animator.currentAnim].empty()) {
                        activeFrame = player.animator.animations[player.animator.currentAnim].size() - 1.0f;
                    }

                    if (!player.animator.isBlending && player.animator.currentFrame >= activeFrame) {
                        // Solta o bot instantaneamente ANTES do ProcessCombat rodar logo abaixo!
                        enemy.isExecuting = false;
                        enemy.executionType = 0;
                        
                        enemy.isStunRagdoll = true; 
                        enemy.isDead = true; 
                        enemy.isStandingUp = false;
                        enemy.isDowned = false;
                        enemy.isBlocking = false;
                        
                        // NÃO aplicamos dano ou vetor aqui! 
                        // O ProcessCombat abaixo vai ver a Hitbox, checar colisão com as partículas 
                        // e arremessar as balljoints organicamente!
                    }

                } else if ((botActive || enemy.isDead) && !enemy.isExecuting) {
                    enemy.UpdateAI(dt, groundY, player, botAggressive, botBlocking);
                }
                
                // ENCERRAMENTO E FAILSAFE DA ANIMAÇÃO DO PLAYER
                if (player.isExecuting && player.executionType == 5 && player.animator.animFinished) {
                    player.isExecuting = false;
                    player.executionType = 0;
                    player.animator.ForceBlend("idle", player.currentAngles);
                    
                    // Failsafe: Se a hitbox configurada no editor foi desenhada muito longe e errou o bot, liberta ele para não travar
                    if (enemy.isExecuting) {
                        enemy.isExecuting = false;
                        enemy.executionType = 0;
                        enemy.isStunRagdoll = true; 
                        enemy.isDead = true; 
                        enemy.isStandingUp = false;
                        enemy.isDowned = false;
                        enemy.isBlocking = false;
                    }
                }
                
                // 1. Testa colisões e spawna faíscas sem interromper os ataques
                ProcessCombat(player, enemy);
                if (botActive && botAggressive) ProcessCombat(enemy, player);

                // 2. NOVO: Resolve os danos diferidos (Permite Double KO / Trade Perfeito!)
                player.ResolveDamage();
                enemy.ResolveDamage();

                // NOVO: Micro-Solver de Corpos Sólidos (Impede braços e corpos de se atravessarem ou furarem paredes)
                if (!player.isDead && !player.isStunRagdoll && !enemy.isDead && !enemy.isStunRagdoll && botActive && !player.isExecuting) {
                    
                    // Escaneia a hitbox dinâmica de quem quer que seja chamado
                    auto GetBounds = [](Character& c, float& minX, float& maxX) {
                        minX = 999999.0f; maxX = -999999.0f;
                        for (auto& p : c.world.particles) {
                            float r = -250.0f; 
                            if (p->position.x - r < minX) minX = p->position.x - r;
                            if (p->position.x + r > maxX) maxX = p->position.x + r;
                        }
                    };

                    // Roda 3 vezes para garantir que o empurrão de um não faça o outro furar a parede
                    for (int i = 0; i < 3; i++) {
                        // 1. Ninguém fura a parede
                        player.HandleWallCollisions();
                        enemy.HandleWallCollisions();

                        // 2. Lê o tamanho exato dos lutadores na pose de animação atual
                        float pMin, pMax, eMin, eMax;
                        GetBounds(player, pMin, pMax);
                        GetBounds(enemy, eMin, eMax);

                        bool playerLeft = player.pPelvis->position.x < enemy.pPelvis->position.x;
                        float overlap = 0.0f;
                        
                        // Trava do Tronco (garante a separação base de 120px)
                        float coreDist = fabs(player.pPelvis->position.x - enemy.pPelvis->position.x);
                        float coreOverlap = (coreDist < 120.0f) ? (120.0f - coreDist) : 0.0f;

                        // Trava dos Membros (Braços e Pernas)
                        if (playerLeft) {
                            if (pMax > eMin) overlap = pMax - eMin;
                        } else {
                            if (eMax > pMin) overlap = eMax - pMin;
                        }

                        // Aplica o empurrão que for maior: o do tronco ou o da ponta do braço
                        overlap = fmax(overlap, coreOverlap);

                        if (overlap > 0.0f) {
                            float push = overlap / 2.0f;
                            if (playerLeft) {
                                player.pPelvis->position.x -= push;
                                enemy.pPelvis->position.x += push;
                            } else {
                                player.pPelvis->position.x += push;
                                enemy.pPelvis->position.x -= push;
                            }
                            // Atualiza os membros visualmente antes de repetir o loop
                            player.ApplyFK();
                            enemy.ApplyFK();
                        }
                    }
                }

                // FÍSICA DOS PODERES (Sopro e Kamehameha)
                if (player.beamActive) {
                    player.beamTimer -= dt; 
                    if (player.beamTimer <= 0.0f) player.beamActive = false;
                    
                    if (!player.isVisibleBeam) {
                        // FORÇA DE SOPRO TELECINÉTICO (Push instantâneo de Área)
                        player.beamActive = false; // Desliga imediatamente no 1º frame para ser um impacto explosivo
                        
                        Vector2 blastCenter = player.pRHand->position;
                        float maxDist = 1200.0f; // Alcance do sopro
                        
                        float dt_local = GetFrameTime() * globalTimeScale;
                        float forceMultiplier = player.beamForce * 350.0f * dt_local; 
                        
                        bool hitSomething = false;

                        // Calcula a distância base do centro do inimigo
                        float dx = enemy.pPelvis->position.x - blastCenter.x;
                        float dy = enemy.pPelvis->position.y - blastCenter.y;
                        float baseDist = sqrt(dx*dx + dy*dy);
                        
                        // Se o inimigo estiver no raio da rajada
                        if (baseDist < maxDist) {
                            
                            // CONVERSÃO IMEDIATA: Se estiver stunnado/caído, vira Ragdoll AGORA para voar com a física!
                            if (!enemy.isDead && !enemy.isStunRagdoll && (enemy.isParryStunned || enemy.isDowned)) {
                                enemy.isParryStunned = false;
                                enemy.isDowned = false;
                                enemy.isStunRagdoll = true; 
                                enemy.isDead = true; 
                                enemy.isStandingUp = false;
                                enemy.isBlocking = false;
                            }

                            // 1. Interação com Inimigo em Ragdoll (Inclui os recém-convertidos)
                            if (enemy.isDead || enemy.isStunRagdoll) {
                                for (auto& p : enemy.world.particles) {
                                    // Distância precisa por partícula para espalhar os membros corretamente
                                    float pdx = p->position.x - blastCenter.x;
                                    float pdy = p->position.y - blastCenter.y;
                                    float pDist = sqrt(pdx*pdx + pdy*pdy);
                                    
                                    if (pDist < maxDist) {
                                        float falloff = 1.0f - (pDist / maxDist);
                                        falloff = falloff * falloff; 
                                        
                                        p->previousPosition.x -= player.beamDir.x * forceMultiplier * falloff;
                                        p->previousPosition.y -= player.beamDir.y * forceMultiplier * falloff;
                                        hitSomething = true;
                                    }
                                }
                            } 
                            // 2. Interação com Inimigo VIVO (Empurrão Orgânico Fortalecido)
                            else {
                                float falloff = 1.0f - (baseDist / maxDist);
                                falloff = falloff * falloff; 
                                
                                float dirSign = (player.beamDir.x > 0) ? 1.0f : -1.0f;
                                
                                if (enemy.isBlocking) {
                                    // BLOQUEANDO: Desliza pesado, quebrando a guarda se necessário
                                    enemy.shield -= player.beamForce * falloff * 0.2f;
                                    
                                    if (enemy.shield <= 0 && !enemy.shieldBroken) {
                                        enemy.shield = 0;
                                        enemy.shieldBroken = true;
                                        enemy.isBlocking = false;
                                        enemy.TakeDamage("blockReactStrong", player.facingDir, 0.0f, false, {0,0}, 1, 3);
                                        
                                        // PROCESSA O DANO AGORA (Esvazia a fila antes de aplicarmos o vento)
                                        enemy.ResolveDamage(); 
                                        enemy.slideVelocity = (player.beamForce * 30.0f * falloff) * dirSign; 
                                    } else {
                                        enemy.isHit = true;
                                        enemy.stunTimer = 0.2f;
                                        enemy.animator.Play("blockReactHigh", enemy.currentAngles);
                                        enemy.slideVelocity = (player.beamForce * 30.0f * falloff) * dirSign;
                                    }
                                } else {
                                    // ABERTO: Voa longe com Reaction Strong e força brutal (Força suficiente para dar Wall Splat)
                                    enemy.TakeDamage("strong", player.facingDir, player.beamForce * 0.05f * falloff, false, {0,0}, 1, 3);
                                    
                                    // PROCESSA O DANO AGORA!
                                    enemy.ResolveDamage(); 
                                    
                                    // Sobrescreve a velocidade padrão de hit com o vendaval letal do Force Push
                                    enemy.slideVelocity = (player.beamForce * 80.0f * falloff) * dirSign; 
                                }
                                hitSomething = true;
                            }
                        }
                        
                        if (hitSomething) {
                            screenShakeTimer = 0.2f; 
                            screenShakeIntensity = (player.beamForce / 100.0f) * 4.0f; 
                        }
                        
                        // Efeito visual de Vento Direcional
                        SpawnDirectionalWindBurst(blastCenter, player.beamForce / 5.0f, player.beamDir);
                    } else {
                        // KAMEHAMEHA VISÍVEL (Calcula a Hitbox Direcional)
                        screenShakeTimer = 0.1f; 
                        screenShakeIntensity = 10.0f; 

                        Vector2 p1 = player.pRHand->position;
                        Vector2 p2 = { p1.x + player.beamDir.x * 25000.0f, p1.y + player.beamDir.y * 25000.0f };
                        
                        if (enemy.isDead || enemy.isStunRagdoll) {
                            // Joga os ossos do Ragdoll pra longe se tocarem no raio
                            for (auto& p : enemy.world.particles) {
                                float num = fabs((p2.y - p1.y)*p->position.x - (p2.x - p1.x)*p->position.y + p2.x*p1.y - p2.y*p1.x);
                                float den = sqrt((p2.y - p1.y)*(p2.y - p1.y) + (p2.x - p1.x)*(p2.x - p1.x));
                                float dist = num / den;
                                float dot = (p->position.x - p1.x) * player.beamDir.x + (p->position.y - p1.y) * player.beamDir.y;

                                if (dist < player.beamThickness / 2.0f && dot > 0.0f) {
                                    p->position.x += player.beamDir.x * player.beamForce * dt; // dt
                                    p->position.y += player.beamDir.y * player.beamForce * dt; // dt
                                    
                                    float rx = p->position.x + (float)GetRandomValue(-100, 100);
                                    float ry = p->position.y + (float)GetRandomValue(-100, 100);
                                    Vector2 chaoticDir = {
                                        -player.beamDir.x + (float)GetRandomValue(-100, 100) / 100.0f,
                                        -player.beamDir.y + (float)GetRandomValue(-100, 100) / 100.0f
                                    };
                                    
                                    // PRESET 2 (SANGUE)
                                    SpawnSparks({rx, ry}, 6.0f, chaoticDir, 2);
                                }
                            }
                        } else {
                            float defTopY = enemy.pHead->position.y - 80.0f; 
                            float defBottomY = fmax(enemy.pLFoot->position.y, enemy.pRFoot->position.y) + 45.0f; 
                            float boxX = enemy.pPelvis->position.x;

                            float den = sqrt((p2.y - p1.y)*(p2.y - p1.y) + (p2.x - p1.x)*(p2.x - p1.x));
                            if (den == 0.0f) den = 0.0001f; 
                            
                            float minDist = 999999.0f;
                            float maxDot = -999999.0f;
                            float hitTopY = 999999.0f;
                            float hitBottomY = -999999.0f;
                            
                            int steps = 15;
                            for (int i = 0; i <= steps; i++) {
                                float pointY = defTopY + ((defBottomY - defTopY) * ((float)i / steps));
                                float num = fabs((p2.y - p1.y)*boxX - (p2.x - p1.x)*pointY + p2.x*p1.y - p2.y*p1.x);
                                float d = num / den;
                                if (d < minDist) minDist = d;

                                float dot = (boxX - p1.x) * player.beamDir.x + (pointY - p1.y) * player.beamDir.y;
                                if (dot > maxDot) maxDot = dot;
                                
                                if (d < (player.beamThickness / 2.0f) + 60.0f && dot > -120.0f) {
                                    if (pointY < hitTopY) hitTopY = pointY;
                                    if (pointY > hitBottomY) hitBottomY = pointY;
                                }
                            }

                            if (minDist < (player.beamThickness / 2.0f) + 60.0f && maxDot > -120.0f) {
                                float dmgPerFrame = (player.beamForce / 10.0f) * dt; // dt
                                
                                float randomX = boxX + (float)GetRandomValue(-100, 100);
                                float randomY = (float)GetRandomValue((int)hitTopY, (int)hitBottomY);
                                Vector2 beamImpactPos = { randomX, randomY };
                                
                                Vector2 chaoticDir = {
                                    -player.beamDir.x + (float)GetRandomValue(-100, 100) / 100.0f,
                                    -player.beamDir.y + (float)GetRandomValue(-100, 100) / 100.0f
                                };
                                
                                int sparkType = enemy.isBlocking ? 1 : 0; 
                                float sparkForce = enemy.isBlocking ? 50.0f : 15.0f; 
                                
                                SpawnSparks(beamImpactPos, sparkForce, chaoticDir, sparkType);
                                
                                enemy.health -= dmgPerFrame;
                                enemy.shield -= dmgPerFrame * 1.5f;
                                
                                // Converte o Atordoamento de Parry direto para Ragdoll Vivo ao receber o Raio!
                                if (enemy.isParryStunned || enemy.isDowned) {
                                    enemy.isParryStunned = false;
                                    enemy.isDowned = false;
                                    enemy.isStunRagdoll = true; 
                                    enemy.isDead = true; 
                                    enemy.isStandingUp = false;
                                    enemy.isBlocking = false;
                                    
                                    // Aplica um coice vetorial usando a direção e força do poder contínuo
                                    float forceMultiplier = (player.beamForce * 30.0f * dt) / 15.0f; 
                                    for (auto& p : enemy.world.particles) {
                                        p->previousPosition.x -= player.beamDir.x * forceMultiplier;
                                        p->previousPosition.y -= player.beamDir.y * forceMultiplier;
                                    }
                                }
                                else if (enemy.shield <= 0 && !enemy.shieldBroken) {
                                    // EXATO INSTANTE EM QUE O VIGOR ESTOURA
                                    enemy.shield = 0;
                                    enemy.shieldBroken = true;
                                    enemy.isBlocking = false;
                                    
                                    // Toca animação forte e arremessa o bot violentamente para trás
                                    enemy.TakeDamage("strong", player.facingDir, 0.0f, false, {0,0}, 1, 3);
                                    enemy.slideVelocity = player.beamForce * 2.0f * (player.beamDir.x > 0 ? 1.0f : -1.0f);
                                }
                                else if (!enemy.shieldBroken) {
                                    enemy.pPelvis->position.x += player.beamDir.x * 100.0f * dt; // dt
                                    enemy.isHit = true;
                                    enemy.stunTimer = 0.1f; 
                                    enemy.isAttacking = false;
                                    enemy.comboStep = 0;
                                    enemy.isParrying = false;
                                    
                                    std::string reactAnim = enemy.isBlocking ? "blockReactBeam" : "beamReact";
                                    
                                    if (enemy.animator.currentAnim != reactAnim) {
                                        enemy.animator.Play(reactAnim, enemy.currentAngles);
                                    } else {
                                        float maxF = enemy.animator.animations[reactAnim].size() - 1.01f;
                                        if (enemy.animator.currentFrame >= maxF) {
                                            enemy.animator.currentFrame = 0.0f;
                                        }
                                    }
                                }
                                
                                if (enemy.health <= 0) {
                                    enemy.health = 0;
                                    enemy.isDead = true;
                                    enemy.isKnockbackActive = true;
                                    enemy.knockbackTimer = 0.5f;
                                    enemy.knockbackDir = player.beamDir;
                                    enemy.knockbackForce = player.beamForce * 3.0f;
                                }
                            }
                        }
                    }
                } // <--- FIM DA FÍSICA DOS PODERES

                // INPUT DA TELECINESE (Agora Universal: Agarra, Puxa ou Atira no Vazio!)
                if (slowMoTimer > 0.0f && player.isExecuting && (player.executionType == 1 || player.executionType == 2)) {
                    if (player.executionTarget != nullptr) {
                        player.executionTarget->mass = player.targetOriginalMass;
                    }
                    player.isExecuting = false;
                    player.executionType = 0;
                    player.isChargingThrow = false;
                    player.throwChargeTimer = 0.0f;
                    player.animator.ForceBlend("idle", player.currentAngles);
                }

                float dist = fabs(player.pPelvis->position.x - enemy.pPelvis->position.x);
                
                std::shared_ptr<Particle> hoveredPart = nullptr;
                float minDist = 150.0f; 
                // Lê as peças do corpo SÓ se o inimigo for elegível para o agarrão
                if (enemy.isDead || enemy.isStunRagdoll || enemy.isParryStunned || enemy.isDowned) {
                    for (auto& p : enemy.world.particles) {
                        float dx = mouseWorld.x - p->position.x;
                        float dy = mouseWorld.y - p->position.y;
                        float d = sqrt(dx*dx + dy*dy);
                        if (d < minDist) { minDist = d; hoveredPart = p; }
                    }
                }
                
                bool tkInputPressed = IsKeyPressed(KEY_C) || IsMouseButtonPressed(MOUSE_BUTTON_SIDE) || IsMouseButtonPressed(MOUSE_BUTTON_EXTRA) || IsMouseButtonPressed(MOUSE_BUTTON_FORWARD) || IsMouseButtonPressed(MOUSE_BUTTON_BACK);
                bool tkInputDown = IsKeyDown(KEY_C) || IsMouseButtonDown(MOUSE_BUTTON_SIDE) || IsMouseButtonDown(MOUSE_BUTTON_EXTRA) || IsMouseButtonDown(MOUSE_BUTTON_FORWARD) || IsMouseButtonDown(MOUSE_BUTTON_BACK);

                if (!player.isExecuting && slowMoTimer <= 0.0f) {
                    bool grabbedSomething = false;

                    // 1. Tenta agarrar se o inimigo for alvo válido
                    if (enemy.isDead || enemy.isStunRagdoll || enemy.isParryStunned || enemy.isDowned) {
                        if (dist < 350.0f && tkInputPressed) { 
                            if (enemy.isParryStunned || enemy.isDowned) {
                                enemy.isParryStunned = false; enemy.isDowned = false;
                                enemy.isStunRagdoll = true; enemy.isDead = true; enemy.isStandingUp = false; enemy.isBlocking = false;
                            }
                            player.isExecuting = true;
                            player.executionType = 1;
                            player.isRemoteGrab = false; 
                            player.executionTimer = 0.0f;
                            player.executionTarget = enemy.pNeck; 
                            player.targetOriginalMass = enemy.pNeck->mass;
                            enemy.pNeck->mass = 500.0f;
                            player.isAttacking = false;
                            player.comboStep = 0;
                            player.inputBuffer = false;
                            grabbedSomething = true;
                        }
                        else if (dist >= 350.0f && hoveredPart != nullptr && tkInputPressed) { 
                            if (enemy.isParryStunned || enemy.isDowned) {
                                enemy.isParryStunned = false; enemy.isDowned = false;
                                enemy.isStunRagdoll = true; enemy.isDead = true; enemy.isStandingUp = false; enemy.isBlocking = false;
                            }
                            player.isExecuting = true;
                            player.executionType = 2; 
                            player.isRemoteGrab = true; 
                            player.executionTarget = hoveredPart; 
                            player.targetOriginalMass = hoveredPart->mass;
                            hoveredPart->mass = 500.0f;
                            player.isAttacking = false;
                            player.comboStep = 0;
                            player.inputBuffer = false;
                            grabbedSomething = true;
                        }
                    }

                    // 2. NOVO: Se não agarrou nada e apertou a tecla, entra na Mira Vazia!
                    if (!grabbedSomething && tkInputPressed && !player.isAttacking && !player.isBlocking) {
                        player.isExecuting = true;
                        player.executionType = 2; 
                        player.isRemoteGrab = true; 
                        player.executionTarget = nullptr; // VAZIO
                        player.isAttacking = false;
                        player.comboStep = 0;
                        player.inputBuffer = false;
                    }
                } 
                else if (player.isExecuting && (player.executionType == 1 || player.executionType == 2)) {
                    // Só cancela o estado se ele soltar a tecla C e NÃO estiver segurando o mouse para atirar
                    if (!tkInputDown && !player.isChargingThrow) { 
                        if (player.executionTarget != nullptr) {
                            player.executionTarget->mass = player.targetOriginalMass;
                        }
                        player.isExecuting = false;
                        player.executionType = 0;
                        player.isChargingThrow = false;
                        player.throwChargeTimer = 0.0f;
                        player.animator.ForceBlend("idle", player.currentAngles);
                    }
                }
                
                if (player.isExecuting && (player.executionType == 1 || player.executionType == 2)) {
                    // Solta o alvo automaticamente se ele ressuscitar de um Ragdoll
                    if (player.executionTarget != nullptr && !enemy.isDead && !enemy.isStunRagdoll) {
                        player.executionTarget->mass = player.targetOriginalMass;
                        player.isExecuting = false;
                        player.executionType = 0;
                        player.isChargingThrow = false;
                        player.throwChargeTimer = 0.0f;
                        player.animator.ForceBlend("idle", player.currentAngles);
                    }
                }

                // Colisão Física Orgânica do Ragdoll Morto contra o Lutador Vivo
                CollideRagdoll(enemy, player); // Se o Bot estiver morto, bate no Jogador
                CollideRagdoll(player, enemy); // Se o Jogador estiver morto, bate no Bot

                // CAMERA DE MORTE (Slow Motion Cinematográfico)
                static bool lastPlayerDead = false;
                static bool lastEnemyDead = false;
                
                bool currPlayerDead = player.isDead && !player.isStunRagdoll;
                bool currEnemyDead = enemy.isDead && !enemy.isStunRagdoll;

                // Detecta o exato frame em que a vida zerou (mudou de falso para verdadeiro)
                if ((currPlayerDead && !lastPlayerDead) || (currEnemyDead && !lastEnemyDead)) {
                    if (enableCinematicDeath) { // Só aciona o cinema se o jogador permitir!
                        slowMoTimer = 1.0f; 
                        slowMoTargetScale = 0.1f; 
                        
                        isDeathCamera = true; 
                        deathTargetId = (currPlayerDead && !lastPlayerDead) ? 0 : 1; 
                        screenShakeTimer = 0.0f; 
                    }
                }

                lastPlayerDead = currPlayerDead;
                lastEnemyDead = currEnemyDead;
            }
        } 
        else {
            // Se o menu estiver aberto, atualiza apenas a animação FK do preview
            player.animator.UpdateGameplay(GetFrameTime(), player.currentAngles);
            player.ApplyFK();
        }

        // Lógica de desenho de Hitbox no modo ANIMATOR
        Rectangle uiPanel = { 10, 10, 250, 620 }; // Aumente a altura do painel para caber os botões
        if (player.currentMode == Character::ANIMATOR && player.animator.editAnimType > 0 && creatingHitbox) {
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

        //Atualiza a matemática do Efeito de Parry
        for (auto it = parryEffects.begin(); it != parryEffects.end(); ) {
            it->currentSize += it->speed * dt; 
            it->opacity = 1.0f - (it->currentSize / it->maxSize); // Diminui opacidade conforme cresce
            
            if (it->opacity <= 0.0f) it = parryEffects.erase(it); // Morre quando some
            else ++it;
        }

        // Calcula o Shake 1 frame antes de aplicar na câmera 2D
        Vector2 originalCamTarget = camera.target;
        if (screenShakeTimer > 0.0f) {
            screenShakeTimer -= GetFrameTime();
            if (!isDeathCamera) { // SÓ TREME A TELA SE NÃO FOR A CÂMERA DE MORTE!
                camera.target.x += GetRandomValue(-100, 100) * 0.01f * screenShakeIntensity;
                camera.target.y += GetRandomValue(-100, 100) * 0.01f * screenShakeIntensity;
            }
        }

        // NOVO: Motor de Arrasto das Juntas do Bot (Animação Conjunta)
        if (player.currentMode == Character::ANIMATOR && player.animator.editAnimType == 2 && !player.animator.isPlaying) {
            
            std::vector<DragTarget> botTargets = {
                {enemy.pNeck, enemy.pPelvis, 0}, {enemy.pHead, enemy.pNeck, 1},
                {enemy.pLElbow, enemy.pNeck, 2}, {enemy.pLHand, enemy.pLElbow, 3},
                {enemy.pRElbow, enemy.pNeck, 4}, {enemy.pRHand, enemy.pRElbow, 5},
                {enemy.pLKnee, enemy.pPelvis, 6}, {enemy.pLFoot, enemy.pLKnee, 7},
                {enemy.pRKnee, enemy.pPelvis, 8}, {enemy.pRFoot, enemy.pRKnee, 9},
                {enemy.pLToe, enemy.pLFoot, 10}, {enemy.pRToe, enemy.pRFoot, 11},
                {enemy.pLFist, enemy.pLHand, 12}, {enemy.pRFist, enemy.pRHand, 13}
            };
            
            // Ativa a flag e descobre onde no Bot o mouse clicou
            if (player.draggedJointIndex == -99 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointCircle(mouseWorld, enemy.pPelvis->position, 25.0f)) {
                    player.draggedJointIndex = -3; // Flag para a pélvis do Bot
                } else {
                    for (auto& t : botTargets) {
                        if (CheckCollisionPointCircle(mouseWorld, t.p->position, 25.0f)) {
                            player.activeDragTarget = t;
                            player.draggedJointIndex = t.angleIdx + 100; // Flag para os ossos do Bot (100 a 113)
                            break;
                        }
                    }
                    if (player.draggedJointIndex == -99) player.draggedJointIndex = -1; // Clicou no vazio
                }
            }

            // Aplica o arrasto
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                if (player.draggedJointIndex == -3) {
                    enemy.pPelvis->position = mouseWorld;
                } else if (player.draggedJointIndex >= 100) {
                    int bIdx = player.draggedJointIndex - 100;
                    float dx = mouseWorld.x - player.activeDragTarget.parent->position.x;
                    float dy = mouseWorld.y - player.activeDragTarget.parent->position.y;
                    enemy.currentAngles[bIdx] = atan2(dy, dx);
                }
                enemy.ApplyFK();
            }
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);

        // Inicia o modo 2D: Tudo aqui sofre zoom e pan
        BeginMode2D(camera);
            DrawRectangle(-10000, (int)groundY, 20000, 1000 - (int)groundY, WHITE);
            DrawRectangle(-10000, (int)groundY-9000, 100, 10000 - (int)groundY, WHITE);
            DrawRectangle(10000, (int)groundY-9000, 100, 10000 - (int)groundY, WHITE);
            player.Draw();
            if (player.currentMode == Character::ANIMATOR && player.animator.editAnimType > 0) {
                if (drawingHitbox) {
                    DrawRectangle(fmin(dragStart.x, dragEnd.x), fmin(dragStart.y, dragEnd.y), fabs(dragEnd.x - dragStart.x), fabs(dragEnd.y - dragStart.y), Fade(RED, 0.5f));
                } else if (player.animator.editHitbox.width > 0) {
                    // SÓ DESENHA A HITBOX SE ESTIVER NO FRAME CONFIGURADO
                    if ((int)player.animator.currentFrame == player.animator.editActiveFrame) {
                        DrawRectangle(player.pPelvis->position.x + player.animator.editHitbox.x, player.pPelvis->position.y + player.animator.editHitbox.y, player.animator.editHitbox.width, player.animator.editHitbox.height, Fade(RED, 0.5f));
                    }
                }
                // desenho do vetor
                if (drawingVector) {
                    Vector2 center = { camera.target.x, camera.target.y }; // Centro imutável da câmera
                    
                    float dx = mouseWorld.x - center.x;
                    float dy = mouseWorld.y - center.y;
                    float angle = atan2(dy, dx);
                    
                    // Atualiza dinamicamente enquanto não aperta ENTER
                    player.animator.editDirection.x = cos(angle);
                    player.animator.editDirection.y = sin(angle);
                    
                    float arrowLen = 120.0f;
                    Vector2 endArrow = { center.x + cos(angle)*arrowLen, center.y + sin(angle)*arrowLen };
                    
                    DrawLineEx(center, endArrow, 8.0f, MAGENTA); // Base da Flecha
                    
                    // Ponta da Flecha (Triângulo Escaleno)
                    Vector2 p1 = endArrow;
                    Vector2 p2 = { endArrow.x - cos(angle - 0.5f)*35.0f, endArrow.y - sin(angle - 0.5f)*35.0f };
                    Vector2 p3 = { endArrow.x - cos(angle + 0.5f)*35.0f, endArrow.y - sin(angle + 0.5f)*35.0f };
                    DrawTriangle(p1, p2, p3, MAGENTA);
                    DrawTriangle(p1, p3, p2, MAGENTA); // Fundo
                    
                    if (IsKeyPressed(KEY_ENTER)) drawingVector = false;
                }
            }

            if (botActive || enemy.isDead) enemy.Draw();

            if (player.currentMode == Character::GAMEPLAY) {
                player.DrawStatusBars();
                if (botActive || enemy.isDead) enemy.DrawStatusBars();
            }

            player.DrawStatusBars();
            if (botActive) enemy.DrawStatusBars();

            // Sinal H e V acima do inimigo em Ragdoll, Atordoado ou Deitado
            if (player.currentMode == Character::GAMEPLAY && (enemy.isDead || enemy.isParryStunned || enemy.isDowned) && !player.isExecuting) {
                float dist = fabs(player.pPelvis->position.x - enemy.pPelvis->position.x);
                
                std::shared_ptr<Particle> hoveredPart = nullptr;
                float minDist = 150.0f;
                for (auto& p : enemy.world.particles) {
                    float dx = mouseWorld.x - p->position.x;
                    float dy = mouseWorld.y - p->position.y;
                    float d = sqrt(dx*dx + dy*dy);
                    if (d < minDist) { minDist = d; hoveredPart = p; }
                }

                if (dist < 350.0f) { // ALTERADO PARA 350
                    if (enemy.isDead) DrawText("H", enemy.pPelvis->position.x - 15, enemy.pHead->position.y - 60, 40, RED);
                    
                    // Apenas desenha o aviso F, a lógica da tecla já ocorreu lá em cima
                    if (!enemy.isDead && (enemy.isParryStunned || enemy.isDowned)) {
                        DrawText("V (EXECUTAR)", enemy.pPelvis->position.x - 60, enemy.pHead->position.y - 90, 30, YELLOW);
                    }
                } else if (dist >= 350.0f && hoveredPart != nullptr) { // ALTERADO PARA 350
                    DrawText("H (PUXAR)", hoveredPart->position.x - 50, hoveredPart->position.y - 40, 30, PURPLE);
                }
            }

            // Desenha Efeito de Impacto DBZ (Rosc)
            for (const auto& vfx : parryEffects) {
                // A espessura do anel diminui conforme a opacidade cai, criando um efeito de dissipação
                float thickness = 15.0f * vfx.opacity; 
                float innerRadius = fmax(0.0f, vfx.currentSize - thickness);
                
                // Anel exterior (Aura amarela translúcida)
                DrawRing(vfx.position, fmax(0.0f, innerRadius - 5.0f), vfx.currentSize + 5.0f, 0.0f, 360.0f, 36, Fade(YELLOW, vfx.opacity * 0.6f));
                
                // Anel interior (Onda de choque branca afiada)
                DrawRing(vfx.position, innerRadius, vfx.currentSize, 0.0f, 360.0f, 36, Fade(WHITE, vfx.opacity));
            }

            // Física Híbrida de Partículas (Faíscas caem, Vento flutua esticado)
            for (auto it = sparks.begin(); it != sparks.end(); ) {
                it->position.x += it->velocity.x * dt; 
                it->position.y += it->velocity.y * dt;
                
                if (it->hasGravity) {
                    it->velocity.y += 2000.0f * dt; 
                } 

                it->lifespan -= dt;
                
                if (it->lifespan <= 0.0f) {
                    it = sparks.erase(it);
                } else {
                    float alpha = it->lifespan / it->maxLifespan;
                    
                    if (it->isWind) {
                        // DESENHO DO VENTO: O multiplicador (0.05f) projeta a cauda matematicamente no contra-fluxo da inércia
                        Vector2 tail = { it->position.x - it->velocity.x * 0.05f, it->position.y - it->velocity.y * 0.05f };
                        DrawLineEx(it->position, tail, it->size, Fade(it->color, alpha * 0.7f));
                    } else {
                        // FAÍSCA COMUM: Partícula sólida
                        DrawRectangle(it->position.x, it->position.y, it->size, it->size, Fade(it->color, alpha));
                    }
                    ++it;
                }
            }

            // Debug de Hitbox e Hurtbox no GAMEPLAY
            if (player.currentMode == Character::GAMEPLAY && showHitboxes) {
                auto DrawDebugBoxes = [](Character& c) {
                    // Hurtbox do Corpo (Azul) - A mesma usada para receber dano
                    float defTopY = c.pHead->position.y - 80.0f; 
                    float defBottomY = fmax(c.pLFoot->position.y, c.pRFoot->position.y) + 45.0f;
                    
                    DrawRectangleLinesEx({ c.pPelvis->position.x - 60.0f, defTopY, 120.0f, defBottomY - defTopY }, 2, BLUE);
                    
                    // INDICADOR VISUAL DA JANELA DE PARRY
                    if (c.parryWindow > 0.0f) {
                        DrawText("PARRY", c.pPelvis->position.x - 30, defTopY - 25, 20, GREEN);
                    }
                    
                    // Painel de pensamento da IA
                    if (c.blockTimer > 0.0f) {
                        DrawText(TextFormat("BLOCK: %.2fs", c.blockTimer), c.pPelvis->position.x - 50, defTopY - 45, 20, ORANGE);
                    }
                    if (c.wantsToParry) {
                        DrawText("VAI DAR PARRY!", c.pPelvis->position.x - 65, defTopY - 65, 20, RED);
                    }
                    
                    // Hitbox de Ataque (Vermelha) - Renderiza no Active Frame correto
                    if ((c.isAttacking || c.executionType == 5) && c.animator.animType[c.animator.currentAnim] > 0) {
                        float activeFrame = c.animator.animActiveFrame[c.animator.currentAnim];
                        if (activeFrame <= 0.0f && !c.animator.animations[c.animator.currentAnim].empty()) {
                            activeFrame = c.animator.animations[c.animator.currentAnim].size() - 1.0f;
                        }

                        if (c.animator.currentFrame >= activeFrame) {
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

            // Barra de Arremesso
            if (player.currentMode == Character::GAMEPLAY && player.isExecuting && player.isChargingThrow) {
                float barWidth = 100.0f;
                float barHeight = 12.0f;
                float fillWidth = (player.throwChargeTimer / 3.0f) * barWidth;
                
                float barX = player.pPelvis->position.x - barWidth / 2.0f;
                float barY = player.pHead->position.y - 40.0f;
                
                DrawRectangle(barX, barY, barWidth, barHeight, Fade(GRAY, 0.8f));
                DrawRectangle(barX, barY, fillWidth, barHeight, RED);
                DrawRectangleLines(barX, barY, barWidth, barHeight, BLACK);
            }

            // EFEITOS VISUAIS DO KAMEHAMEHA (Carregamento e Disparo)
            if (player.currentMode == Character::GAMEPLAY) {
                
                // 3 pulsos caóticos independentes (Fazem as camadas tremerem separadas)
                float p1 = 1.0f + (GetRandomValue(-15, 15) / 100.0f); // Tremor suave (Aura)
                float p2 = 1.0f + (GetRandomValue(-20, 20) / 100.0f); // Tremor médio (Meio)
                float p3 = 1.0f + (GetRandomValue(-30, 30) / 100.0f); // Tremor violento (Núcleo)

                // 1. SUCÇÃO DO SOPRO TELECINÉTICO (ExecutionType == 2)
                if (player.isExecuting && player.isChargingThrow && player.executionType == 2) {
                    float chargeRatio = player.throwChargeTimer / 3.0f;
                    
                    float dirX = mouseWorld.x - player.pRHand->position.x;
                    float dirY = mouseWorld.y - player.pRHand->position.y;
                    float dist = sqrt(dirX*dirX + dirY*dirY);
                    if (dist > 0.01f) { dirX /= dist; dirY /= dist; }
                    else { dirX = player.facingDir; dirY = 0.0f; }
                    
                    float baseAngle = atan2(dirY, dirX);
                    int windCount = GetRandomValue(1, (int)(chargeRatio * 5.0f) + 1); 
                    
                    for (int i = 0; i < windCount; i++) {
                        Spark s;
                        float spread = 45.0f; // Cone de 90 graus
                        float angle = baseAngle + (float)GetRandomValue(-(int)spread, (int)spread) * DEG2RAD;
                        
                        float spawnDist = (float)GetRandomValue(300, 800); 
                        s.position.x = player.pRHand->position.x + cos(angle) * spawnDist;
                        s.position.y = player.pRHand->position.y + sin(angle) * spawnDist;
                        
                        float speed = (float)GetRandomValue(1500, 3500); 
                        s.velocity.x = cos(angle + PI) * speed; // Aponta pra mão (+PI)
                        s.velocity.y = sin(angle + PI) * speed;
                        
                        s.maxLifespan = spawnDist / speed; 
                        s.lifespan = s.maxLifespan;
                        s.size = (float)GetRandomValue(2, 5); 
                        s.color = Fade(WHITE, 0.8f); 
                        s.hasGravity = false;
                        s.isWind = true; 
                        
                        sparks.push_back(s);
                    }
                }

                // 2. ESFERA E SUCÇÃO DO KAMEHAMEHA DURANTE O CARREGAMENTO (ExecutionType == 3)
                if (player.isExecuting && player.isChargingThrow && player.executionType == 3) {
                    float chargeRatio = player.throwChargeTimer / 3.0f;
                    float sphereSize = fmax(1.0f, 120.0f * chargeRatio); 

                    // Sucção de energia em 360 graus
                    int energyCount = GetRandomValue(1, (int)(chargeRatio * 6.0f) + 1);
                    for (int i = 0; i < energyCount; i++) {
                        Spark s;
                        float angle = (float)GetRandomValue(0, 360) * DEG2RAD; 
                        
                        // O raio de nascimento da energia aumenta conforme carrega
                        float maxSpawnRadius = 300.0f + (chargeRatio * 500.0f);
                        float spawnDist = (float)GetRandomValue((int)sphereSize + 20, (int)maxSpawnRadius); 
                        
                        s.position.x = player.pRHand->position.x + cos(angle) * spawnDist;
                        s.position.y = player.pRHand->position.y + sin(angle) * spawnDist;
                        
                        float speed = (float)GetRandomValue(1000, 2500); 
                        s.velocity.x = cos(angle + PI) * speed; 
                        s.velocity.y = sin(angle + PI) * speed;
                        
                        s.maxLifespan = spawnDist / speed; 
                        s.lifespan = s.maxLifespan;
                        s.size = (float)GetRandomValue(3, 7); 
                        
                        // Três cores diferentes do DBZ
                        int colorPicker = GetRandomValue(0, 2);
                        if (colorPicker == 0) s.color = Fade(WHITE, 0.9f);
                        else if (colorPicker == 1) s.color = Fade(BLUE, 0.9f);
                        else s.color = Fade(SKYBLUE, 0.9f);
                        
                        s.hasGravity = false;
                        s.isWind = true; // Rastro esticado também!
                        
                        sparks.push_back(s);
                    }

                    BeginBlendMode(BLEND_ADDITIVE);
                    
                    DrawCircleGradient(player.pRHand->position, sphereSize * 1.5f * p1, Fade(BLUE, 0.5f), Fade(BLUE, 0.0f));
                    DrawCircleGradient(player.pRHand->position, sphereSize * 0.8f * p2, Fade(SKYBLUE, 0.8f), Fade(SKYBLUE, 0.0f));
                    DrawCircleGradient(player.pRHand->position, sphereSize * 0.4f * p3, WHITE, Fade(WHITE, 0.0f));
                    
                    EndBlendMode();
                }
                
                // 3. O RAIO E A ESFERA DURANTE O DISPARO
                if (player.beamActive && player.isVisibleBeam) {
                    Vector2 startPos = player.pRHand->position;
                    Vector2 endPos = { startPos.x + player.beamDir.x * 25000.0f, startPos.y + player.beamDir.y * 25000.0f };
                    
                    float fadeOut = fmax(0.0f, player.beamTimer / 1.0f); 
                    float thick = player.beamThickness * fadeOut;
                    
                    // A esfera regride de tamanho (Retém ~70% do tamanho máximo do carregamento)
                    // beamThickness máxima é 250. Multiplicar por 0.35 dá ~87.5 no pico.
                    float sphereSize = (player.beamThickness * 0.35f) * fadeOut; 

                    BeginBlendMode(BLEND_ADDITIVE);

                    // O FEIXE DE ENERGIA (Desenhado primeiro para ficar no fundo)
                    DrawLineEx(startPos, endPos, thick * 1.5f * p1, Fade(BLUE, 0.3f * fadeOut));
                    DrawLineEx(startPos, endPos, thick * 1.1f * ((p1+p2)/2.0f), Fade(BLUE, 0.5f * fadeOut)); // Transição 1
                    DrawLineEx(startPos, endPos, thick * 0.8f * p2, Fade(SKYBLUE, 0.7f * fadeOut));
                    DrawLineEx(startPos, endPos, thick * 0.5f * ((p2+p3)/2.0f), Fade(SKYBLUE, 0.9f * fadeOut)); // Transição 2
                    DrawLineEx(startPos, endPos, thick * 0.3f * p3, Fade(WHITE, 1.0f * fadeOut)); // Núcleo Letal
                    
                    // ARREDONDAMENTO DA BASE DO RAIO (Esconde a ponta reta/quadrada do DrawLineEx)
                    // Dividimos por 2 porque o DrawCircle pede o Raio, enquanto o DrawLine pede a Espessura (Diâmetro)
                    DrawCircleV(startPos, (thick * 0.8f * p2) / 2.0f, Fade(SKYBLUE, 0.9f * fadeOut));
                    DrawCircleV(startPos, (thick * 0.3f * p3) / 2.0f, Fade(WHITE, 1.0f * fadeOut));

                    // ESFERA NA BASE (O núcleo reduzido que está ejetando o poder)
                    DrawCircleGradient(startPos, sphereSize * 3.5f * p1, Fade(BLUE, 0.5f * fadeOut), Fade(BLUE, 0.0f));
                    DrawCircleGradient(startPos, sphereSize * 2.8f * p2, Fade(SKYBLUE, 0.8f * fadeOut), Fade(SKYBLUE, 0.0f));
                    DrawCircleGradient(startPos, sphereSize * 1.4f * p3, Fade(WHITE, 1.0f * fadeOut), Fade(WHITE, 0.0f));

                    EndBlendMode();
                }
            }

        EndMode2D();
        camera.target = originalCamTarget;

        if (comboMenu.isOpen) {
            comboMenu.UpdateAndDraw(player, enemy);
        }

        if (!comboMenu.isOpen && (showUIMenu || player.currentMode != Character::GAMEPLAY)) {
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
                enemy.currentMode = player.currentMode; // SINCRONIZA O BOT!
                if (player.currentMode == Character::ANIMATOR || player.currentMode == Character::GAMEPLAY) {
                    player.ApplyFK();
                    enemy.ApplyFK();
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
                    player.animator.AddPose(player.currentAngles, player.pPelvis->position, enemy.currentAngles, enemy.pPelvis->position);
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
                const char* typeTextMeta = player.animator.editAnimType == 0 ? "TIPO: NORMAL" :
                                          (player.animator.editAnimType == 1 ? "TIPO: GOLPE" : "TIPO: CONJUNTA");
                if (CheckCollisionPointRec(GetMousePosition(), btnAttackMode) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    player.animator.editAnimType = (player.animator.editAnimType + 1) % 3;
                }
                DrawRectangleRec(btnAttackMode, player.animator.editAnimType > 0 ? GREEN : LIGHTGRAY);
                DrawRectangleLinesEx(btnAttackMode, 2, BLACK);
                DrawText(typeTextMeta, btnAttackMode.x + 10, btnAttackMode.y + 10, 20, BLACK);

                float offsetY = 0.0f;

                if (player.animator.editAnimType > 0) {
                    Rectangle btnFrameInput = { 20, 510, 200, 30 };
                    if (CheckCollisionPointRec(GetMousePosition(), btnFrameInput) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
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
                    if (CheckCollisionPointRec(GetMousePosition(), btnHitbox) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) creatingHitbox = !creatingHitbox;
                    DrawRectangleRec(btnHitbox, creatingHitbox ? ORANGE : GRAY);
                    DrawText(creatingHitbox ? "APLICAR HITBOX" : "CRIAR HITBOX", btnHitbox.x + 10, btnHitbox.y + 5, 20, BLACK);

                    Rectangle btnHitType = { 20, 590, 200, 30 };
                    if (CheckCollisionPointRec(GetMousePosition(), btnHitType) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        player.animator.editHitType = (player.animator.editHitType + 1) % 5;
                    }
                    const char* typeText = player.animator.editHitType == 0 ? "REACAO: ALTA" :
                                          (player.animator.editHitType == 1 ? "REACAO: MEDIA" :
                                          (player.animator.editHitType == 2 ? "REACAO: BAIXA" : 
                                          (player.animator.editHitType == 3 ? "REACAO: FORTE" : "REACAO: CUSTOM")));
                    DrawRectangleRec(btnHitType, PURPLE);
                    DrawRectangleLinesEx(btnHitType, 2, BLACK);
                    DrawText(typeText, btnHitType.x + 10, btnHitType.y + 5, 20, WHITE);
                    
                    if (player.animator.editHitType == 4) {
                        offsetY = 40.0f;
                        Rectangle btnReactInput = { 20, 630, 200, 30 };
                        if (CheckCollisionPointRec(GetMousePosition(), btnReactInput) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            typingReaction = true; reactionText = "";
                        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) typingReaction = false;

                        if (typingReaction) {
                            int key = GetCharPressed();
                            while (key > 0) {
                                if ((key >= 32) && (key <= 125)) reactionText += (char)key;
                                key = GetCharPressed();
                            }
                            if (IsKeyPressed(KEY_BACKSPACE) && reactionText.length() > 0) reactionText.pop_back();
                            if (IsKeyPressed(KEY_ENTER)) {
                                player.animator.editCustomReaction = reactionText.empty() ? "none" : reactionText;
                                typingReaction = false;
                            }
                        }
                        DrawRectangleRec(btnReactInput, typingReaction ? LIGHTGRAY : GRAY);
                        DrawText(TextFormat("Custom: %s", player.animator.editCustomReaction.c_str()), btnReactInput.x + 5, btnReactInput.y + 5, 10, BLACK);
                        if (typingReaction) DrawText(reactionText.c_str(), btnReactInput.x + 130, btnReactInput.y + 5, 15, RED);
                    }

                    DrawText(TextFormat("Dano da Skill: %.1f", player.animator.editDamage), 20, 640.0f + offsetY, 15, BLACK);
                    Rectangle btnDmgMinus = { 170.0f, 635.0f + offsetY, 20.0f, 25.0f };
                    Rectangle btnDmgPlus = { 200.0f, 635.0f + offsetY, 20.0f, 25.0f };
                    if (CheckCollisionPointRec(GetMousePosition(), btnDmgMinus) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) player.animator.editDamage -= 1.0f;
                    if (CheckCollisionPointRec(GetMousePosition(), btnDmgPlus) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) player.animator.editDamage += 1.0f;
                    DrawRectangleRec(btnDmgMinus, GRAY); DrawText("-", 175, (int)(638.0f + offsetY), 20, WHITE);
                    DrawRectangleRec(btnDmgPlus, GRAY); DrawText("+", 203, (int)(638.0f + offsetY), 20, WHITE);
                    
                    Rectangle btnVector = { 20.0f, 675.0f + offsetY, 200.0f, 30.0f };
                    if (CheckCollisionPointRec(GetMousePosition(), btnVector) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        drawingVector = !drawingVector;
                    }
                    DrawRectangleRec(btnVector, drawingVector ? ORANGE : GRAY);
                    DrawText(drawingVector ? "APLICAR VETOR" : "CRIAR VETOR", btnVector.x + 10, btnVector.y + 5, 20, BLACK);

                    Rectangle btnTarget = { 20.0f, 715.0f + offsetY, 200.0f, 30.0f };
                    if (CheckCollisionPointRec(GetMousePosition(), btnTarget) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        player.animator.editTargetPart = (player.animator.editTargetPart + 1) % 3;
                    }
                    const char* tgtText = player.animator.editTargetPart == 0 ? "FORCA: CABECA" :
                                         (player.animator.editTargetPart == 1 ? "FORCA: TRONCO" : "FORCA: PERNA DIR");
                    DrawRectangleRec(btnTarget, BLUE);
                    DrawRectangleLinesEx(btnTarget, 2, BLACK);
                    DrawText(tgtText, btnTarget.x + 10, btnTarget.y + 5, 20, WHITE);

                    // NOVO: Botão de Câmera da Cutscene
                    Rectangle btnCamFollow = { 20.0f, 755.0f + offsetY, 200.0f, 30.0f };
                    if (CheckCollisionPointRec(GetMousePosition(), btnCamFollow) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        player.animator.editCamFollow = player.animator.editCamFollow == 1 ? 0 : 1;
                    }
                    const char* camText = player.animator.editCamFollow == 1 ? "CAMERA: SEGUE" : "CAMERA: TRAVADA";
                    DrawRectangleRec(btnCamFollow, player.animator.editCamFollow == 1 ? DARKGREEN : MAROON);
                    DrawRectangleLinesEx(btnCamFollow, 2, BLACK);
                    DrawText(camText, btnCamFollow.x + 10, btnCamFollow.y + 5, 20, WHITE);

                    offsetY += 160.0f; // Ajustado para dar espaço ao novo botão
                }
                
                Rectangle btnInitial = { 20.0f, 640.0f + offsetY, 200.0f, 30.0f };
                DrawButton(btnInitial, "Ver Posicao Inicial", player.showInitialFrame);

                if (!player.animator.isPlaying && !player.animator.track.empty()) {
                    Rectangle btnPrev = { 20.0f, 680.0f + offsetY, 95.0f, 30.0f };
                    Rectangle btnNext = { 125.0f, 680.0f + offsetY, 95.0f, 30.0f };
                    
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
            } // Fecha o Animator

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

                Rectangle btnBotDead = { 20, 365, 200, 40 };
                DrawButton(btnBotDead, enemy.isDead ? "Bot: MORTO (Ragdoll)" : "Bot: VIVO", enemy.isDead);

                Rectangle btnRestore = { 20, 415, 200, 40 };
                bool hoverRestore = CheckCollisionPointRec(GetMousePosition(), btnRestore);
                if (hoverRestore && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    player.health = player.maxHealth; player.shield = player.maxShield; player.shieldBroken = false; player.isDead = false;
                    enemy.health = enemy.maxHealth; enemy.shield = enemy.maxShield; enemy.shieldBroken = false; enemy.isDead = false;
                }
                DrawRectangleRec(btnRestore, hoverRestore ? LIGHTGRAY : GRAY);
                DrawRectangleLinesEx(btnRestore, 2, BLACK);
                DrawText("Restaurar Vidas", btnRestore.x + 10, btnRestore.y + 10, 20, BLACK);

                // Ligar/Desligar Defesa
                Rectangle btnBotBlock = { 20, 465, 200, 40 };
                DrawButton(btnBotBlock, botBlocking ? "Bot: DEFENDENDO" : "Bot: SEM DEFESA", botBlocking);

                // NOVO: Ligar/Desligar Câmera Cinematográfica
                Rectangle btnCinematic = { 20, 515, 200, 40 };
                DrawButton(btnCinematic, enableCinematicDeath ? "Camera Morte: ON" : "Camera Morte: OFF", enableCinematicDeath);
            }
        } // Fecha o comboMenu.isOpen

        EndDrawing();
    } // Fecha o while principal

    CloseWindow();
    return 0;
} // Fecha a main