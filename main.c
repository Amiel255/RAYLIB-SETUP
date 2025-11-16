#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_BULLETS 180
#define MAX_ENEMIES 120
#define MAX_PARTICLES 256
#define MAX_POWERUPS 16
#define STAR_COUNT 120

typedef enum {
    GAME_MENU,
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_OVER
} GameScreen;

typedef enum {
    POWER_HEAL,
    POWER_SPREAD,
    POWER_DASH,
    POWER_SLOW
} PowerUpType;

typedef enum {
    ENEMY_CHASER,
    ENEMY_SHOOTER,
    ENEMY_TANK
} EnemyType;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    float radius;
    float speed;
    float fireDelay;
    float fireTimer;
    int lives;
    float invulnerableTimer;
    float dashTimer;
    float dashCooldown;
    bool dashing;
    int spreadLevel;
    float slowMoTimer;
} Player;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    float radius;
    float lifetime;
    bool active;
} Bullet;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    float radius;
    float hp;
    float cooldown;
    EnemyType type;
    bool active;
} Enemy;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float size;
    float lifetime;
    bool active;
} Particle;

typedef struct {
    Vector2 position;
    PowerUpType type;
    float timer;
    bool active;
} PowerUp;

typedef struct {
    Vector2 position;
    float speed;
    float size;
    Color color;
} Star;

static Player player;
static Bullet bullets[MAX_BULLETS];
static Enemy enemies[MAX_ENEMIES];
static Particle particles[MAX_PARTICLES];
static PowerUp powerUps[MAX_POWERUPS];
static Star stars[STAR_COUNT];
static int score = 0;
static int bestScore = 0;
static float spawnTimer = 0.0f;
static float difficulty = 1.0f;
static GameScreen currentScreen = GAME_MENU;

static void ResetPlayer(Rectangle playArea)
{
    player.position = (Vector2){playArea.width / 2.0f, playArea.height / 2.0f};
    player.velocity = Vector2Zero();
    player.speed = 320.0f;
    player.radius = 16.0f;
    player.fireDelay = 0.18f;
    player.fireTimer = 0.0f;
    player.lives = 3;
    player.invulnerableTimer = 1.5f;
    player.dashTimer = 0.0f;
    player.dashCooldown = 0.0f;
    player.dashing = false;
    player.spreadLevel = 0;
    player.slowMoTimer = 0.0f;
}

static void ResetWorld(void)
{
    Rectangle playArea = (Rectangle){0, 0, 1280, 720};
    ResetPlayer(playArea);

    for (int i = 0; i < MAX_BULLETS; i++)
    {
        bullets[i].active = false;
    }
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        enemies[i].active = false;
    }
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        particles[i].active = false;
    }
    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        powerUps[i].active = false;
    }
    for (int i = 0; i < STAR_COUNT; i++)
    {
        stars[i].position = (Vector2){GetRandomValue(0, (int)playArea.width), GetRandomValue(0, (int)playArea.height)};
        stars[i].speed = GetRandomValue(20, 120);
        stars[i].size = GetRandomValue(1, 3);
        stars[i].color = (Color){(unsigned char)GetRandomValue(180, 255), (unsigned char)GetRandomValue(180, 255), 255, 255};
    }

    score = 0;
    spawnTimer = 0.0f;
    difficulty = 1.0f;
    currentScreen = GAME_MENU;
}

static void SpawnParticle(Vector2 position, Color color, float size, float speed)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (!particles[i].active)
        {
            particles[i].active = true;
            particles[i].position = position;
            particles[i].velocity = Vector2Scale(Vector2Rotate((Vector2){0, -1}, DEG2RAD * GetRandomValue(0, 360)), speed);
            particles[i].color = color;
            particles[i].size = size;
            particles[i].lifetime = 0.5f + GetRandomValue(0, 30) / 100.0f;
            break;
        }
    }
}

static void SpawnExplosion(Vector2 position, Color baseColor)
{
    for (int i = 0; i < 16; i++)
    {
        float speed = 120.0f + GetRandomValue(0, 80);
        Color color = Fade(baseColor, 0.7f);
        SpawnParticle(position, color, 4.0f, speed);
    }
}

static void FireBullet(Vector2 position, float angle, float speed)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].active)
        {
            bullets[i].active = true;
            bullets[i].position = position;
            bullets[i].velocity = Vector2Scale((Vector2){cosf(angle), sinf(angle)}, speed);
            bullets[i].radius = 6.0f;
            bullets[i].lifetime = 2.5f;
            break;
        }
    }
}

static void SpawnEnemy(Rectangle playArea)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!enemies[i].active)
        {
            enemies[i].active = true;
            enemies[i].type = (EnemyType)GetRandomValue(0, 2);
            enemies[i].radius = enemies[i].type == ENEMY_TANK ? 30.0f : 18.0f;
            enemies[i].hp = enemies[i].type == ENEMY_TANK ? 6.0f : 3.0f;
            enemies[i].speed = enemies[i].type == ENEMY_CHASER ? 120.0f : 80.0f;
            enemies[i].cooldown = (float)GetRandomValue(20, 60) / 60.0f;

            int side = GetRandomValue(0, 3);
            Vector2 spawn = {0};
            switch (side)
            {
            case 0: spawn = (Vector2){0, GetRandomValue(0, (int)playArea.height)}; break;
            case 1: spawn = (Vector2){playArea.width, GetRandomValue(0, (int)playArea.height)}; break;
            case 2: spawn = (Vector2){GetRandomValue(0, (int)playArea.width), 0}; break;
            case 3: spawn = (Vector2){GetRandomValue(0, (int)playArea.width), playArea.height}; break;
            }

            enemies[i].position = spawn;
            enemies[i].velocity = Vector2Zero();
            break;
        }
    }
}

static void SpawnPowerUp(Vector2 position)
{
    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (!powerUps[i].active)
        {
            powerUps[i].active = true;
            powerUps[i].position = position;
            powerUps[i].type = (PowerUpType)GetRandomValue(0, 3);
            powerUps[i].timer = 10.0f;
            break;
        }
    }
}

static void UpdatePlayer(Rectangle playArea, float delta)
{
    Vector2 input = Vector2Zero();
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) input.y -= 1;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) input.y += 1;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) input.x -= 1;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) input.x += 1;

    if (Vector2Length(input) > 0.0f) input = Vector2Normalize(input);

    float speedModifier = player.slowMoTimer > 0.0f ? 0.6f : 1.0f;
    player.velocity = Vector2Scale(input, player.speed * speedModifier);
    player.position = Vector2Clamp(Vector2Add(player.position, Vector2Scale(player.velocity, delta)), (Vector2){16,16}, (Vector2){playArea.width - 16, playArea.height - 16});

    if (player.invulnerableTimer > 0.0f) player.invulnerableTimer -= delta;
    if (player.dashCooldown > 0.0f) player.dashCooldown -= delta;
    if (player.dashTimer > 0.0f) player.dashTimer -= delta;
    if (player.slowMoTimer > 0.0f) player.slowMoTimer -= delta;

    bool dashInput = IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    if (dashInput && player.dashCooldown <= 0.0f)
    {
        player.dashing = true;
        player.dashTimer = 0.22f;
        player.dashCooldown = 1.2f;
    }

    if (player.dashing)
    {
        Vector2 dashDir = Vector2Length(input) > 0.0f ? input : Vector2Normalize(Vector2Subtract(GetMousePosition(), player.position));
        player.position = Vector2Clamp(Vector2Add(player.position, Vector2Scale(dashDir, 1200.0f * delta)), (Vector2){0,0}, (Vector2){playArea.width, playArea.height});
        player.invulnerableTimer = 0.4f;
        if (player.dashTimer <= 0.0f) player.dashing = false;
    }

    player.fireTimer -= delta;
    bool fire = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsKeyDown(KEY_LEFT_CONTROL);
    if (fire && player.fireTimer <= 0.0f)
    {
        Vector2 target = GetMousePosition();
        float angle = atan2f(target.y - player.position.y, target.x - player.position.x);
        FireBullet(player.position, angle, 600.0f);
        if (player.spreadLevel > 0)
        {
            float spread = DEG2RAD * 18.0f;
            FireBullet(player.position, angle + spread, 580.0f);
            FireBullet(player.position, angle - spread, 580.0f);
        }
        player.fireTimer = player.fireDelay;
    }
}

static void UpdateBullets(float delta, Rectangle playArea)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].active) continue;
        bullets[i].position = Vector2Add(bullets[i].position, Vector2Scale(bullets[i].velocity, delta));
        bullets[i].lifetime -= delta;

        if (bullets[i].lifetime <= 0.0f || bullets[i].position.x < 0 || bullets[i].position.x > playArea.width || bullets[i].position.y < 0 || bullets[i].position.y > playArea.height)
        {
            bullets[i].active = false;
        }
    }
}

static void UpdatePowerUps(float delta)
{
    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (!powerUps[i].active) continue;
        powerUps[i].timer -= delta;
        if (powerUps[i].timer <= 0.0f) powerUps[i].active = false;
    }
}

static void UpdateEnemies(float delta)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!enemies[i].active) continue;

        Vector2 toPlayer = Vector2Subtract(player.position, enemies[i].position);
        float distance = Vector2Length(toPlayer);
        Vector2 direction = distance > 0 ? Vector2Scale(toPlayer, 1.0f / distance) : Vector2Zero();

        switch (enemies[i].type)
        {
        case ENEMY_CHASER:
            enemies[i].velocity = Vector2Scale(direction, enemies[i].speed);
            break;
        case ENEMY_SHOOTER:
            enemies[i].velocity = Vector2Scale(direction, enemies[i].speed * 0.6f);
            enemies[i].cooldown -= delta;
            if (enemies[i].cooldown <= 0.0f)
            {
                FireBullet(enemies[i].position, atan2f(toPlayer.y, toPlayer.x), 380.0f);
                enemies[i].cooldown = 1.1f;
            }
            break;
        case ENEMY_TANK:
            enemies[i].velocity = Vector2Scale(direction, enemies[i].speed * 0.4f);
            break;
        }

        enemies[i].position = Vector2Add(enemies[i].position, Vector2Scale(enemies[i].velocity, delta));
    }
}

static void HandleCollisions(void)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!enemies[i].active) continue;

        for (int j = 0; j < MAX_BULLETS; j++)
        {
            if (!bullets[j].active) continue;

            if (CheckCollisionCircles(enemies[i].position, enemies[i].radius, bullets[j].position, bullets[j].radius))
            {
                enemies[i].hp -= 1.0f;
                bullets[j].active = false;
                SpawnParticle(bullets[j].position, GOLD, 3.0f, 240.0f);
                if (enemies[i].hp <= 0.0f)
                {
                    enemies[i].active = false;
                    score += 10;
                    SpawnExplosion(enemies[i].position, ORANGE);
                    if (GetRandomValue(0, 100) < 18) SpawnPowerUp(enemies[i].position);
                }
            }
        }

        if (CheckCollisionCircles(player.position, player.radius, enemies[i].position, enemies[i].radius))
        {
            if (player.invulnerableTimer <= 0.0f)
            {
                player.lives -= 1;
                player.invulnerableTimer = 1.2f;
                SpawnExplosion(player.position, SKYBLUE);
            }
        }
    }

    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (!powerUps[i].active) continue;
        if (CheckCollisionCircles(player.position, player.radius + 4, powerUps[i].position, 14.0f))
        {
            switch (powerUps[i].type)
            {
            case POWER_HEAL:
                player.lives = player.lives < 5 ? player.lives + 1 : player.lives;
                break;
            case POWER_SPREAD:
                player.spreadLevel = 1;
                player.fireDelay = 0.14f;
                break;
            case POWER_DASH:
                player.dashCooldown = 0.0f;
                player.dashTimer = 0.0f;
                break;
            case POWER_SLOW:
                player.slowMoTimer = 4.0f;
                break;
            }
            powerUps[i].active = false;
            SpawnExplosion(powerUps[i].position, GREEN);
        }
    }
}

static void UpdateParticles(float delta)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (!particles[i].active) continue;
        particles[i].position = Vector2Add(particles[i].position, Vector2Scale(particles[i].velocity, delta));
        particles[i].lifetime -= delta;
        if (particles[i].lifetime <= 0.0f) particles[i].active = false;
    }
}

static void UpdateStars(Rectangle playArea, float delta)
{
    for (int i = 0; i < STAR_COUNT; i++)
    {
        stars[i].position.y += stars[i].speed * delta;
        if (stars[i].position.y > playArea.height)
        {
            stars[i].position.y = 0;
            stars[i].position.x = GetRandomValue(0, (int)playArea.width);
        }
    }
}

static void DrawHud(Rectangle playArea)
{
    DrawRectangleLines(0, 0, (int)playArea.width, (int)playArea.height, Fade(LIGHTGRAY, 0.5f));
    DrawText(TextFormat("Score %d", score), 16, 16, 24, RAYWHITE);
    DrawText(TextFormat("Best %d", bestScore), 16, 44, 20, GRAY);
    for (int i = 0; i < player.lives; i++) DrawCircle(24 + i * 22, 78, 8, SKYBLUE);
    DrawText("WASD to move, LMB to fire, Space/RMB to dash", 16, playArea.height - 32, 20, Fade(RAYWHITE, 0.7f));
}

static void DrawPowerUps(void)
{
    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (!powerUps[i].active) continue;
        Color color = GREEN;
        switch (powerUps[i].type)
        {
        case POWER_HEAL: color = LIME; break;
        case POWER_SPREAD: color = ORANGE; break;
        case POWER_DASH: color = SKYBLUE; break;
        case POWER_SLOW: color = VIOLET; break;
        }
        DrawCircleV(powerUps[i].position, 14.0f, Fade(color, 0.8f));
        DrawCircleLines((int)powerUps[i].position.x, (int)powerUps[i].position.y, 16.0f, Fade(RAYWHITE, 0.6f));
    }
}

static void DrawParticles(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (!particles[i].active) continue;
        DrawCircleV(particles[i].position, particles[i].size, Fade(particles[i].color, particles[i].lifetime * 1.6f));
    }
}

static void DrawBullets(void)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].active) continue;
        DrawCircleV(bullets[i].position, bullets[i].radius, GOLD);
    }
}

static void DrawEnemies(void)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!enemies[i].active) continue;
        Color color = RED;
        switch (enemies[i].type)
        {
        case ENEMY_CHASER: color = RED; break;
        case ENEMY_SHOOTER: color = MAROON; break;
        case ENEMY_TANK: color = DARKPURPLE; break;
        }
        DrawCircleV(enemies[i].position, enemies[i].radius, Fade(color, 0.85f));
        DrawCircleLines((int)enemies[i].position.x, (int)enemies[i].position.y, enemies[i].radius + 3.0f, Fade(BLACK, 0.2f));
    }
}

static void DrawPlayer(void)
{
    Color tint = player.invulnerableTimer > 0.0f ? SKYBLUE : RAYWHITE;
    DrawCircleV(player.position, player.radius, Fade(tint, 0.9f));
    Vector2 aimDir = Vector2Normalize(Vector2Subtract(GetMousePosition(), player.position));
    DrawRing(player.position, player.radius - 8.0f, player.radius + 2.0f, atan2f(aimDir.y, aimDir.x) * RAD2DEG - 10, atan2f(aimDir.y, aimDir.x) * RAD2DEG + 10, 12, Fade(RAYWHITE, 0.9f));
}

static void DrawStars(Rectangle playArea)
{
    for (int i = 0; i < STAR_COUNT; i++)
    {
        DrawRectangle((int)stars[i].position.x, (int)stars[i].position.y, stars[i].size, stars[i].size, stars[i].color);
    }
}

int main(void)
{
    const int screenWidth = 1280;
    const int screenHeight = 720;
    Rectangle playArea = (Rectangle){0, 0, (float)screenWidth, (float)screenHeight};

    InitWindow(screenWidth, screenHeight, "Neon Drift");
    SetTargetFPS(60);
    ResetWorld();

    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();
        spawnTimer += delta * difficulty;
        difficulty += delta * 0.05f;

        switch (currentScreen)
        {
        case GAME_MENU:
            if (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                currentScreen = GAME_PLAYING;
                ResetPlayer(playArea);
                score = 0;
            }
            break;
        case GAME_PLAYING:
            UpdateStars(playArea, delta);
            UpdatePlayer(playArea, delta);
            UpdateBullets(delta, playArea);
            UpdateEnemies(delta);
            UpdateParticles(delta);
            UpdatePowerUps(delta);
            HandleCollisions();

            if (spawnTimer >= 1.2f)
            {
                SpawnEnemy(playArea);
                spawnTimer = 0.0f;
            }

            if (player.lives <= 0)
            {
                currentScreen = GAME_OVER;
                if (score > bestScore) bestScore = score;
            }

            if (IsKeyPressed(KEY_P)) currentScreen = GAME_PAUSED;
            break;
        case GAME_PAUSED:
            UpdateStars(playArea, delta);
            UpdateParticles(delta);
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ENTER)) currentScreen = GAME_PLAYING;
            break;
        case GAME_OVER:
            UpdateStars(playArea, delta);
            UpdateParticles(delta);
            if (IsKeyPressed(KEY_ENTER))
            {
                ResetWorld();
                currentScreen = GAME_PLAYING;
            }
            break;
        }

        BeginDrawing();
        ClearBackground((Color){10, 10, 18, 255});

        DrawStars(playArea);
        DrawParticles();

        if (currentScreen == GAME_MENU)
        {
            DrawText("NEON DRIFT", screenWidth / 2 - MeasureText("NEON DRIFT", 48) / 2, 220, 48, RAYWHITE);
            DrawText("Dash through bullets, survive the wave.", screenWidth / 2 - 230, 280, 20, LIGHTGRAY);
            DrawText("Press Enter or LMB to start", screenWidth / 2 - 170, 360, 22, SKYBLUE);
        }
        else if (currentScreen == GAME_PLAYING)
        {
            DrawEnemies();
            DrawBullets();
            DrawPowerUps();
            DrawPlayer();
            DrawHud(playArea);
        }
        else if (currentScreen == GAME_PAUSED)
        {
            DrawText("Paused", screenWidth / 2 - 70, 320, 44, SKYBLUE);
            DrawText("Press P or Enter to resume", screenWidth / 2 - 190, 380, 22, RAYWHITE);
            DrawHud(playArea);
        }
        else if (currentScreen == GAME_OVER)
        {
            DrawText("Game Over", screenWidth / 2 - 110, 280, 46, RED);
            DrawText(TextFormat("Score: %d", score), screenWidth / 2 - 70, 340, 24, RAYWHITE);
            DrawText(TextFormat("Best: %d", bestScore), screenWidth / 2 - 70, 370, 20, LIGHTGRAY);
            DrawText("Press Enter to play again", screenWidth / 2 - 170, 430, 22, SKYBLUE);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
