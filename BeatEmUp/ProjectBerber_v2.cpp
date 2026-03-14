#include "raylib.h"
#include <stdio.h>
#include <math.h>

// --- Enums e Constantes ---
typedef enum {
    STATE_IDLE,
    STATE_CHASE,
    STATE_STAGGER,
    STATE_STUN,
    STATE_KNOCKDOWN,
    STATE_FALLING
} EnemyState;

Color COMBO_COLORS[4] = { DARKGRAY, YELLOW, GREEN, BLUE };

// --- Estruturas ---

typedef struct {
    Vector2 position;
    Vector2 velocity;
    float speed;
    float jumpForce;
    bool isJumping;
    Rectangle hitbox;
    int health;
    
    // --- COMBATE ---
    bool isAttacking;
    int comboStep;          // 0=Idle, 1=Amarelo, 2=Verde, 3=Azul
    float attackTimer;      // Duração do golpe
    bool hasDealtDamage;    // Hit único
    
    // Buffer e Carregamento
    bool inputBuffer;       // Para o combo fluir
    float chargeTime;       // Para o Ataque Forte
    bool isStrongAttack;    // Flag para saber se é o golpe vermelho
    
    // Aéreo
    bool isAerialAttack;    // Flag do golpe de descida
    
    bool facingRight;
} Player;

typedef struct {
    Vector2 position;
    Vector2 velocity; // Adicionado física para o inimigo voar
    Rectangle hitbox;
    int health;
    bool isAlive;
    EnemyState state; // O "currAttack" do código original
    
    // Timers da IA (Inspirado no AS3)
    float staggerTimer;   // Leves hits
    float stunTimer;      // Hits pesados/especiais
    float knockdownTimer; // Derrubado no chão
    
    // Feedback visual
    float shakeTimer;
    Vector2 shakeOffset;
    bool onGround;
    bool isFalling;
} Enemy;

// --- Protótipos ---
Player CreatePlayer();
void UpdatePlayer(Player *player, Enemy *enemy);
void DrawPlayer(Player player);

Enemy CreateEnemy(Vector2 position);
bool GroundAI(Enemy *enemy); // A função universal do código Flash
void UpdateEnemy(Enemy *enemy, Vector2 playerPosition);
void DrawEnemy(Enemy enemy);

bool CheckCollision(Rectangle a, Rectangle b);
Vector2 CalculatePushbackDirection(Vector2 attackerPos, Vector2 victimPos, float force);

// --- Main ---

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "Barbarian C++ AI Study");
    SetTargetFPS(60);

    Player player = CreatePlayer();
    Enemy enemy = CreateEnemy((Vector2){450, 430});

    while (!WindowShouldClose()) {
        // Updates
        UpdatePlayer(&player, &enemy);
        UpdateEnemy(&enemy, player.position);

        // Draw
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            // Chão
            DrawRectangle(0, 494, screenWidth, 106, GRAY);
            DrawText("Controles: WASD (Mover/Pular) | ESPACO (Atacar) | Segurar ESPACO (Forte)", 10, 10, 20, DARKGRAY);

            DrawPlayer(player);
            DrawEnemy(enemy);
            
            // Debug States
            const char* stateText;
            switch(enemy.state) {
                case STATE_STAGGER: stateText = "STAGGER (Verde)"; break;
                case STATE_STUN: stateText = "STUN (Amarelo)"; break;
                case STATE_KNOCKDOWN: stateText = "KNOCKDOWN (Deitado)"; break;
                case STATE_FALLING: stateText = "FALLING"; break;
                default: stateText = "NORMAL"; break;
            }
            DrawText(TextFormat("Inimigo Estado: %s", stateText), 10, 40, 20, BLACK);
            
        EndDrawing();
    }

    CloseWindow();
    return 0;
}

// --- Implementações Player ---

Player CreatePlayer() {
    Player player;
    player.position = (Vector2){100, 430};
    player.velocity = (Vector2){0, 0};
    player.speed = 250.0f;
    player.jumpForce = -550.0f;
    player.isJumping = false;
    player.hitbox = (Rectangle){0, 0, 32, 64};
    player.health = 100;
    
    // Combate Reset
    player.isAttacking = false;
    player.comboStep = 0;
    player.attackTimer = 0.0f;
    player.hasDealtDamage = false;
    player.inputBuffer = false;
    
    // Mecânicas Extras Reset
    player.chargeTime = 0.0f;
    player.isStrongAttack = false;
    player.isAerialAttack = false;
    
    player.facingRight = true;
    return player;
}

void UpdatePlayer(Player *player, Enemy *enemy) {
    float dt = GetFrameTime();
    
    // --- 1. Movimentação e Pulo ---
    // Só move se não estiver travado numa animação de ataque (exceto aéreo que tem inércia)
    if (!player->isAttacking || player->isJumping) {
        float moveX = 0.0f;
        if (IsKeyDown(KEY_D)) { moveX += 1; player->facingRight = true; }
        if (IsKeyDown(KEY_A)) { moveX -= 1; player->facingRight = false; }
        
        player->position.x += moveX * player->speed * dt;

        if (IsKeyPressed(KEY_W) && !player->isJumping) {
            player->velocity.y = player->jumpForce;
            player->isJumping = true;
        }
    }

    // --- 2. Lógica de INPUT de Combate ---
    
    // A) ATAQUE AÉREO (Prioridade Máxima se estiver no ar)
    if (player->isJumping) {
        if (IsKeyPressed(KEY_SPACE)) {
            player->isAerialAttack = true; // Prepara a espada laranja
        }
    }
    // B) COMBATE NO CHÃO
    else if (player->isAttacking) {
        // Já está atacando? Gere o timer e o buffer do combo
        player->attackTimer -= dt;

        // Buffer: Permite apertar espaço antes do golpe acabar para emendar o próximo
        if (IsKeyPressed(KEY_SPACE) && !player->isStrongAttack) { 
            player->inputBuffer = true;
        }

        // Fim do golpe atual
        if (player->attackTimer <= 0) {
            // Se tinha buffer e NÃO era ataque forte, continua o combo
            if (player->inputBuffer && !player->isStrongAttack) {
                player->comboStep++;
                if (player->comboStep > 3) player->comboStep = 1;

                // Configura próximo golpe
                player->inputBuffer = false;
                player->hasDealtDamage = false;
                
                if (player->comboStep == 1) player->attackTimer = 1.0f; 
                if (player->comboStep == 2) player->attackTimer = 1.0f;
                if (player->comboStep == 3) player->attackTimer = 2.0f;
            } 
            else {
                // Reseta tudo para Idle
                player->isAttacking = false;
                player->isStrongAttack = false;
                player->comboStep = 0;
                player->hasDealtDamage = false;
            }
        }
    }
    else {
        // ESTADO IDLE: Decide entre Combo ou Forte
        
        // Segurando Espaço -> Carrega Ataque Forte
        if (IsKeyDown(KEY_SPACE)) {
            player->chargeTime += dt;
            
            // Se segurou por 0.5s, dispara o FORTE
            if (player->chargeTime >= 0.5f) {
                player->isAttacking = true;
                player->isStrongAttack = true; // Marca como forte
                player->attackTimer = 1.0f;    // Duração do forte
                player->hasDealtDamage = false;
                player->chargeTime = 0;        // Reseta carga
            }
        }
        
        // Soltou Espaço -> Se foi rápido, é Combo
        if (IsKeyReleased(KEY_SPACE)) {
            if (player->chargeTime < 0.5f) {
                player->isAttacking = true;
                player->isStrongAttack = false; // É combo normal
                player->comboStep = 1;
                player->attackTimer = 1.0f;
                player->hasDealtDamage = false;
                player->inputBuffer = false;
            }
            player->chargeTime = 0; // Reseta carga se soltar
        }
    }

    // --- 3. Física e Colisão Chão (Trigger do Aéreo) ---
    player->position.y += player->velocity.y * dt;
    player->velocity.y += 1200.0f * dt;

    if (player->position.y >= 430) {
        player->position.y = 430;
        player->isJumping = false;
        player->velocity.y = 0;
        
        // CHEGOU NO CHÃO COM ATAQUE AÉREO ARMADO?
        if (player->isAerialAttack) {
            player->isAerialAttack = false; // Desarma
            
            // Lógica de Explosão ao Pousar (AoE)
            // Cria uma hitbox grande ao redor do pé do jogador
            Rectangle slamRect = {
                player->position.x - 60, 
                player->position.y - 20, 
                150, // Largura grande
                50 
            };
            
            // Feedback Visual Rápido (Opcional, mas ajuda a ver o hit)
            DrawRectangleRec(slamRect, ORANGE); 

            if (CheckCollision(slamRect, enemy->hitbox)) {
                enemy->velocity.y = -400; // Joga pra cima
                enemy->velocity.x = (player->facingRight ? 200 : -200);
                enemy->stunTimer = 1.5f;
                enemy->health -= 25; // Dano alto
                enemy->state = STATE_STAGGER; // Ou STATE_KNOCKDOWN se tiver
            }
        }
    }
    
    player->hitbox.x = player->position.x;
    player->hitbox.y = player->position.y;

    // --- 4. Colisão de Ataque (Normal e Forte) ---
    if (player->isAttacking && !player->hasDealtDamage) {
        Rectangle attackRect;
        
        if (player->facingRight) 
            attackRect = (Rectangle){player->position.x + 32, player->position.y + 25, 60, 20};
        else 
            attackRect = (Rectangle){player->position.x - 60, player->position.y + 25, 60, 20};

        if (CheckCollision(attackRect, enemy->hitbox)) {
            player->hasDealtDamage = true;
            
            if (player->isStrongAttack) {
                // ATAQUE FORTE (VERMELHO)
                enemy->velocity.x = (player->facingRight ? 400 : -400); // Knockback ENORME
                enemy->velocity.y = -200;
                enemy->knockdownTimer = 2.0f; // Derruba
                enemy->health -= 20;
            } 
            else {
                // COMBO (AMARELO/VERDE/AZUL)
                enemy->velocity.x = (player->facingRight ? 150 : -150);
                enemy->velocity.y = -100;
                enemy->staggerTimer = 0.5f;
                enemy->health -= 10;
                
                // Finalizador do Combo (Azul) também derruba
                if (player->comboStep == 3) {
                     enemy->velocity.x *= 2;
                     enemy->knockdownTimer = 2.0f;
                }
            }
        }
    }
}

void DrawPlayer(Player player) {
    DrawRectangleRec(player.hitbox, BLUE);
    
    // 1. ATAQUE AÉREO (Espada Laranja para baixo)
    if (player.isAerialAttack) {
        Rectangle swordAir = {
            player.position.x + (player.facingRight ? 20 : 0), 
            player.position.y + 30, 
            12, 
            40 // Apontando pro chão
        };
        DrawRectangleRec(swordAir, ORANGE);
        return; // Prioridade no desenho
    }

    // 2. ATAQUES DE CHÃO
    if (!player.isAttacking) {
        // IDLE (Cinza Vertical)
        Rectangle swordIdle = {
            player.position.x + (player.facingRight ? 28 : -4), 
            player.position.y + 10, 
            6, 
            35 
        };
        DrawRectangleRec(swordIdle, DARKGRAY);
        
        // Feedback visual de carregando (opcional)
        if (player.chargeTime > 0.1f) {
            DrawCircle(player.position.x + 16, player.position.y - 10, 5, RED);
        }
    } 
    else {
        // ATACANDO (Horizontal)
        Color swordColor = DARKGRAY;
        
        if (player.isStrongAttack) {
            swordColor = RED; // Ataque Forte
        } else {
            if (player.comboStep == 1) swordColor = YELLOW;
            if (player.comboStep == 2) swordColor = GREEN;
            if (player.comboStep == 3) swordColor = BLUE;
        }

        Rectangle swordAttack;
        if (player.facingRight)
            swordAttack = (Rectangle){player.position.x + 30, player.position.y + 30, 60, 10};
        else
            swordAttack = (Rectangle){player.position.x - 58, player.position.y + 30, 60, 10};
            
        DrawRectangleRec(swordAttack, swordColor);
    }
}

// --- Implementações Enemy (Lógica AS3 Adaptada) ---

Enemy CreateEnemy(Vector2 position) {
    Enemy enemy;
    enemy.position = position;
    enemy.velocity = (Vector2){0,0};
    enemy.hitbox = (Rectangle){position.x, position.y, 32, 64};
    enemy.health = 100;
    enemy.isAlive = true;
    enemy.state = STATE_IDLE;
    enemy.onGround = true;
    
    // Inicializa timers
    enemy.staggerTimer = 0;
    enemy.stunTimer = 0;
    enemy.knockdownTimer = 0;
    enemy.shakeTimer = 0;
    
    return enemy;
}

// Essa é a tradução da função groundAI do AS3
bool GroundAI(Enemy *enemy) {
    float dt = GetFrameTime();

    // 1. Está caindo (isFalling no AS3)
    if (!enemy->onGround) {
        enemy->state = STATE_FALLING;
        // Aplica gravidade aqui ou no update
        return true; // Retorna true para impedir a IA normal de rodar
    }
    
    // 2. Knockdown (No chão)
    if (enemy->knockdownTimer > 0) {
        enemy->knockdownTimer -= dt;
        enemy->state = STATE_KNOCKDOWN;
        enemy->velocity.x = 0;
        
        // Se o timer acabar, ele levanta
        if (enemy->knockdownTimer <= 0) {
            enemy->state = STATE_IDLE;
        }
        return true;
    }

    // 3. Stun (Atordoado em pé)
    if (enemy->stunTimer > 0) {
        enemy->stunTimer -= dt;
        enemy->state = STATE_STUN;
        enemy->velocity.x = 0;
        return true;
    }

    // 4. Stagger (Cambaleando do hit)
    if (enemy->staggerTimer > 0) {
        enemy->staggerTimer -= dt;
        enemy->state = STATE_STAGGER;
        // Fricção para parar o empurrão do stagger
        enemy->velocity.x *= 0.90f; 
        return true;
    }

    return false; // Retorna false, permitindo que a IA "Specific" (perseguir) rode
}

void UpdateEnemy(Enemy *enemy, Vector2 playerPosition) {
    float dt = GetFrameTime();
    
    // Física Básica (Gravidade)
    enemy->velocity.y += 1200.0f * dt;
    enemy->position.x += enemy->velocity.x * dt;
    enemy->position.y += enemy->velocity.y * dt;

    // Colisão com chão simples
    if (enemy->position.y >= 430) {
        enemy->position.y = 430;
        enemy->velocity.y = 0;
        enemy->onGround = true;
    } else {
        enemy->onGround = false;
    }
    
    // Shake Effect (Visual)
    if (enemy->shakeTimer > 0) {
        enemy->shakeTimer -= dt;
        enemy->shakeOffset = (Vector2){ (float)GetRandomValue(-3, 3), (float)GetRandomValue(-3, 3) };
    } else {
        enemy->shakeOffset = (Vector2){0,0};
    }

    // --- CORAÇÃO DA IA ---
    // Chama a GroundAI. Se ela retornar true, o inimigo está ocupado sofrendo dano/caindo.
    if (GroundAI(enemy)) {
        // O inimigo não faz nada aqui, pois está sofrendo reação
    } 
    else {
        // --- IA ESPECÍFICA (Ex: aiSPEARMAN) ---
        // Se chegou aqui, o inimigo está livre para agir (Perseguir)
        enemy->state = STATE_CHASE;
        
        // Lógica simples de perseguição
        if (enemy->position.x < playerPosition.x - 40) enemy->velocity.x = 100;
        else if (enemy->position.x > playerPosition.x + 40) enemy->velocity.x = -100;
        else enemy->velocity.x = 0;
    }

    // Atualiza Hitbox
    if (enemy->state == STATE_KNOCKDOWN) {
        // Deitado: inverte largura/altura
        enemy->hitbox = (Rectangle){enemy->position.x - 16, enemy->position.y + 32, 64, 32};
    } else {
        // Em pé
        enemy->hitbox = (Rectangle){enemy->position.x + enemy->shakeOffset.x, enemy->position.y + enemy->shakeOffset.y, 32, 64};
    }
}

void DrawEnemy(Enemy enemy) {
    Color color;
    
    // Feedback visual baseado no estado (Pedido do usuário)
    switch (enemy.state) {
        case STATE_STAGGER: color = GREEN; break;
        case STATE_STUN:    color = GOLD; break;
        case STATE_KNOCKDOWN: color = MAROON; break; // Vermelho escuro deitado
        default: color = RED; break; // Normal
    }

    DrawRectangleRec(enemy.hitbox, color);
    
    // Debug visual para saber onde é a frente
    DrawRectangle(enemy.hitbox.x, enemy.hitbox.y, 5, 5, WHITE);
}

// --- Funções Auxiliares ---
bool CheckCollision(Rectangle a, Rectangle b) {
    return (a.x < b.x + b.width && a.x + a.width > b.x &&
            a.y < b.y + b.height && a.y + a.height > b.y);
}