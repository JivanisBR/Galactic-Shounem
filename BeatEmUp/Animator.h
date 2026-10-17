#pragma once
#include "raylib.h"     // Usa as estruturas Vector2 e Rectangle do motor gráfico
#include <vector>       // Arrays dinâmicos para armazenar as listas de poses/keyframes
#include <math.h>       // Operações trigonométricas, como fmod para interpolação
#include <fstream>      // Criação de fluxos de entrada/saída de arquivos .txt no disco
#include <string>       // Manipulação de textos e nomes de animações
#include <map>          // Dicionários atrelando os metadados aos nomes dos golpes
#include <sstream>      // Leitura e fragmentação dos dados em uma linha de texto (ex: META)

struct Pose {
    float angles[14];
    Vector2 rootPos;
    float targetAngles[14];
    Vector2 targetRootPos;
};

class Animator {
public:
    bool animFinished = false;
    std::map<std::string, bool> animLooping; 
    
    // MAPAS DE METADADOS
    std::map<std::string, int> animType; //  0=Comum, 1=Golpe, 2=Conjunta
    std::map<std::string, int> animActiveFrame;
    std::map<std::string, Rectangle> animHitbox;
    std::map<std::string, float> animPower;
    std::map<std::string, Vector2> animDirection;
    std::map<std::string, int> animHitType;
    std::map<std::string, std::string> animCustomReaction;
    std::map<std::string, int> animTargetPart;
    std::map<std::string, int> animCamFollow; // NOVO

    // VARIÁVEIS DE EDIÇÃO (MODO ANIMATOR)
    int editAnimType = 0; 
    int editActiveFrame = 0;
    Rectangle editHitbox = {0, 0, 0, 0};
    float editPower = 10.0f;
    Vector2 editDirection = {1.0f, 0.0f};
    int editHitType = 0;
    std::string editCustomReaction = "none";
    int editTargetPart = 1; // 0 = Cabeça, 1 = Tronco, 2 = Perna Direita
    int editCamFollow = 1; // 1 = Segue, 0 = Travada

    std::vector<Pose> track;
    bool isPlaying = false;
    float currentFrame = 0.0f;
    float playSpeed = 10.0f; 

    std::map<std::string, std::vector<Pose>> animations;
    std::string currentAnim = "";
    std::string nextAnim = "";
    float playDirection = 1.0f; 
    bool isBlending = false;
    float blendTimer = 0.0f;
    const float BLEND_DURATION = 0.15f; 
    float savedAngles[14];

    std::map<std::string, float> animDamage;
    float editDamage = 10.0f;

    float LerpAngle(float start, float end, float t) {
        float difference = fmod(end - start, PI * 2.0f);
        float distance = fmod(2.0f * difference, PI * 2.0f) - difference;
        return start + distance * t;
    }

    void SaveToFile(const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return;
        
        // Salva editAnimType em vez do booleano
        file << "META " << editAnimType << " " << editActiveFrame << " " 
             << editHitbox.x << " " << editHitbox.y << " " << editHitbox.width << " " << editHitbox.height << " "
             << editPower << " " << editDirection.x << " " << editDirection.y << " " << editHitType << " " << editCustomReaction << " " << editTargetPart << " " << editCamFollow << "\n";
        file << "DMG: " << editDamage << "\n";
        
        for (const auto& p : track) {
            file << p.rootPos.x << " " << p.rootPos.y;
            for (int i = 0; i < 14; i++) file << " " << p.angles[i];
            
            //  Salva os dados do Bot (mesmo que sejam 0 se não for conjunta)
            file << " " << p.targetRootPos.x << " " << p.targetRootPos.y;
            for (int i = 0; i < 14; i++) file << " " << p.targetAngles[i];
            
            file << "\n";
        }
        file.close();
    }

    void LoadFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return;
        track.clear();
        currentFrame = 0.0f;
        editAnimType = 0; editActiveFrame = 0; editHitbox = {0,0,0,0}; editPower = 10.0f; editDirection = {1.0f, 0.0f};

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::istringstream iss(line);
            std::string firstToken;
            iss >> firstToken;

            if (firstToken == "DMG:") { iss >> editDamage; continue; }
            
            if (firstToken == "META") {
                iss >> editAnimType >> editActiveFrame >> editHitbox.x >> editHitbox.y >> editHitbox.width >> editHitbox.height >> editPower >> editDirection.x >> editDirection.y;
                if (iss >> editHitType) {
                    if (iss >> editCustomReaction) {
                        if (iss >> editTargetPart) {
                            if (!(iss >> editCamFollow)) editCamFollow = 1;
                        } else { editTargetPart = 1; editCamFollow = 1; }
                    } else { editCustomReaction = "none"; editTargetPart = 1; editCamFollow = 1; }
                } else { editHitType = 0; editCustomReaction = "none"; editTargetPart = 1; editCamFollow = 1; }
            }
            else {
                Pose p;
                p.rootPos.x = std::stof(firstToken);
                iss >> p.rootPos.y;
                for (int i = 0; i < 14; i++) iss >> p.angles[i];
                
                //  Lê os dados do Bot (se o arquivo antigo não tiver, preenche com 0)
                if (iss >> p.targetRootPos.x) {
                    iss >> p.targetRootPos.y;
                    for (int i = 0; i < 14; i++) iss >> p.targetAngles[i];
                } else {
                    p.targetRootPos = {0.0f, 0.0f};
                    for (int i = 0; i < 14; i++) p.targetAngles[i] = 0.0f;
                }
                track.push_back(p);
            }
        }
        file.close();
    }

    void LoadToLibrary(const std::string& path, const std::string& name, bool isLoop = true) {
        std::ifstream file(path);
        if (!file.is_open()) return;
        std::vector<Pose> newAnim;
        
        int aType = 0; int actFrame = 0; Rectangle hb = {0,0,0,0}; float pwr = 10.0f; Vector2 dir = {1.0f, 0.0f}; int hType = 0; std::string cReact = "none"; int tPart = 1; int cFollow = 1;
        float cDmg = 10.0f;
        std::string line;

        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::istringstream iss(line);
            std::string firstToken;
            iss >> firstToken;

            if (firstToken == "DMG:") { iss >> cDmg; continue; }
            
            if (firstToken == "META") {
                iss >> aType >> actFrame >> hb.x >> hb.y >> hb.width >> hb.height >> pwr >> dir.x >> dir.y;
                if (iss >> hType) {
                    if (iss >> cReact) {
                        if (iss >> tPart) {
                            if (!(iss >> cFollow)) cFollow = 1;
                        } else { tPart = 1; cFollow = 1; }
                    } else { cReact = "none"; tPart = 1; cFollow = 1; }
                } else { hType = 0; cReact = "none"; tPart = 1; cFollow = 1; }
            }
            else {
                Pose p;
                p.rootPos.x = std::stof(firstToken);
                iss >> p.rootPos.y;
                for (int i = 0; i < 14; i++) iss >> p.angles[i];
                
                //  Lê os dados do Bot
                if (iss >> p.targetRootPos.x) {
                    iss >> p.targetRootPos.y;
                    for (int i = 0; i < 14; i++) iss >> p.targetAngles[i];
                } else {
                    p.targetRootPos = {0.0f, 0.0f};
                    for (int i = 0; i < 14; i++) p.targetAngles[i] = 0.0f;
                }
                newAnim.push_back(p);
            }
        }
        if (!newAnim.empty()) {
            animations[name] = newAnim;
            animLooping[name] = isLoop;
            animType[name] = aType; //  Registra o tipo da animação na memória
            animActiveFrame[name] = actFrame;
            animHitbox[name] = hb;
            animPower[name] = pwr;
            animDirection[name] = dir;
            animHitType[name] = hType;
            animCustomReaction[name] = cReact;
        }
        animDamage[name] = cDmg;
        animTargetPart[name] = tPart;
        animCamFollow[name] = cFollow;
        file.close();
    }

    void AddPose(float* currentAngles, Vector2 root, float* targetAngles, Vector2 targetRoot) {
        Pose p;
        for(int i = 0; i < 14; i++) {
            p.angles[i] = currentAngles[i];
            p.targetAngles[i] = targetAngles[i];
        }
        p.rootPos = root;
        p.targetRootPos = targetRoot;
        track.push_back(p);
    }

    void Update(float dt, float* currentAngles, Vector2& currentRoot) {
        if (!isPlaying || track.empty()) return;
        
        currentFrame += playSpeed * dt;
        
        // Travas de segurança blindadas usando while
        while (currentFrame >= track.size()) currentFrame -= track.size(); 
        while (currentFrame < 0.0f) currentFrame += track.size();

        int idx1 = (int)currentFrame;
        int idx2 = (idx1 + 1) % track.size();
        float blend = currentFrame - idx1;

        currentRoot.x = track[idx1].rootPos.x + (track[idx2].rootPos.x - track[idx1].rootPos.x) * blend;
        currentRoot.y = track[idx1].rootPos.y + (track[idx2].rootPos.y - track[idx1].rootPos.y) * blend;

        // Loop corrigido para 14 membros
        for(int i = 0; i < 14; i++) {
            float a1 = track[idx1].angles[i];
            float a2 = track[idx2].angles[i];
            float diff = a2 - a1;
            while (diff < -PI) diff += 2 * PI;
            while (diff > PI) diff -= 2 * PI;
            currentAngles[i] = a1 + diff * blend;
        }
    }

    void Play(const std::string& name, float* currentAngles) {
        // 1. Trava anti-crash: Ignora nomes inválidos ou vetores fantasmas (vazios)
        if (animations.find(name) == animations.end() || animations[name].empty()) return;
        
        // 2. Trava de Congelamento:
        if (isBlending) { 
            if (nextAnim == name) return; 
        } 
        else { 
            // A MÁGICA ESTÁ AQUI: 
            // Previne congelamento de estados contínuos (walking, idle, block), 
            // mas libera a repetição do Play se for um ataque (para permitir repetir o mesmo soco num combo)
            if (currentAnim == name && animType[name] == 0) return;
        }

        if (currentAnim == "") { 
            currentAnim = name;
            currentFrame = 0.0f;
            return;
        }

        nextAnim = name;
        isBlending = true;
        blendTimer = 0.0f;
        animFinished = false;
        for (int i = 0; i < 14; i++) savedAngles[i] = currentAngles[i];
    }

    // Força uma interpolação suave de qualquer pose física/IK de volta para uma animação real
    void ForceBlend(const std::string& targetAnim, float* currentAngles) {
        if (animations.find(targetAnim) == animations.end() || animations[targetAnim].empty()) return;
        nextAnim = targetAnim;
        isBlending = true;
        blendTimer = 0.0f;
        animFinished = false;
        // Salva a pose exata em que o braço (ou corpo) estava pelo IK para fazer o blend
        for (int i = 0; i < 14; i++) savedAngles[i] = currentAngles[i];
    }

    void UpdateGameplay(float dt, float* currentAngles) {
        if (animations.find(currentAnim) == animations.end() || animations[currentAnim].empty()) return;

        if (isBlending) {
            blendTimer += dt;
            float t = blendTimer / BLEND_DURATION;
            
            if (t >= 1.0f) {
                isBlending = false;
                currentAnim = nextAnim;
                currentFrame = 0.0f;
                t = 1.0f;
            }

            auto& targetTrack = animations[nextAnim];
            for (int i = 0; i < 14; i++) {
                currentAngles[i] = LerpAngle(savedAngles[i], targetTrack[0].angles[i], t);
            }
        } 
        else {
            auto& animTrack = animations[currentAnim];
            // Aplica a velocidade considerando a direção (normal ou reversa)
            currentFrame += (playSpeed * playDirection) * dt;

            // Lógica cravada para a transformação (exatos 6 frames = índices de 0 a 5)
            if (currentAnim == "transformation") {
                if (playDirection > 0.0f && currentFrame >= 5.0f) {
                    currentFrame = 5.0f; // Trava perfeitamente na pose final
                } else if (playDirection < 0.0f && currentFrame <= 0.0f) {
                    currentFrame = 0.0f; // Trava perfeitamente na pose inicial
                }
            } 
            else {
                // Lógica para as demais animações (Loop e One-Shot)
                if (animLooping[currentAnim]) {
                    if (currentFrame >= animTrack.size()) {
                        currentFrame -= animTrack.size(); // Loopa
                    } else if (currentFrame < 0.0f) {
                        currentFrame += animTrack.size(); // Previne index negativo
                    }
                } else {
                    // Animações Únicas (Ataques, Hits, Block)
                    float maxFrame = animTrack.size() - 1.0f;
                    if (currentFrame >= maxFrame) {
                        currentFrame = maxFrame; // Crava matematicamente no último frame sem deixar vazar
                        animFinished = true;
                    } else if (currentFrame < 0.0f) {
                        currentFrame = 0.0f;
                    }
                }
            }
            
            int idx1 = (int)currentFrame;
            int idx2 = (idx1 + 1) % animTrack.size();
            float blend = currentFrame - idx1;
            
            for (int i = 0; i < 14; i++) {
                float a1 = animTrack[idx1].angles[i];
                float a2 = animTrack[idx2].angles[i];
                float diff = a2 - a1;
                while (diff < -PI) diff += 2 * PI;
                while (diff > PI) diff -= 2 * PI;
                currentAngles[i] = a1 + diff * blend;
            }
        }
    }

    // NOVO: Função exclusiva que pega a pose salva do Defensor e aplica no Bot
    void UpdateDefender(float* currentAngles, Vector2& currentRoot, Vector2 attackerRoot, float facingDir) {
        // CORREÇÃO: Lê a animação alvo se estiver na transição
        std::string animToRead = isBlending ? nextAnim : currentAnim;
        if (animations.find(animToRead) == animations.end() || animations[animToRead].empty()) return;
        auto& animTrack = animations[animToRead];
        
        float blend = 0.0f;
        int idx1 = 0;
        int idx2 = 0;
        
        // Se estiver blendando, trava o cálculo no Frame 0!
        if (!isBlending) {
            idx1 = (int)currentFrame;
            idx2 = (idx1 + 1) % animTrack.size();
            blend = currentFrame - idx1;
        }
        
        // Pega a distância original gravada na animação entre o Bot e o Player
        float offsetX1 = animTrack[idx1].targetRootPos.x - animTrack[idx1].rootPos.x;
        float offsetY1 = animTrack[idx1].targetRootPos.y - animTrack[idx1].rootPos.y;
        float offsetX2 = animTrack[idx2].targetRootPos.x - animTrack[idx2].rootPos.x;
        float offsetY2 = animTrack[idx2].targetRootPos.y - animTrack[idx2].rootPos.y;
        
        float blendX = offsetX1 + (offsetX2 - offsetX1) * blend;
        float blendY = offsetY1 + (offsetY2 - offsetY1) * blend;
        
        // A posição exata que o Bot DEVE estar para a animação encaixar perfeitamente!
        currentRoot.x = attackerRoot.x + blendX * facingDir;
        currentRoot.y = attackerRoot.y + blendY;
        
        for (int i = 0; i < 14; i++) {
            float a1 = animTrack[idx1].targetAngles[i];
            float a2 = animTrack[idx2].targetAngles[i];
            float diff = a2 - a1;
            while (diff < -PI) diff += 2 * PI;
            while (diff > PI) diff -= 2 * PI;
            currentAngles[i] = a1 + diff * blend;
        }
    }
};