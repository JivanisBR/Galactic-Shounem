#pragma once

// Herda do Character para reaproveitar IK, FK, texturas e carregamento de animações
struct Bot : public Character {
    
    enum AIPattern { PATTERN_PASSIVE, PATTERN_AGGRESSIVE };
    AIPattern currentPattern = PATTERN_PASSIVE;
    
    // VARIÁVEIS DE INTELIGÊNCIA PROATIVA
    float baseParryChance = 80.0f; // 20% de chance base de tentar um Parry proposital
    float currentParryChance = 60.0f;
    bool hasRolledParry = false;   
    float parryCooldown = 0.0f;    // NOVO: O "Tempo de Raciocínio" do Bot para evitar spam
    float opportunistTimer = 0.0f; // NOVO: Tempo em que o Bot vai esquecer a defesa e só atacar
    
    void UpdateAI(float dt, float groundY, Character& player, bool isAggressive, bool forceBlock = false) {
        
        // NOVO: Controle Dinâmico de Padrão e Dificuldade
        if (health <= maxHealth * 0.6f) {
            currentPattern = PATTERN_AGGRESSIVE;
            currentParryChance = baseParryChance * 3.f; // Multiplicador! 3x mais chance de dar parry (60%)
        } else {
            currentPattern = PATTERN_PASSIVE;
            currentParryChance = baseParryChance;
        }

        if (shieldBroken && !isStunRagdoll) {
            shield += (maxShield / 5.0f) * dt;
            if (shield >= maxShield) {
                shield = maxShield;
                shieldBroken = false;
            }
        }
        
        // desliga o cérebro (ignora FK e IA) e roda só a gravidade nas partículas
        if (isDead) {
            if (isKnockbackActive) {
                knockbackTimer -= dt;
                if (knockbackTimer <= 0.0f) isKnockbackActive = false;
                
                std::shared_ptr<Particle> tParticle = pPelvis;
                if (knockbackTarget == 0) tParticle = pHead;
                else if (knockbackTarget == 2) tParticle = pRFoot;
                
                for (auto& p : world.particles) {
                    float force = (p == tParticle) ? (knockbackForce * 2.0f) : knockbackForce;
                    p->position.x += knockbackDir.x * force * dt;
                    p->position.y += knockbackDir.y * force * dt;
                }
            }
            
            world.Update(dt, -10000.0f, 10000.0f, -10000.0f, groundY);
            return;
        }
        
        float distX = player.pPelvis->position.x - pPelvis->position.x;
        float absDist = fabs(distX);
        float minDistance = 250.0f;
        float attackRange = 280.0f;

        // 0. Colisão Sólida (O jogador empurra o Bot se chegar muito perto)
        // Desligamos a barreira se o player for ragdoll, para o bot poder chegar perto e chutar!
        if (!player.isDead && !player.isStunRagdoll && absDist < minDistance && absDist > 0.01f) {
            float overlap = minDistance - absDist;
            float pushDir = (distX > 0) ? -1.0f : 1.0f;
            pPelvis->position.x += pushDir * overlap; 
            distX = player.pPelvis->position.x - pPelvis->position.x; 
            absDist = fabs(distX);
        }

        if (!isAttacking && !isTransforming) {
            facingDir = (distX > 0) ? 1.0f : -1.0f;
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
                float maxFrame = animator.animations["stun"].size() - 1.01f;
                if (animator.currentFrame >= maxFrame && animator.playDirection > 0.0f) animator.playDirection = -1.0f;
                else if (animator.currentFrame <= 0.01f && animator.playDirection < 0.0f) animator.playDirection = 1.0f;
            }
            if (!shieldBroken) {
                isParryStunned = false;
            }
        }
        else if (isDowned) {
            if (animator.currentAnim != "deitado") {
                animator.Play("deitado", currentAngles);
                animator.playDirection = 1.0f;
            } else {
                float maxFrame = animator.animations["deitado"].size() - 1.01f;
                if (animator.currentFrame >= maxFrame && animator.playDirection > 0.0f) animator.playDirection = -1.0f;
                else if (animator.currentFrame <= 0.01f && animator.playDirection < 0.0f) animator.playDirection = 1.0f;
            }
            
            if (!shieldBroken) {
                isDowned = false;
                isStandingUp = true;
                animator.Play("standup", currentAngles);
                animator.playDirection = 1.0f;
            }
        }
        else if (isStandingUp) {
            if (animator.animFinished) {
                isStandingUp = false;
            }
        }
        else if (isParrying) {
            // PARRY CANCEL: Se o Bot for agressivo, ele quebra a própria animação de parry 
            // no exato milissegundo em que acerta para punir o jogador imediatamente!
            if (currentPattern == PATTERN_AGGRESSIVE && player.isParryStunned) {
                isParrying = false;
                animator.playDirection = 1.0f;
            } else {
                if (animator.currentAnim != "parry") {
                    animator.Play("parry", currentAngles);
                    animator.playDirection = 1.0f;
                } else {
                    float maxFrame = fmax(0.0f, (float)animator.animations["parry"].size() - 1.01f);
                    if (animator.currentFrame >= maxFrame && animator.playDirection > 0.0f) {
                        animator.playDirection = -1.0f;
                    } 
                    else if (animator.currentFrame <= 0.01f && animator.playDirection < 0.0f) {
                        isParrying = false;
                        animator.playDirection = 1.0f;
                    }
                }
            }
        }
        else {
            bool isMoving = false;
            bool isBotRunning = false;
            float speed = 250.0f;

            if (absDist > 800.0f) {
                speed = 3000.0f; 
                isBotRunning = true;
            } else {
                speed = 1000.0f; 
            }

            // Diminui os timers de defesa e raciocínio
            if (blockTimer > 0.0f) blockTimer -= dt;
            if (parryCooldown > 0.0f) parryCooldown -= dt; 
            if (opportunistTimer > 0.0f) opportunistTimer -= dt;

            bool botShouldBlock = false;
            
            // LEITURA DA MENTE DO JOGADOR (Parry Proativo de Propósito)
            if (player.isAttacking && player.animator.animType[player.animator.currentAnim] > 0) {
                // Lê o startup do soco (nos 2 primeiros frames da animação do player)
                if (!hasRolledParry && player.animator.currentFrame < 2.0f) {
                    hasRolledParry = true; // Já tomou a decisão sobre esse ataque
                    
                    // SÓ rola o dado se a cabeça dele "esfriou" do último Parry
                    if (parryCooldown <= 0.0f) {
                        if (GetRandomValue(1, 100) <= currentParryChance) {
                            wantsToParry = true;
                            
                            // Aplica a punição orgânica (Espalha a frequência no tempo)
                            if (currentPattern == PATTERN_AGGRESSIVE) {
                                parryCooldown = GetRandomValue(5, 15) / 10.0f; // Tenta de novo em 1.5s a 2.5s
                            } else {
                                parryCooldown = GetRandomValue(20, 40) / 10.0f; // Tenta de novo em 3.0s a 6.0s
                            }
                        }
                    }
                }
            } else {
                hasRolledParry = false; // Reseta o gatilho quando o soco acabar
            }

            if (wantsToParry) {
                botShouldBlock = true;
                
                if (player.isAttacking && player.animator.animType[player.animator.currentAnim] > 0) {
                    float activeFrame = player.animator.animActiveFrame[player.animator.currentAnim];
                    
                    // 1. Abaixa a guarda de propósito 1.5 frames antes do golpe (Arma o bote)
                    if (player.animator.currentFrame >= activeFrame - 1.5f && player.animator.currentFrame < activeFrame - 0.5f) {
                        botShouldBlock = false;
                    }
                    // 2. Levanta a guarda cravado 0.5 frames antes do golpe bater! (Executa o Parry)
                    else if (player.animator.currentFrame >= activeFrame - 0.1f && player.animator.currentFrame <= activeFrame) {
                        botShouldBlock = true;
                        wantsToParry = false; // Bote acionado!
                    }
                } else {
                    // Cérebro dinâmico: Se o jogador fingir o golpe e cancelar, o bot desiste do Parry planejado
                    wantsToParry = false;
                }
            } 
            else if (blockTimer > 0.0f) {
                botShouldBlock = true;
            }

            // OPORTUNISMO RADAR: Se o player tomou parry, o bot solta a defesa por 3 segundos
            if (player.isParryStunned) {
                opportunistTimer = 5.0f;
            }

            // Enquanto estiver no modo oportunista, ele esquece que defesa existe
            if (opportunistTimer > 0.0f) {
                botShouldBlock = false;
                blockTimer = 0.0f;
                wantsToParry = false;
            }

            // Lógica final do Block e ativação da Janela
            bool wasBlocking = isBlocking;
            isBlocking = ((botShouldBlock || forceBlock) && !shieldBroken);
            
            if (isBlocking && !wasBlocking) {
                parryWindow = 0.1f; 
            }
            if (parryWindow > 0.0f) parryWindow -= dt;

            // 1. Decisão: Andar, Atacar ou Parar
            if (!isTransforming) {
                if (isBlocking) {
                    if (isAttacking) { isAttacking = false; comboStep = 0; } 
                } 
                else if (isAggressive) { 
                    
                    if (player.isDead) {
                        // O Bot só humilha chutando o corpo se estiver no Padrão Agressivo
                        if (currentPattern == PATTERN_AGGRESSIVE) {
                            if (absDist <= 400.0f) { 
                                if (!isAttacking) {
                                    isAttacking = true;
                                    comboStep = 99; // ID da rasteira/canelasso
                                    hasDealtDamage = false; 
                                    animator.Play("canelasso", currentAngles);
                                    if (!animator.animations["canelasso"].empty()) lastAnimX = animator.animations["canelasso"][0].rootPos.x;
                                    inputBuffer = false;
                                }
                            } else {
                                if (!isAttacking) {
                                    // Corre furiosamente na direção do corpo!
                                    isBotRunning = true;
                                    pPelvis->position.x += 3000.0f * facingDir * dt;
                                    isMoving = true;
                                }
                            }
                        } else {
                            // MODO PASSIVO (Honroso): O Bot para exatamente onde está, 
                            // aborta qualquer ataque e fica no Idle assistindo o player no chão.
                            isMoving = false;
                            if (isAttacking) { 
                                isAttacking = false; 
                                comboStep = 0; 
                                inputBuffer = false; 
                            }
                        }
                    }
                    else if (absDist <= attackRange) {
                        // Combate Normal
                        if (!isAttacking) {
                            isAttacking = true;
                            comboStep = 1;
                            hasDealtDamage = false; 
                            std::string firstAttack = comboList[0];
                            animator.Play(firstAttack, currentAngles);
                            if (!animator.animations[firstAttack].empty()) lastAnimX = animator.animations[firstAttack][0].rootPos.x;
                            inputBuffer = false;
                        } else if (comboStep < 5 && comboStep != 99 && comboStep != 98 && comboStep != 97) {
                            inputBuffer = true; 
                        }
                    } else {
                        if (!isAttacking) {
                            pPelvis->position.x += speed * facingDir * dt;
                            isMoving = true;
                        }
                    }
                } else {
                    if (isAttacking) { isAttacking = false; comboStep = 0; }
                }
            }

            // 2. Transição de Combo
            if (isAttacking && animator.animFinished) {
                if (inputBuffer && comboStep < 5) {
                    comboStep++;
                    hasDealtDamage = false;
                    std::string nextAttack = comboList[comboStep - 1];
                    animator.Play(nextAttack, currentAngles);
                    if (!animator.animations[nextAttack].empty()) lastAnimX = animator.animations[nextAttack][0].rootPos.x;
                    inputBuffer = false;
                } else {
                    isAttacking = false;
                    comboStep = 0;
                    inputBuffer = false;
                }
            }

            // 3. Máquina de Estados Base
            if (!isTransforming && !isAttacking) {
                if (isBlocking) animator.Play("block", currentAngles); 
                else if (isMoving) {
                    if (isBotRunning) animator.Play("running", currentAngles);
                    else animator.Play("walking", currentAngles);
                }
                else animator.Play("idle", currentAngles);
            }
        }

        // 4. Root Motion X 
        if (isAttacking && animator.animType[animator.currentAnim] > 0 && !animator.isBlending) {
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

        // 5. Root Motion Y Dinâmico
        float basePelvisY = groundY - 422.5f;
        float animOffsetY = 0.0f;
        if (animator.animations.find(animator.currentAnim) != animator.animations.end()) {
            auto& track = animator.animations[animator.currentAnim];
            int idx1 = (int)animator.currentFrame;
            int idx2 = (idx1 + 1) % track.size();
            float blend = animator.currentFrame - idx1;
            
            float currentAnimY = track[idx1].rootPos.y + (track[idx2].rootPos.y - track[idx1].rootPos.y) * blend;
            animOffsetY = currentAnimY - 482.5f; 
        }
        pPelvis->position.y = basePelvisY + animOffsetY;
        velocityY = 0.0f;

        HandleWallCollisions();

        // 6. Aplica a interpolação e a FK na engine
        animator.UpdateGameplay(dt, currentAngles);
        ApplyFK();
    }
};