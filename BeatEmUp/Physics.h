#pragma once
#include "raylib.h"
#include <math.h>
#include <vector>
#include <memory>

enum ParticleGroup {
    GROUP_DEFAULT,
    GROUP_ARM,
    GROUP_LEG
};

struct Particle {
    Vector2 position;
    Vector2 previousPosition;
    float mass;
    float radius;
    bool isPinned;
    ParticleGroup group;

    Particle(Vector2 pos, float r = 15.0f, float m = 1.0f, bool pinned = false, ParticleGroup g = GROUP_DEFAULT) {
        position = pos;
        previousPosition = pos;
        radius = r;
        mass = m;
        isPinned = pinned;
        group = g;
    }
};

struct Constraint {
    std::shared_ptr<Particle> p1;
    std::shared_ptr<Particle> p2;
    float restLength;

    Constraint(std::shared_ptr<Particle> a, std::shared_ptr<Particle> b, float len) {
        p1 = a;
        p2 = b;
        restLength = len;
    }
};

struct AngleConstraint {
    std::shared_ptr<Particle> pRoot, pCenter, pEnd;
    float minAngle, maxAngle;
    bool isBroken;

    AngleConstraint(std::shared_ptr<Particle> r, std::shared_ptr<Particle> c, std::shared_ptr<Particle> e, float minA, float maxA) {
        pRoot = r;
        pCenter = c;
        pEnd = e;
        minAngle = minA;
        maxAngle = maxA;
        isBroken = true; // Alterado: Padrão agora é vir quebrado (Ragdoll solto)
    }
};

class PhysicsWorld {
public:
    std::vector<std::shared_ptr<Particle>> particles;
    std::vector<std::shared_ptr<Constraint>> constraints;
    std::vector<std::shared_ptr<AngleConstraint>> angleConstraints;
    Vector2 gravity = { 0.0f, 0.5f };

    void ApplyForces() {
        for (auto& p : particles) {
            if (p->isPinned) continue;
            Vector2 velocity = { p->position.x - p->previousPosition.x, p->position.y - p->previousPosition.y };
            
            float maxVel = 60.0f;
            float velMag = sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
            if (velMag > maxVel) {
                velocity.x = (velocity.x / velMag) * maxVel;
                velocity.y = (velocity.y / velMag) * maxVel;
            }

            p->previousPosition = p->position;
            p->position.x += velocity.x + gravity.x;
            p->position.y += velocity.y + gravity.y;
        }
    }

    void SolveConstraints() {
        const int iterations = 5; 
        for (int i = 0; i < iterations; i++) {
            for (auto& c : constraints) {
                float dx = c->p2->position.x - c->p1->position.x;
                float dy = c->p2->position.y - c->p1->position.y;
                float distance = sqrt(dx * dx + dy * dy);
                if (distance == 0.0f) continue;

                float difference = c->restLength - distance;
                float percent = difference / distance / 2.0f;
                Vector2 offset = { dx * percent, dy * percent };

                if (!c->p1->isPinned) { c->p1->position.x -= offset.x; c->p1->position.y -= offset.y; }
                if (!c->p2->isPinned) { c->p2->position.x += offset.x; c->p2->position.y += offset.y; }
            }

            for (auto& ac : angleConstraints) {
                if (ac->isBroken) continue;

                float dx1 = ac->pRoot->position.x - ac->pCenter->position.x;
                float dy1 = ac->pRoot->position.y - ac->pCenter->position.y;
                float dx2 = ac->pEnd->position.x - ac->pCenter->position.x;
                float dy2 = ac->pEnd->position.y - ac->pCenter->position.y;

                float baseAngle = atan2(dy1, dx1);
                float currentAngle = atan2(dy2, dx2);
                
                float relAngle = currentAngle - baseAngle;
                while (relAngle < -PI) relAngle += 2 * PI;
                while (relAngle > PI) relAngle -= 2 * PI;

                float clampedAngle = relAngle;
                if (relAngle < ac->minAngle) clampedAngle = ac->minAngle;
                if (relAngle > ac->maxAngle) clampedAngle = ac->maxAngle;

                if (relAngle != clampedAngle && !ac->pEnd->isPinned) {
                    float targetAngle = baseAngle + clampedAngle;
                    float dist = sqrt(dx2*dx2 + dy2*dy2);
                    
                    float targetX = ac->pCenter->position.x + cos(targetAngle) * dist;
                    float targetY = ac->pCenter->position.y + sin(targetAngle) * dist;
                    
                    float offsetX = targetX - ac->pEnd->position.x;
                    float offsetY = targetY - ac->pEnd->position.y;

                    ac->pEnd->position.x += offsetX;
                    ac->pEnd->position.y += offsetY;
                    
                    ac->pEnd->previousPosition.x += offsetX;
                    ac->pEnd->previousPosition.y += offsetY;
                }
            }
            
            for (size_t a = 0; a < particles.size(); a++) {
                for (size_t b = a + 1; b < particles.size(); b++) {
                    auto& pa = particles[a];
                    auto& pb = particles[b];

                    // Braços não colidem com nada
                    if (pa->group == GROUP_ARM || pb->group == GROUP_ARM) continue;
                    
                    // Pernas não colidem ENTRE SI
                    if (pa->group == GROUP_LEG && pb->group == GROUP_LEG) continue;

                    float dx = pb->position.x - pa->position.x;
                    float dy = pb->position.y - pa->position.y;
                    float dist = sqrt(dx*dx + dy*dy);
                    float minDist = pa->radius + pb->radius;

                    if (dist > 0 && dist < minDist) {
                        float overlap = (minDist - dist) / 2.0f;
                        float nx = (dx / dist) * overlap;
                        float ny = (dy / dist) * overlap;

                        if (!pa->isPinned) { pa->position.x -= nx; pa->position.y -= ny; }
                        if (!pb->isPinned) { pb->position.x += nx; pb->position.y += ny; }
                    }
                }
            }
        }
    }

    void ConstrainToBounds(float minX, float maxX, float minY, float maxY) {
        for (auto& p : particles) {
            if (p->position.y + p->radius > maxY) {
                p->position.y = maxY - p->radius;
                p->previousPosition.y = p->position.y;
                p->previousPosition.x = p->position.x; 
            }
            else if (p->position.y - p->radius < minY) {
                p->position.y = minY + p->radius;
                p->previousPosition.y = p->position.y;
            }
            if (p->position.x - p->radius < minX) {
                p->position.x = minX + p->radius;
                p->previousPosition.x = p->position.x;
            }
            else if (p->position.x + p->radius > maxX) {
                p->position.x = maxX - p->radius;
                p->previousPosition.x = p->position.x;
            }
        }
    }

    void Update(float minX, float maxX, float minY, float maxY) {
        ApplyForces();
        SolveConstraints();
        ConstrainToBounds(minX, maxX, minY, maxY);
    }
};