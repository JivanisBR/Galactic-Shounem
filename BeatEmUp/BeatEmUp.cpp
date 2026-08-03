#include "raylib.h"
#include "Physics.h"
#include "Animator.h"
#include <math.h>

struct DragTarget {
    std::shared_ptr<Particle> p;
    std::shared_ptr<Particle> parent;
    int angleIdx;
};

struct Character {
    PhysicsWorld world;
    Animator animator;
    
    bool isDead = false;
    bool showInitialFrame = false;
    
    float currentAngles[10];
    float boneLengths[10];
    int draggedJointIndex = -1; 
    DragTarget activeDragTarget;

    std::shared_ptr<Particle> pHead, pNeck, pPelvis;
    std::shared_ptr<Particle> pLElbow, pLHand, pRElbow, pRHand;
    std::shared_ptr<Particle> pLKnee, pLFoot, pRKnee, pRFoot;

    Texture2D texTorso, texHead, texArm, texForearm, texThigh, texCalf;

    void Init(Vector2 startPos) {
        Image imgTorso = GenImageColor(160, 320, BLUE); texTorso = LoadTextureFromImage(imgTorso); UnloadImage(imgTorso);
        Image imgHead = GenImageColor(160, 160, BLANK); ImageDrawCircle(&imgHead, 80, 80, 80, YELLOW); texHead = LoadTextureFromImage(imgHead); UnloadImage(imgHead);
        Image imgArm = GenImageColor(80, 160, WHITE); texArm = LoadTextureFromImage(imgArm); UnloadImage(imgArm);
        Image imgForearm = GenImageColor(80, 160, WHITE); texForearm = LoadTextureFromImage(imgForearm); UnloadImage(imgForearm);
        Image imgThigh = GenImageColor(75, 160, WHITE); texThigh = LoadTextureFromImage(imgThigh); UnloadImage(imgThigh);
        Image imgCalf = GenImageColor(75, 160, WHITE); texCalf = LoadTextureFromImage(imgCalf); UnloadImage(imgCalf);

        pNeck = std::make_shared<Particle>(startPos, 60.0f);
        pHead = std::make_shared<Particle>((Vector2){startPos.x, startPos.y - 160.0f}, 60.0f);
        
        pPelvis = std::make_shared<Particle>((Vector2){startPos.x, startPos.y + 282.5f}, 60.0f);
        
        pLElbow = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 160.0f}, 30.0f, 1.0f, false, GROUP_ARM);
        pLHand = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 320.0f}, 30.0f, 1.0f, false, GROUP_ARM);
        pRElbow = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 160.0f}, 30.0f, 1.0f, false, GROUP_ARM);
        pRHand = std::make_shared<Particle>((Vector2){startPos.x - 80.0f, startPos.y + 320.0f}, 30.0f, 1.0f, false, GROUP_ARM);

        pLKnee = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 442.5f}, 35.0f, 1.0f, false, GROUP_LEG);
        pLFoot = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 602.5f}, 35.0f, 1.0f, false, GROUP_LEG);
        pRKnee = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 442.5f}, 35.0f, 1.0f, false, GROUP_LEG);
        pRFoot = std::make_shared<Particle>((Vector2){startPos.x + 40.0f, startPos.y + 602.5f}, 35.0f, 1.0f, false, GROUP_LEG);

        world.particles = { pHead, pNeck, pPelvis, pLElbow, pLHand, pRElbow, pRHand, pLKnee, pLFoot, pRKnee, pRFoot };

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

        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pNeck, pLElbow, pLHand, -0.5f, 2.5f)); 
        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pNeck, pRElbow, pRHand, -0.5f, 2.5f)); 
        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pPelvis, pRKnee, pRFoot, -2.5f, 0.5f));
        world.angleConstraints.push_back(std::make_shared<AngleConstraint>(pPelvis, pLKnee, pLFoot, -2.5f, 0.5f));

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
    }

    void ApplyFK() {
        auto setPos = [&](std::shared_ptr<Particle> p, std::shared_ptr<Particle> parent, int angleIdx) {
            p->position.x = parent->position.x + cos(currentAngles[angleIdx]) * boneLengths[angleIdx];
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
    }

    void Update(float groundY) {
        Vector2 mouse = GetMousePosition();

        if (isDead) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                for (auto& p : world.particles) {
                    if (CheckCollisionPointCircle(mouse, p->position, p->radius * 1.5f)) {
                        activeDragTarget.p = p;
                        draggedJointIndex = 99; 
                        break;
                    }
                }
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggedJointIndex == 99) {
                activeDragTarget.p->position = mouse;
                activeDragTarget.p->previousPosition = mouse;
            }
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) draggedJointIndex = -1;

            world.Update(270.0f, 1600.0f, 0.0f, groundY);
        } else {
            animator.Update(GetFrameTime(), currentAngles, pPelvis->position);

            if (!animator.isPlaying) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.x > 270.0f) {
                    std::vector<DragTarget> targets = {
                        {pNeck, pPelvis, 0}, {pHead, pNeck, 1},
                        {pLElbow, pNeck, 2}, {pLHand, pLElbow, 3},
                        {pRElbow, pNeck, 4}, {pRHand, pRElbow, 5},
                        {pLKnee, pPelvis, 6}, {pLFoot, pLKnee, 7},
                        {pRKnee, pPelvis, 8}, {pRFoot, pRKnee, 9}
                    };
                    
                    if (CheckCollisionPointCircle(mouse, pPelvis->position, 25.0f)) {
                        draggedJointIndex = -2;
                    } else {
                        for (auto& t : targets) {
                            if (CheckCollisionPointCircle(mouse, t.p->position, 25.0f)) {
                                activeDragTarget = t;
                                draggedJointIndex = t.angleIdx;
                                break;
                            }
                        }
                    }
                }
                
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    if (draggedJointIndex == -2) {
                        pPelvis->position = mouse;
                    } else if (draggedJointIndex >= 0) {
                        float dx = mouse.x - activeDragTarget.parent->position.x;
                        float dy = mouse.y - activeDragTarget.parent->position.y;
                        currentAngles[draggedJointIndex] = atan2(dy, dx);
                    }
                }
                
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) draggedJointIndex = -1;
            }

            ApplyFK();
        }
    }

    void DrawGhostFrame(Pose& pose, Color tint) {
        Vector2 gPelvis = pose.rootPos;
        auto calcFK = [&](int angleIdx, Vector2 parentPos) -> Vector2 {
            float angle = pose.angles[angleIdx];
            float len = boneLengths[angleIdx];
            return { parentPos.x + cos(angle) * len, parentPos.y + sin(angle) * len };
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

        auto DrawGhostBone = [&](Texture2D tex, Vector2 p1, Vector2 p2, float width, float lengthBonus = 0.0f) {
            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            float angle = atan2(dy, dx) * RAD2DEG;
            float length = sqrt(dx*dx + dy*dy) + lengthBonus;
            Rectangle source = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
            Rectangle dest = { p1.x, p1.y, width, length };
            Vector2 origin = { width / 2.0f, 0.0f };
            DrawTexturePro(tex, source, dest, origin, angle - 90.0f, tint);
        };

        DrawGhostBone(texArm, gNeck, gRElbow, 80.0f); DrawGhostBone(texForearm, gRElbow, gRHand, 80.0f);
        DrawGhostBone(texThigh, gPelvis, gRKnee, 75.0f); DrawGhostBone(texCalf, gRKnee, gRFoot, 75.0f);
        DrawGhostBone(texTorso, gNeck, gPelvis, 160.0f, 37.5f);
        DrawGhostBone(texHead, gNeck, gHead, 160.0f);
        DrawGhostBone(texArm, gNeck, gLElbow, 80.0f); DrawGhostBone(texForearm, gLElbow, gLHand, 80.0f);
        DrawGhostBone(texThigh, gPelvis, gLKnee, 75.0f); DrawGhostBone(texCalf, gLKnee, gLFoot, 75.0f);
    }

    void Draw() {
        if (!animator.track.empty()) {
            // Desenha o frame 0 (Azul)
            if (showInitialFrame) {
                DrawGhostFrame(animator.track[0], Fade(BLUE, 0.4f));
            }
            // Desenha o frame anterior (Cinza)
            if (!animator.isPlaying && !isDead) {
                DrawGhostFrame(animator.track.back(), Fade(DARKGRAY, 0.4f));
            }
        }

        auto DrawBone = [](Texture2D tex, std::shared_ptr<Particle> p1, std::shared_ptr<Particle> p2, float width, Color tint, float lengthBonus = 0.0f) {
            float dx = p2->position.x - p1->position.x;
            float dy = p2->position.y - p1->position.y;
            float angle = atan2(dy, dx) * RAD2DEG;
            float length = sqrt(dx*dx + dy*dy) + lengthBonus;
            Rectangle source = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
            Rectangle dest = { p1->position.x, p1->position.y, width, length };
            Vector2 origin = { width / 2.0f, 0.0f };
            DrawTexturePro(tex, source, dest, origin, angle - 90.0f, tint);
        };

        DrawBone(texArm, pNeck, pRElbow, 80.0f, RED); DrawBone(texForearm, pRElbow, pRHand, 80.0f, RED);
        DrawBone(texThigh, pPelvis, pRKnee, 75.0f, GOLD); DrawBone(texCalf, pRKnee, pRFoot, 75.0f, GOLD);
        
        DrawBone(texTorso, pNeck, pPelvis, 160.0f, WHITE, 37.5f);
        
        DrawBone(texHead, pNeck, pHead, 160.0f, WHITE);
        DrawBone(texArm, pNeck, pLElbow, 80.0f, PINK); DrawBone(texForearm, pLElbow, pLHand, 80.0f, PINK);
        DrawBone(texThigh, pPelvis, pLKnee, 75.0f, ORANGE); DrawBone(texCalf, pLKnee, pLFoot, 75.0f, ORANGE);

        if (!isDead) {
            auto drawJoint = [](std::shared_ptr<Particle> p) {
                DrawCircleV(p->position, 15.0f, Fade(DARKGRAY, 0.7f));
                DrawCircleLines(p->position.x, p->position.y, 15.0f, BLACK);
            };
            drawJoint(pPelvis); drawJoint(pNeck); drawJoint(pHead);
            drawJoint(pLElbow); drawJoint(pLHand); drawJoint(pRElbow); drawJoint(pRHand);
            drawJoint(pLKnee); drawJoint(pLFoot); drawJoint(pRKnee); drawJoint(pRFoot);
        }
    }
};

void DrawButton(Rectangle rect, const char* text, bool& flag) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), rect);
    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) flag = !flag;
    DrawRectangleRec(rect, flag ? GREEN : LIGHTGRAY);
    DrawRectangleLinesEx(rect, 2, BLACK);
    DrawText(text, rect.x + 10, rect.y + 10, 20, BLACK);
}

int main() {
    InitWindow(1600, 1000, "Galactic Shounen - Animator Sandbox");
    SetTargetFPS(60);

    Character player;
    player.Init({ 800.0f, 200.0f });
    const float groundY = 900.0f;

    while (!WindowShouldClose()) {
        player.Update(groundY);

        BeginDrawing();
        ClearBackground(WHITE);
        DrawRectangle(0, (int)groundY, 1600, 1000 - (int)groundY, DARKGRAY);
        
        player.Draw();

        DrawRectangle(10, 10, 250, 350, Fade(LIGHTGRAY, 0.8f));
        DrawText("SISTEMA", 20, 20, 20, BLACK);
        
        Rectangle btnDead = { 20, 50, 200, 40 };
        DrawButton(btnDead, player.isDead ? "Fisica: RAGDOLL" : "Fisica: ANIMATION", player.isDead);
        if (CheckCollisionPointRec(GetMousePosition(), btnDead) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !player.isDead) {
            player.pPelvis->position.y = 200.0f;
            player.pPelvis->position.x = 800.0f;
            player.ApplyFK();
        }

        if (!player.isDead) {
            DrawText("ANIMATOR", 20, 120, 20, BLACK);
            
            Rectangle btnFrame = { 20, 150, 200, 40 };
            if (CheckCollisionPointRec(GetMousePosition(), btnFrame) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                player.animator.AddPose(player.currentAngles, player.pPelvis->position);
            }
            DrawRectangleRec(btnFrame, DARKGRAY); DrawRectangleLinesEx(btnFrame, 2, BLACK);
            DrawText("Gravar Frame", btnFrame.x + 10, btnFrame.y + 10, 20, WHITE);

            Rectangle btnPlay = { 20, 200, 200, 40 };
            if (CheckCollisionPointRec(GetMousePosition(), btnPlay) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                player.animator.isPlaying = !player.animator.isPlaying;
            }
            DrawRectangleRec(btnPlay, player.animator.isPlaying ? GREEN : RED); DrawRectangleLinesEx(btnPlay, 2, BLACK);
            DrawText(player.animator.isPlaying ? "PAUSAR" : "PLAY LOOP", btnPlay.x + 10, btnPlay.y + 10, 20, BLACK);

            DrawText(TextFormat("Velocidade: %.1f", player.animator.playSpeed), 20, 260, 20, BLACK);
            Rectangle btnSpeedDown = { 170, 255, 20, 25 };
            Rectangle btnSpeedUp = { 200, 255, 20, 25 };
            if (CheckCollisionPointRec(GetMousePosition(), btnSpeedDown) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) player.animator.playSpeed -= 1.0f;
            if (CheckCollisionPointRec(GetMousePosition(), btnSpeedUp) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) player.animator.playSpeed += 1.0f;
            DrawRectangleRec(btnSpeedDown, GRAY); DrawText("-", 175, 258, 20, WHITE);
            DrawRectangleRec(btnSpeedUp, GRAY); DrawText("+", 203, 258, 20, WHITE);

            // Exibição condicional de texto na UI
            if (player.animator.isPlaying) {
                DrawText(TextFormat("Frame Atual: %d / %d", (int)player.animator.currentFrame + 1, (int)player.animator.track.size()), 20, 310, 15, BLACK);
            } else {
                DrawText(TextFormat("Frames Salvos: %d", (int)player.animator.track.size()), 20, 310, 15, BLACK);
            }
            Rectangle btnInitial = { 20, 340, 200, 30 };
            DrawButton(btnInitial, "Ver Posição Inicial", player.showInitialFrame);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}