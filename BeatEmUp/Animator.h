#pragma once
#include "raylib.h"
#include <vector>
#include <math.h>

struct Pose {
    float angles[10];
    Vector2 rootPos;
};

class Animator {
public:
    std::vector<Pose> track;
    bool isPlaying = false;
    float currentFrame = 0.0f;
    float playSpeed = 10.0f; 

    void AddPose(float* currentAngles, Vector2 root) {
        Pose p;
        for(int i = 0; i < 10; i++) p.angles[i] = currentAngles[i];
        p.rootPos = root;
        track.push_back(p);
    }

    // Atualize a assinatura para receber Vector2& currentRoot
    void Update(float dt, float* currentAngles, Vector2& currentRoot) {
        if (!isPlaying || track.empty()) return;
        
        currentFrame += playSpeed * dt;
        
        if (currentFrame >= track.size()) {
            currentFrame = 0.0f; 
        }

        int idx1 = (int)currentFrame;
        int idx2 = (idx1 + 1) % track.size();
        float blend = currentFrame - idx1;

        // NOVO: Teleporta (corta a interpolação) quando vai do último frame para o frame 0
        if (idx2 == 0) {
            blend = 0.0f;
        }

        // NOVO: Interpola a posição da pélvis no mundo
        currentRoot.x = track[idx1].rootPos.x + (track[idx2].rootPos.x - track[idx1].rootPos.x) * blend;
        currentRoot.y = track[idx1].rootPos.y + (track[idx2].rootPos.y - track[idx1].rootPos.y) * blend;

        for(int i = 0; i < 10; i++) {
            float a1 = track[idx1].angles[i];
            float a2 = track[idx2].angles[i];
            
            float diff = a2 - a1;
            while (diff < -PI) diff += 2 * PI;
            while (diff > PI) diff -= 2 * PI;
            
            currentAngles[i] = a1 + diff * blend;
        }
    }
};