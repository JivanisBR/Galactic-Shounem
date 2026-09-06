#pragma once

// Herda do Character para reaproveitar IK, FK, texturas e carregamento de animações
struct Bot : public Character {
    
    void UpdateAI(float dt, float groundY, Character& player, bool isAggressive) {
        float distX = player.pPelvis->position.x - pPelvis->position.x;
        float absDist = fabs(distX);
        float minDistance = 250.0f;
        float attackRange = 280.0f;

        // 0. Colisão Sólida (O jogador empurra o Bot se chegar muito perto)
        if (absDist < minDistance && absDist > 0.01f) {
            float overlap = minDistance - absDist;
            float pushDir = (distX > 0) ? -1.0f : 1.0f;
            pPelvis->position.x += pushDir * overlap; 
            distX = player.pPelvis->position.x - pPelvis->position.x; // Atualiza a distância
            absDist = fabs(distX);
        }

        if (!isAttacking && !isTransforming) {
            facingDir = (distX > 0) ? 1.0f : -1.0f;
        }

        if (isHit) {
            stunTimer -= dt;
            if (stunTimer <= 0.0f) isHit = false;
        } 
        else {
            bool isMoving = false;
            float speed = 250.0f;

            // 1. Decisão: Andar, Atacar ou Parar
            if (!isTransforming) {
                if (isAggressive) {
                    if (absDist <= attackRange) {
                        if (!isAttacking) {
                            isAttacking = true;
                            comboStep = 1;
                            hasDealtDamage = false; // NOVO
                            animator.Play("attack1", currentAngles);
                            lastAnimX = animator.animations["attack1"][0].rootPos.x;
                            inputBuffer = false;
                        } else if (comboStep < 5) {
                            inputBuffer = true; 
                        }
                    } else {
                        if (!isAttacking) {
                            pPelvis->position.x += speed * facingDir * dt;
                            isMoving = true;
                        }
                    }
                } else {
                    if (isAttacking) {
                        isAttacking = false;
                        comboStep = 0;
                    }
                }
            }

            // 2. Transição de Combo (Idêntica ao jogador)
            if (isAttacking && animator.animFinished) {
                if (inputBuffer && comboStep < 5) {
                    comboStep++;
                    hasDealtDamage = false;
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

            // 3. Máquina de Estados Base
            if (!isTransforming && !isAttacking) {
                if (isMoving) animator.Play("walking", currentAngles);
                else animator.Play("idle", currentAngles);
            }
        }

        // 4. Root Motion X (Idêntico ao jogador)
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

        // 5. Root Motion Y Dinâmico
        float basePelvisY = groundY - 422.5f;
        float animOffsetY = 0.0f;
        if (animator.animations.find(animator.currentAnim) != animator.animations.end()) {
            auto& track = animator.animations[animator.currentAnim];
            int idx1 = (int)animator.currentFrame;
            int idx2 = (idx1 + 1) % track.size();
            float blend = animator.currentFrame - idx1;
            
            // CORREÇÃO: Usar rootPos.y aqui
            float currentAnimY = track[idx1].rootPos.y + (track[idx2].rootPos.y - track[idx1].rootPos.y) * blend;
            animOffsetY = currentAnimY - 482.5f; 
        }
        pPelvis->position.y = basePelvisY + animOffsetY;
        velocityY = 0.0f;

        // 6. Aplica a interpolação e a FK na engine
        animator.UpdateGameplay(dt, currentAngles);
        ApplyFK();
    }
};