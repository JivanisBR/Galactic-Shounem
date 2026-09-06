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
};

class Animator {
public:
    bool animFinished = false;
    std::map<std::string, bool> animLooping; 
    
    // MAPAS DE METADADOS
    std::map<std::string, bool> animIsAttack;
    std::map<std::string, int> animActiveFrame;
    std::map<std::string, Rectangle> animHitbox;
    std::map<std::string, float> animPower;
    std::map<std::string, Vector2> animDirection;
    std::map<std::string, int> animHitType;

    // VARIÁVEIS DE EDIÇÃO (MODO ANIMATOR)
    bool editIsAttack = false;
    int editActiveFrame = 0;
    Rectangle editHitbox = {0, 0, 0, 0};
    float editPower = 10.0f;
    Vector2 editDirection = {1.0f, 0.0f};
    int editHitType = 0;

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

    float LerpAngle(float start, float end, float t) {
        float difference = fmod(end - start, PI * 2.0f);
        float distance = fmod(2.0f * difference, PI * 2.0f) - difference;
        return start + distance * t;
    }

    void SaveToFile(const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return;
        
        file << "META " << editIsAttack << " " << editActiveFrame << " " 
             << editHitbox.x << " " << editHitbox.y << " " << editHitbox.width << " " << editHitbox.height << " "
             << editPower << " " << editDirection.x << " " << editDirection.y << " " << editHitType << "\n";

        for (const auto& p : track) {
            file << p.rootPos.x << " " << p.rootPos.y;
            for (int i = 0; i < 14; i++) file << " " << p.angles[i];
            file << "\n";
        }
        file.close();
    }

    void LoadFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return;
        track.clear();
        currentFrame = 0.0f;
        editIsAttack = false; editActiveFrame = 0; editHitbox = {0,0,0,0}; editPower = 10.0f; editDirection = {1.0f, 0.0f};

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::istringstream iss(line);
            std::string firstToken;
            iss >> firstToken;
            
            if (firstToken == "META") {
                int tempAttack;
                iss >> tempAttack >> editActiveFrame >> editHitbox.x >> editHitbox.y >> editHitbox.width >> editHitbox.height >> editPower >> editDirection.x >> editDirection.y;
                if (iss >> editHitType) {} else { editHitType = 0; } // Lê se existir, senão fica 0 (compatibilidade com arquivos velhos)
                editIsAttack = tempAttack > 0;
            }
            else {
                Pose p;
                p.rootPos.x = std::stof(firstToken);
                iss >> p.rootPos.y;
                for (int i = 0; i < 14; i++) iss >> p.angles[i];
                track.push_back(p);
            }
        }
        file.close();
    }

    void LoadToLibrary(const std::string& path, const std::string& name, bool isLoop = true) {
        std::ifstream file(path);
        if (!file.is_open()) return;
        std::vector<Pose> newAnim;
        
        bool isAttack = false; int actFrame = 0; Rectangle hb = {0,0,0,0}; float pwr = 10.0f; Vector2 dir = {1.0f, 0.0f}; int hType = 0;
        std::string line;

        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::istringstream iss(line);
            std::string firstToken;
            iss >> firstToken;
            
            if (firstToken == "META") {
                int tempAttack;
                iss >> tempAttack >> actFrame >> hb.x >> hb.y >> hb.width >> hb.height >> pwr >> dir.x >> dir.y;
                if (iss >> hType) {} else { hType = 0; }
                isAttack = tempAttack > 0;
            }
            else {
                // Arquivo sem meta ou já estamos lendo os frames
                Pose p;
                p.rootPos.x = std::stof(firstToken);
                iss >> p.rootPos.y;
                for (int i = 0; i < 14; i++) iss >> p.angles[i];
                newAnim.push_back(p);
            }
        }
        if (!newAnim.empty()) {
            animations[name] = newAnim;
            animLooping[name] = isLoop;
            animIsAttack[name] = isAttack;
            animActiveFrame[name] = actFrame;
            animHitbox[name] = hb;
            animPower[name] = pwr;
            animDirection[name] = dir;
            animHitType[name] = hType;
        }
        file.close();
    }

    void AddPose(float* currentAngles, Vector2 root) {
        Pose p;
        for(int i = 0; i < 14; i++) p.angles[i] = currentAngles[i];
        p.rootPos = root;
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
        if (animations.find(name) == animations.end()) return;
        if (isBlending) { if (nextAnim == name) return; } 
        else { if (currentAnim == name) return; }
        
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

    void UpdateGameplay(float dt, float* currentAngles) {
        if (animations.find(currentAnim) == animations.end()) return;

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
};