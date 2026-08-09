#pragma once
#include "raylib.h"
#include <vector>
#include <math.h>
#include <fstream>
#include <string>
#include <map>

struct Pose {
    float angles[12];
    Vector2 rootPos;
};

class Animator {
public:
    std::vector<Pose> track;
    bool isPlaying = false;
    float currentFrame = 0.0f;
    float playSpeed = 10.0f; 

    std::map<std::string, std::vector<Pose>> animations;
    std::string currentAnim = "";
    std::string nextAnim = "";
    bool isBlending = false;
    float blendTimer = 0.0f;
    const float BLEND_DURATION = 0.15f; 
    float savedAngles[12];

    float LerpAngle(float start, float end, float t) {
        float difference = fmod(end - start, PI * 2.0f);
        float distance = fmod(2.0f * difference, PI * 2.0f) - difference;
        return start + distance * t;
    }

    void SaveToFile(const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return;
        for (const auto& p : track) {
            file << p.rootPos.x << " " << p.rootPos.y;
            for (int i = 0; i < 12; i++) file << " " << p.angles[i];
            file << "\n";
        }
        file.close();
    }

    void LoadFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return;
        track.clear();
        currentFrame = 0.0f;
        Pose p;
        while (file >> p.rootPos.x >> p.rootPos.y) {
            for (int i = 0; i < 12; i++) file >> p.angles[i];
            track.push_back(p);
        }
        file.close();
    }

    void AddPose(float* currentAngles, Vector2 root) {
        Pose p;
        for(int i = 0; i < 12; i++) p.angles[i] = currentAngles[i];
        p.rootPos = root;
        track.push_back(p);
    }

    void Update(float dt, float* currentAngles, Vector2& currentRoot) {
        if (!isPlaying || track.empty()) return;
        
        currentFrame += playSpeed * dt;
        if (currentFrame >= track.size()) currentFrame -= track.size(); 

        int idx1 = (int)currentFrame;
        int idx2 = (idx1 + 1) % track.size();
        float blend = currentFrame - idx1;

        currentRoot.x = track[idx1].rootPos.x + (track[idx2].rootPos.x - track[idx1].rootPos.x) * blend;
        currentRoot.y = track[idx1].rootPos.y + (track[idx2].rootPos.y - track[idx1].rootPos.y) * blend;

        for(int i = 0; i < 12; i++) {
            float a1 = track[idx1].angles[i];
            float a2 = track[idx2].angles[i];
            float diff = a2 - a1;
            while (diff < -PI) diff += 2 * PI;
            while (diff > PI) diff -= 2 * PI;
            currentAngles[i] = a1 + diff * blend;
        }
    }

    void LoadToLibrary(const std::string& path, const std::string& name) {
        std::ifstream file(path);
        if (!file.is_open()) return;
        std::vector<Pose> newAnim;
        Pose p;
        while (file >> p.rootPos.x >> p.rootPos.y) {
            for (int i = 0; i < 12; i++) file >> p.angles[i];
            newAnim.push_back(p);
        }
        if (!newAnim.empty()) animations[name] = newAnim;
        file.close();
    }

    void Play(const std::string& name, float* currentAngles) {
        if (animations.find(name) == animations.end()) return;
        
        if (isBlending) {
            if (nextAnim == name) return;
        } else {
            if (currentAnim == name) return;
        }
        
        if (currentAnim == "") { 
            currentAnim = name;
            currentFrame = 0.0f;
            return;
        }

        nextAnim = name;
        isBlending = true;
        blendTimer = 0.0f;
        for (int i = 0; i < 12; i++) savedAngles[i] = currentAngles[i];
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
            for (int i = 0; i < 12; i++) {
                currentAngles[i] = LerpAngle(savedAngles[i], targetTrack[0].angles[i], t);
            }
        } 
        else {
            currentFrame += playSpeed * dt;
            auto& animTrack = animations[currentAnim];
            if (currentFrame >= animTrack.size()) currentFrame -= animTrack.size();
            
            int idx1 = (int)currentFrame;
            int idx2 = (idx1 + 1) % animTrack.size();
            float blend = currentFrame - idx1;
            
            for (int i = 0; i < 12; i++) {
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