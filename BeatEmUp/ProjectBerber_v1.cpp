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
    
    // Sistema de combo
    int comboStep;
    float comboCooldown;
    float lastAttackTime;
    Color swordColor;

    // Sistema de Combo Novo
    bool isAttacking;       // Está atacando agora?
    int comboStep;          // 0=Idle, 1=Amarelo, 2=Verde, 3=Azul
    float attackTimer;      // Contagem regressiva do ataque atual
    bool inputBuffer;       // O jogador apertou espaço DURANTE o ataque anterior?
    bool hasDealtDamage;    // O ataque atual já causou dano? (Evita multi-hit)
    
    // Ataque forte
    float chargeTime;
    float strongAttackCooldown;
    
    // Ataque aéreo
    bool isAerialAttack;
    float landingEffectTimer;
    
    int health;
    bool isAttacking;
    float attackDuration;
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
    
    // Inicialização do Combo
    player.isAttacking = false;
    player.comboStep = 0;
    player.attackTimer = 0.0f;
    player.inputBuffer = false;
    player.hasDealtDamage = false;
    
    player.facingRight = true;
    return player;
}

void UpdatePlayer(Player *player, Enemy *enemy) {
    float dt = GetFrameTime();
    
    // --- 1. Movimentação (Só pode andar se NÃO estiver atacando) ---
    // (A maioria dos beat em ups trava o movimento durante o soco)
    if (!player->isAttacking) {
        float moveX = 0.0f;
        if (IsKeyDown(KEY_D)) { moveX += 1; player->facingRight = true; }
        if (IsKeyDown(KEY_A)) { moveX -= 1; player->facingRight = false; }
        
        player->position.x += moveX * player->speed * dt;

        // Pulo
        if (IsKeyPressed(KEY_W) && !player->isJumping) {
            player->velocity.y = player->jumpForce;
            player->isJumping = true;
        }
    }

    // --- 2. Sistema de Combo (Máquina de Estados) ---
    
    // Verifica se o jogador quer atacar (Aperto novo ou Segurando)
    bool wantToAttack = IsKeyPressed(KEY_SPACE) || IsKeyDown(KEY_SPACE);

    if (player->isAttacking) {
        // Reduz o tempo do ataque atual
        player->attackTimer -= dt;

        // BUFFER: Se o jogador apertou espaço DURANTE a animação, lembramos disso
        if (IsKeyPressed(KEY_SPACE)) {
            player->inputBuffer = true;
        }

        // FIM DO ATAQUE ATUAL
        if (player->attackTimer <= 0) {
            // Se o jogador apertou espaço durante o golpe OU está segurando espaço agora
            if (player->inputBuffer || IsKeyDown(KEY_SPACE)) {
                // Passa para o próximo passo
                player->comboStep++;
                
                // Reset do loop (1 -> 2 -> 3 -> 1)
                if (player->comboStep > 3) player->comboStep = 1;

                // Configurações do novo golpe
                player->inputBuffer = false; // Consumiu o buffer
                player->hasDealtDamage = false; // Novo golpe pode dar dano
                
                // Define durações conforme seu pedido
                if (player->comboStep == 1) player->attackTimer = 1.0f; // Amarelo
                if (player->comboStep == 2) player->attackTimer = 1.0f; // Verde
                if (player->comboStep == 3) player->attackTimer = 2.0f; // Azul
            } 
            else {
                // Jogador parou de apertar: Reseta tudo para IDLE
                player->isAttacking = false;
                player->comboStep = 0;
                player->hasDealtDamage = false;
            }
        }
    } 
    else {
        // ESTADO IDLE (Esperando começar)
        if (wantToAttack && !player->isJumping) { // Começa o combo no chão
            player->isAttacking = true;
            player->comboStep = 1;      // Começa no ataque 1
            player->attackTimer = 1.0f; // Dura 1 segundo
            player->hasDealtDamage = false;
            player->inputBuffer = false;
        }
    }

    // --- 3. Física e Colisão com Chão ---
    player->position.y += player->velocity.y * dt;
    player->velocity.y += 1200.0f * dt; // Gravidade

    if (player->position.y >= 430) {
        player->position.y = 430;
        player->isJumping = false;
        player->velocity.y = 0;
    }
    
    player->hitbox.x = player->position.x;
    player->hitbox.y = player->position.y;

    // --- 4. Colisão de Ataque (Dano) ---
    // Só verifica se está atacando E se este golpe específico ainda não deu dano
    if (player->isAttacking && !player->hasDealtDamage) {
        Rectangle attackRect;
        
        // Define a área da espada (Horizontal)
        if (player->facingRight) 
            attackRect = (Rectangle){player->position.x + 32, player->position.y + 25, 60, 20};
        else 
            attackRect = (Rectangle){player->position.x - 60, player->position.y + 25, 60, 20};

        if (CheckCollision(attackRect, enemy->hitbox)) {
            // Aplica dano e efeitos
            player->hasDealtDamage = true; // MARCA QUE JÁ BATEU NESTE GOLPE!
            
            // Empurrão e Stagger
            enemy->velocity.x = (player->facingRight ? 150 : -150);
            enemy->velocity.y = -100; // Leve pulinho ao tomar hit
            enemy->staggerTimer = 0.5f;
            enemy->shakeTimer = 0.3f;
            enemy->health -= 10;
            
            // Se for o 3º ataque (Finalizador Azul), derruba o inimigo!
            if (player->comboStep == 3) {
                 enemy->velocity.x *= 2; // Empurra mais longe
                 enemy->velocity.y = -300; // Joga pra cima
                 enemy->knockdownTimer = 2.0f; // Derruba
            }
        }
    }
}

void DrawPlayer(Player player) {
    // Desenha o corpo do jogador
    DrawRectangleRec(player.hitbox, BLUE);
    
    // --- Lógica da Espada ---
    if (!player.isAttacking) {
        // IDLE: Espada Cinza na Vertical (Descansando)
        Rectangle swordIdle = {
            player.position.x + (player.facingRight ? 28 : -4), 
            player.position.y + 10, 
            6,  // Fina
            35  // Comprida pra baixo
        };
        DrawRectangleRec(swordIdle, DARKGRAY);
    } 
    else {
        // COMBO: Espada Colorida na Horizontal
        Color swordColor = DARKGRAY;
        
        // Define a cor baseada no passo do combo
        if (player.comboStep == 1) swordColor = YELLOW; // 1s
        if (player.comboStep == 2) swordColor = GREEN;  // 1s
        if (player.comboStep == 3) swordColor = BLUE;   // 2s

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