#include <stdio.h>

#include <raylib.h>
//#include <math.h>
#include <raymath.h>

//bool is_web = false;
#ifdef PLATFORM_WEB
    #include <emscripten/emscripten.h>
	#define GLSL_VERSION 100
	//is_web = true;
#else
	#define GLSL_VERSION 330
#endif

#define loopi(c) for(int i=0; i<c; i++)
#define loopj(c) for(int j=0; j<c; j++)
#define loopk(c) for(int k=0; k<c; k++)

#define SCREEN_WDITH 720
#define SCREEN_HEIGHT 960
#define GAME_WIDTH	240
#define GAME_HEIGHT	320

#define IS_FLASHING(t) (((t/4)%2) == 0)


Vector2 get_rand_circle(int r)
{
	return (Vector2){GetRandomValue(-r, r), GetRandomValue(-r, r)};
}


typedef struct
{
	int start;
	int count;
	bool loop;
	int wait;
} SpriteAnimation;

typedef struct Sprite
{
	Texture2D texture;
	int width;
	int height;

	// Animation
	SpriteAnimation animations[8];
	int currentAnimation;
	int frame;
	int _wait;
	bool playing;
} Sprite;


void sprite_update(Sprite *sprite)
{
	if(!sprite->playing) return;

	const SpriteAnimation *anim = &sprite->animations[sprite->currentAnimation];
	int frame = sprite->frame;
	if(sprite->_wait >= anim->wait)
	{
		frame++;
		sprite->_wait = 0;
	}
	else
	{
		sprite->_wait++;
		return;
	}

	if(frame > anim->start + anim->count - 1)
	{
		if(anim->loop)
		{
			frame = anim->start;
		}
		else
		{
			// Stop the last frame
			frame = anim->start + anim->count - 1;
			sprite->playing = false;
		}
	}
	sprite->frame = frame;
}

void sprite_draw(Sprite sprite, Vector2 pos)
{
	int w = sprite.texture.width / sprite.width;
	int x = sprite.frame % w;
	int y = sprite.height * (sprite.frame / w);
	DrawTextureRec(sprite.texture, (Rectangle){x*sprite.width, y*sprite.height, sprite.width, sprite.height}, pos, WHITE);
}

void sprite_play(Sprite *sprite, int anim)
{
	sprite->frame = sprite->animations[anim].start;
	sprite->_wait = 0;
	sprite->currentAnimation = anim;
	sprite->playing = true;
}

bool sprite_is_end(Sprite sprite)
{
	SpriteAnimation *anim = &sprite.animations[sprite.currentAnimation];
	if(!anim->loop && sprite.frame >= anim->start+anim->count-1 && !sprite.playing)
	{
		return true;
	}
	return false;
}


struct{
	int frame;
	bool shaking;
} screenEffect = {0};

void screen_shake(int duration)
{
	screenEffect.frame = duration;
	screenEffect.shaking = true;
}


#define MAX_BALLS 64
#define BLL_RADIUS 4
#define MAX_ENEMIES 32


int g_frame = 0;
int g_spawn_time = 300;
int g_result_frame = 0;
int g_result = 0;
int g_use_keyboard = false;
Texture2D spr;
Texture2D bg;
Texture2D tex_player;
Texture2D tex_ball;
Texture2D tex_enemy;
Texture2D tex_spark;
RenderTexture2D rt;
Font font;

// sound
Sound snd_hit;
Sound snd_hurt;
Sound snd_swing;
Sound snd_spawn;
Sound snd_throw;
Sound snd_win;
Sound snd_lose;
Music bgm_main;

Shader shader_flash;


// Player
typedef struct
{
	Vector2 pos;
	bool isClick;
	Vector2 clickStart;
	int flashTime;
	Sprite sprite;
	int life;
	int swingTime;
	bool isHit;
	bool isDead;
	bool swingRequest;
} Player;
Player player = {0};


// Ball
typedef struct
{
	Vector2 pos;
	Vector2 vel;
	bool isActive;
	int killTime;
	int ignoreCollision;
	bool player; // Who's ball
} Ball;
Ball balls[MAX_BALLS] = {0};

void player_init()
{
	player = (Player){
		.pos = {104.0f, 273.0f},
		.life = 3,
		.sprite = (Sprite){
			.texture = tex_player,
			.width = 32,
			.height= 32,
			.animations = {
				[0] = {.start=0, .count=1, .wait = 6},	// Idle
				[1] = {.start=1, .count=6, .wait = 4},
				[2] = {.start=7, .count=4, .wait = 2, .loop=true}	// Walking
			}
		},
	};
}

void player_update()
{
	if(player.isDead)
	{
		return;
	}

	Vector2 move = {0};
	Vector2 touchPos = GetTouchPosition(0);

	if(IsKeyDown(KEY_W)){move.y--; g_use_keyboard=true;}
	if(IsKeyDown(KEY_S)){move.y++; g_use_keyboard=true;}
	if(IsKeyDown(KEY_A)){move.x--; g_use_keyboard=true;}
	if(IsKeyDown(KEY_D)){move.x++; g_use_keyboard=true;}
	if(IsMouseButtonDown(0) && !g_use_keyboard)
	{
		//Vector2 mousePos = {0};
#ifdef PLATFORM_WEB
		touchPos = GetTouchPosition(0);
#else
		touchPos = GetMousePosition();
#endif

		if(player.isClick == false)
		{
			player.isClick = true;
			player.clickStart = touchPos;
		}

		float dist = Vector2Distance(player.clickStart, touchPos) * 0.01f;
		dist = fminf(dist, 2.0f);
		move = Vector2Subtract(touchPos, player.clickStart);
		move = Vector2Normalize(move);
		move = Vector2Scale(move, dist);
	}
	else
	{
		player.isClick = false;
	}

	// Move Player
	Vector2 pos = Vector2Add(player.pos, move);
	// Block player with screen
	if(pos.x < 8 || pos.x > GAME_WIDTH-8)
	{
		pos.x = player.pos.x;
	}
	if(pos.y < 32 || pos.y > GAME_HEIGHT-16)
	{
		pos.y = player.pos.y;
	}
	player.pos = pos;

	if(player.isHit)
	{
		player.flashTime += 1;
		if(player.flashTime > 50)
		{
			// Reset player invincible
			player.flashTime = 0;
			player.isHit = false;
		}
	}

	// Walking animation
	if(move.x != 0.0f || move.y != 0.0f)
	{
		if(player.sprite.currentAnimation == 0)
		{
			sprite_play(&player.sprite, 2);
		}
	}
	else if(player.sprite.currentAnimation == 2)
	{
		// Stop walk animation
		sprite_play(&player.sprite, 0);
	}

	// Swing
	if((IsMouseButtonPressed(0) || IsKeyPressed(KEY_SPACE)))
	{
		if(!(player.sprite.frame > 0 && player.sprite.frame < 4))	// Stop swing request with double clicking
		{
			player.swingRequest = true;
		}
	}
	if(player.swingRequest && (player.sprite.currentAnimation == 2 || sprite_is_end(player.sprite)))
	{
		sprite_play(&player.sprite, 1);
		PlaySoundMulti(snd_swing);
		player.swingRequest = false;
	}

	// Check swing hits
	if(player.sprite.frame > 0 && player.sprite.frame < 6)
	{
		float angle = 0.0f;
		Vector2 batDir;
		switch (player.sprite.frame)
		{
		case 1:
			batDir = (Vector2){1,0};
			break;
		case 2:
			batDir = (Vector2){0,-1};
			break;
		case 3:
			batDir = (Vector2){-1,0};
			break;
		case 4:
			batDir = (Vector2){0,1};
			break;
		case 5:
			batDir = (Vector2){1,0};
			break;
		default:
			break;
		}

		bool hit = false;
		loopi(MAX_BALLS)
		{
			Ball* b = &balls[i];
			if(!b->isActive) continue;

			float dist = Vector2Distance(player.pos, balls[i].pos);
			Vector2 dir = Vector2Normalize(Vector2Subtract(player.pos, b->pos));
			angle = Vector2Angle(batDir, dir);
			if(dist < 20)
			{
				balls[i].vel = Vector2Scale(Vector2Normalize(Vector2Subtract(balls[i].pos, player.pos)), 2.0f);
				balls[i].player = true;
				screen_shake(2);
				hit = true;
			}
		}
		if(hit)
		{
			PlaySoundMulti(snd_hit);
		}
	}

	// Set idling animation
	if(player.sprite.currentAnimation == 1 && sprite_is_end(player.sprite))
	{
		sprite_play(&player.sprite, 0);
	}
}


typedef enum{
	ETYPE_NONE,
	ETYPE_NORMAL,
	ETYPE_SPLIT,
	ETYPE_RAPID,
	ETYPE_SPARK,
} EnemyType;

// Enemy
typedef struct
{
	Vector2 pos;
	int flashTime;
	Sprite sprite;
	Sprite spr_spark;
	int spawnWait;
	bool isActive;
	int throwWait;
	bool isHit;
	int index;
	EnemyType type;
} Enemy;
Enemy enemies[MAX_ENEMIES] = {0};

void enemy_new(Vector2 pos, EnemyType type)
{
	if(type == ETYPE_NONE) return;

	// Random spawn
	if(pos.x == 0.0f && pos.y == 0.0f)
	{
		pos = Vector2Add((Vector2){GAME_WIDTH/2, GAME_HEIGHT/2-20}, get_rand_circle(110));
	}

	loopi(MAX_ENEMIES)
	{
		if(!enemies[i].isActive)
		{
			Enemy* e = &enemies[i];
			*e = (Enemy){
				.isActive = true,
				.pos = pos,
				.index = i,
				.type = type,
				.throwWait = GetRandomValue(60, 180),
				.sprite = (Sprite){
					.texture = tex_enemy,
					.width = 32,
					.height = 32,
					.animations = {
						[0] = {.start=0, .count=1, .wait=0},
						[1] = {.start=1, .count=1, .wait=0}
					}
				},

				.spr_spark = (Sprite){
					.texture = tex_spark,
					.width = 32,
					.height= 64,
					.animations = {
						[0] = {.start=0, .count=6, .wait=4}
					},
				},
			};

			sprite_play(&e->spr_spark, 0);
			e->spawnWait = g_frame + 12;
			PlaySoundMulti(snd_spawn);
			return;
		}
	}
}

void enemy_delete(Enemy* enemy)
{
	*enemy = (Enemy){0};
}

void ball_new(Vector2 pos, Vector2 vel);
void enemy_update()
{
	loopi(MAX_ENEMIES)
	{
		Enemy* e = &enemies[i];
		if(!e->isActive || e->spawnWait > g_frame || player.isDead) continue;

		if(!e->isHit)
		{
			if(e->throwWait == 0)
			{
				sprite_play(&e->sprite, 1);

				//Vector2 error = e->type == ETYPE_RAPID ? get_rand_circle(64) : get_rand_circle(16);

				if(e->type == ETYPE_SPLIT)
				{
					float error = (float)GetRandomValue(-8, 8);
					Vector2 dir = Vector2Normalize(Vector2Rotate(Vector2Subtract(player.pos, e->pos), error*DEG2RAD));
					ball_new(e->pos, dir);
					float ang = GetRandomValue(0, 1) > 0 ? 10.0f : -10.0f;
					dir = Vector2Rotate(dir, ang * DEG2RAD);
					ball_new(e->pos, dir);
				}
				else if(e->type == ETYPE_SPARK)
				{
					for(int n=0; n<12; n++)
					{
						float ang = (360.0f / 12.0f);
						Vector2 dir = Vector2Subtract((Vector2){player.pos.x+5, player.pos.y}, e->pos);
						dir = Vector2Normalize(Vector2Rotate(dir, ang*(float)n*DEG2RAD));
						ball_new(e->pos, dir);
					}
				}
				else
				{
					float error = e->type == ETYPE_RAPID ? (float)GetRandomValue(-15, 15) : (float)GetRandomValue(-10, 10);
					Vector2 dir = Vector2Normalize(Vector2Rotate(Vector2Subtract(player.pos, e->pos), error*DEG2RAD));
					ball_new(e->pos, dir);
				}


				PlaySoundMulti(snd_throw);
			}
			
			// End throw
			int endThrow = e->type == ETYPE_RAPID ? -20 : -40;
			if(e->throwWait <= endThrow)
			{
				sprite_play(&e->sprite, 0);

				if(e->type == ETYPE_RAPID)
				{
					e->throwWait = GetRandomValue(20, 40);
				}
				else
				{
					e->throwWait = GetRandomValue(60, 180);
				}
			}
			e->throwWait--;
		}
		else
		{
			// Dead
			e->flashTime++;
			if(e->flashTime > 60)
			{
				enemy_delete(e);
			}
		}
	}
}

void wave_add_kill();
void enemy_take_damage(Enemy* enemy)
{
	if(enemy->isHit) return;
	enemy->isHit = true;
	wave_add_kill();
}

void enemy_kill(Enemy* enemy)
{
	enemy->isHit = true;
}



void ball_new(Vector2 pos, Vector2 vel)
{
	for(int i=0; i<MAX_BALLS; i++)
	{
		if(balls[i].isActive == false)
		{
			balls[i] = (Ball){0};
			balls[i].pos = pos;
			balls[i].vel = vel;
			balls[i].isActive = true;
			return;
		}
	}
}

void ball_update()
{
	loopi(MAX_BALLS)
	{
		Ball* b = &balls[i];
		if(!b->isActive) continue;

		// Ball is dead
		if(b->killTime > 0)
		{
			if(b->killTime <= g_frame)
			{
				b->isActive = false;
				continue;
			}
		}


		b->ignoreCollision++;
		Vector2 pos = Vector2Add(b->pos, b->vel);
		b->pos = pos;

		// Ball vs Screen
		if(pos.x < 0 || pos.x > GAME_WIDTH)
		{
			b->vel.x *= -1;
		}
		if(pos.y < 16 || pos.y > GAME_HEIGHT)
		{
			b->vel.y *= -1;
		}

		if(b->killTime > 0) continue;

		// Ball vs Enemies
		//if(b->ignoreCollision < 50) continue; // ignore collision 50 frames
		if(b->player && b->ignoreCollision > 50)
		{
			loopk(MAX_ENEMIES)
			{
				Enemy* e = &enemies[k];
				if(!e->isActive || e->spawnWait > g_frame) continue;

				float dist = Vector2Distance(b->pos, e->pos);
				if(dist < 10)
				{
					// Hit
					PlaySoundMulti(snd_hurt);
					enemy_take_damage(e);
					*b = (Ball){0};
				}
			}
		}

		// Ball vs Player
		float dist = Vector2Distance(player.pos, b->pos);
		if(!player.isHit && dist < 8)
		{
			PlaySoundMulti(snd_hurt);
			player.isHit = true;
			player.life--;
			if(player.life <= 0)
			{
				player.isDead = true;
				if(g_result == 0)
				{
					// Game over
					g_result = 2;
					g_result_frame = g_frame + 60;
				}
			}
			*b = (Ball){0};
		}
	}
}

void ball_kill(Ball* b)
{
	b->killTime = g_frame + 60;
}

void kill_all()
{
	loopi(MAX_ENEMIES)
	{
		enemy_kill(&enemies[i]);
	}

	loopi(MAX_BALLS)
	{
		ball_kill(&balls[i]);
	}
}



#define MAX_GAME_WAVES 9
#define MAX_SPAWN_DATA 32
typedef struct
{
	Vector2 pos;
	EnemyType type;
	int frame;
} SpawnData;

typedef struct
{
	SpawnData data[MAX_SPAWN_DATA];
	int enemyCount;
	int frame;
} WaveData;

struct
{
	int frame;
	int lastFrame;
	int waveCount;
	int kills;
	WaveData data[MAX_GAME_WAVES];
} g_wave = {
	.waveCount = 0,
	.data = {
		// Wave 0
		[0].frame = 60 * 10,	// 60sec
		[0].data = {
			{(Vector2){120, 180}, ETYPE_NORMAL, 60},
		},
		[0].enemyCount = 1,

		// Wave 1
		[1].frame = 60 * 60,	// 60sec
		[1].data = {
			{(Vector2){100, 180}, ETYPE_NORMAL, 60},
			{(Vector2){140, 180}, ETYPE_NORMAL, 60},
		},
		[1].enemyCount = 2,

		// Wave 2
		[2].frame = 60 * 60,	// 60sec
		[2].data = {
			{(Vector2){120, 180}, ETYPE_SPLIT, 60},
		},
		[2].enemyCount = 1,

		// Wave 3
		[3].frame = 60 * 60,	// 60sec
		[3].data = {
			{(Vector2){80, 140}, ETYPE_NORMAL, 60},
			{(Vector2){160, 140}, ETYPE_NORMAL, 60},
			{(Vector2){120, 80}, ETYPE_SPLIT, 300},
		},
		[3].enemyCount = 3,

		// Wave 4
		[4].frame = 60 * 60,	// 60sec
		[4].data = {
			{(Vector2){204, 192}, ETYPE_NORMAL, 60},
			{(Vector2){120, 112}, ETYPE_NORMAL, 90},
			{(Vector2){35, 192}, ETYPE_NORMAL, 120},
			{(Vector2){120, 180}, ETYPE_SPLIT, 150},
		},
		[4].enemyCount = 4,

		// Wave 5
		[5].frame = 60 * 60,	// 60sec
		[5].data = {
			{(Vector2){40, 100}, ETYPE_NORMAL, 60},
			{(Vector2){80, 110}, ETYPE_NORMAL, 90},
			{(Vector2){120, 112}, ETYPE_SPLIT, 120},
			{(Vector2){160, 110}, ETYPE_NORMAL, 90},
			{(Vector2){200, 100}, ETYPE_NORMAL, 60},
		},
		[5].enemyCount = 5,

		// Wave 6
		[6].frame = 60 * 60,	// 60sec
		[6].data = {
			{(Vector2){120, 180}, ETYPE_RAPID, 60},
			{(Vector2){70, 140}, ETYPE_RAPID, 360},
			{(Vector2){170, 140}, ETYPE_RAPID, 360},
			{(Vector2){20, 110}, ETYPE_SPLIT, 660},
			{(Vector2){220, 110}, ETYPE_SPLIT, 660},
		},
		[6].enemyCount = 5,


		// Wave 7
		[7].frame = 60 * 60,	// 60sec
		[7].data = {
			{(Vector2){0, 0}, ETYPE_NORMAL, 60},
			{(Vector2){0, 0}, ETYPE_NORMAL, 120},
			{(Vector2){0, 0}, ETYPE_NORMAL, 180},
			{(Vector2){0, 0}, ETYPE_NORMAL, 240},
			{(Vector2){0, 0}, ETYPE_NORMAL, 300},
			{(Vector2){0, 0}, ETYPE_NORMAL, 360},
			{(Vector2){0, 0}, ETYPE_NORMAL, 420},
			{(Vector2){0, 0}, ETYPE_RAPID, 480},
			{(Vector2){0, 0}, ETYPE_RAPID, 540},
			{(Vector2){0, 0}, ETYPE_RAPID, 600},
		},
		[7].enemyCount = 10,

		// Wave 8
		[8].frame = 60 * 60,	// 60sec
		[8].data = {
			{(Vector2){80, 140}, ETYPE_SPARK, 60},
			{(Vector2){160, 140}, ETYPE_SPARK, 60},
			{(Vector2){120, 100}, ETYPE_SPARK, 300},
		},
		[8].enemyCount = 3,
	}
};

void wave_add_kill()
{
	g_wave.kills++;
}


void reset_game()
{
	g_frame = 0;
	g_result_frame = 0;
	g_result = 0;
	g_wave.waveCount = 0;
	g_wave.lastFrame = 0;
	g_wave.frame = 0;
	g_wave.kills = 0;

	loopi(MAX_ENEMIES)
	{
		enemies[i].isActive = false;
	}
	loopi(MAX_BALLS)
	{
		balls[i].isActive = false;
	}

	player_init();
}



// #define MAX_TOUCH_POINTS 10
// Vector2 touchPositions[MAX_TOUCH_POINTS] = { 0 };
void update_frame()
{
	UpdateMusicStream(bgm_main);
    //     // Update
    //     //----------------------------------------------------------------------------------
    //     // Get multiple touchpoints
    //     for (int i = 0; i < MAX_TOUCH_POINTS; ++i) touchPositions[i] = GetTouchPosition(i);
    //     //----------------------------------------------------------------------------------

    //     // Draw
    //     //----------------------------------------------------------------------------------
    //     BeginDrawing();

    //         ClearBackground(RAYWHITE);
            
    //         for (int i = 0; i < MAX_TOUCH_POINTS; ++i)
    //         {
    //             // Make sure point is not (0, 0) as this means there is no touch for it
    //             if ((touchPositions[i].x > 0) && (touchPositions[i].y > 0))
    //             {
    //                 // Draw circle and touch index number
    //                 DrawCircleV(touchPositions[i], 34, ORANGE);
    //                 DrawText(TextFormat("%d", i), (int)touchPositions[i].x - 10, (int)touchPositions[i].y - 70, 40, BLACK);
    //             }
    //         }

    //         DrawText("touch the screen at multiple locations to get multiple balls", 10, 10, 20, DARKGRAY);

    //     EndDrawing();
    //     //----------------------------------------------------------------------------------
	// return;

	// Spawn ball
	// if(frame > 120)
	// {
	// 	ball_new((Vector2){120, 160}, (Vector2){0,1});
	// 	frame = 0;
	// }

	// Random spawn
	// if(g_spawn_time <= g_frame)
	// {
	// 	Vector2 pos = Vector2Add((Vector2){GAME_WIDTH/2, GAME_HEIGHT/2-20}, get_rand_circle(110));
	// 	enemy_new(pos, ETYPE_NORMAL);
	// 	g_spawn_time = g_frame + 300;
	// }

	if(g_wave.waveCount < MAX_GAME_WAVES)
	{
		WaveData wave = g_wave.data[g_wave.waveCount];
		loopi(MAX_SPAWN_DATA)
		{
			if(wave.data[i].type != ETYPE_NONE && g_wave.frame == wave.data[i].frame)
			{
				enemy_new(wave.data[i].pos, wave.data[i].type);
			}
		}

		g_wave.lastFrame = g_wave.frame;
		g_wave.frame++;


		// Check enemies alive
		bool nextWave = false;
		//printf("wave[%i]  %i  : %i\n", g_wave.waveCount, g_wave.kills, wave.enemyCount);
		if(g_wave.kills >= wave.enemyCount)
		{
			nextWave = true;
		}
		// if(g_wave.frame >= wave.frame)
		// {
		// 	nextWave = true;
		// }

		// Check wave time limit
		if(nextWave)
		{
			// Next wave
			g_wave.waveCount++;
			g_wave.frame = 0;
			g_wave.kills = 0;
			kill_all();
			if(g_wave.waveCount >= MAX_GAME_WAVES)
			{
				// Game Clear
				g_result_frame = g_frame + 60;
				g_result = 1;
			}
		}
	}




	// Ball
	ball_update();

	// Enemy
	enemy_update();

	// Player
	player_update();



	// Draw
	BeginTextureMode(rt);
		ClearBackground(RAYWHITE);

		DrawTexture(bg, 0, 0, WHITE);

		// Enemies
		loopi(MAX_ENEMIES)
		{
			if(enemies[i].isActive)
			{
				Enemy* e = &enemies[i];

				if(!sprite_is_end(e->spr_spark))
				{
					sprite_update(&e->spr_spark);
					sprite_draw(e->spr_spark, (Vector2){e->pos.x-16, e->pos.y-52});
				}

				if(e->spawnWait <= g_frame)
				{
					sprite_update(&e->sprite);
					if(e->isHit && IS_FLASHING(e->flashTime))
					{
						//sprite_draw(e->sprite, (Vector2){e->pos.x-16, e->pos.y-20});
					}
					else
					{
						sprite_draw(e->sprite, (Vector2){e->pos.x-16, e->pos.y-20});
						//DrawCircle(e->pos.x, e->pos.y, 2.0f, RED);
					}
				}
			}
		}


		// Player
		sprite_update(&player.sprite);
		if(player.isHit && IS_FLASHING(player.flashTime))
		{
			BeginShaderMode(shader_flash);
			sprite_draw(player.sprite, (Vector2){player.pos.x-16, player.pos.y-20});
			EndShaderMode();
		}
		else
		{
			//DrawTexture(spr, player.pos.x-16, player.pos.y-20, WHITE);
			sprite_draw(player.sprite, (Vector2){player.pos.x-16, player.pos.y-20});
		}

		// Ball
		loopi(MAX_BALLS)
		{
			if(balls[i].isActive)
			{
				Ball b = balls[i];
				Color c = balls[i].player ? YELLOW : WHITE;

				if(b.killTime > 0 && IS_FLASHING(g_frame))
				{
					//BeginShaderMode(shader_flash);
					//DrawTexture(tex_ball, b.pos.x-2, b.pos.y-2, c);
					//EndShaderMode();
				}
				else
				{
					DrawTexture(tex_ball, b.pos.x-2, b.pos.y-2, c);
				}
			}
		}

		//DrawCircle(GAME_WIDTH/2, GAME_HEIGHT/2-20, 110, RED);


		// UI
		DrawRectangle(0, 0, GAME_WIDTH, 16, (Color){0, 0, 0, 255});
		DrawTextEx(font, "OUT", (Vector2){160, 4}, 12.0f, 2.0f, WHITE);
		loopi(3)
		{
			int n = 3 - player.life;
			if(i >= n)
			{
				DrawCircle(190+(10*i), 8, 4, (Color){ 100, 41, 55, 255 });
			}
			else
			{
				DrawCircle(190+(10*i), 8, 4, RED);
			}
		}

		// Result
		if(g_result_frame > 0 && g_result_frame <= g_frame)
		{
			if(g_result_frame == g_frame)
			{
				if(g_result == 1)
				{
					PlaySoundMulti(snd_win);
				}
				else
				{
					PlaySoundMulti(snd_lose);
				}
			}

			DrawRectangle(GAME_WIDTH/2-60, GAME_HEIGHT/2-36, 120, 30, (Color){0, 0, 0, 255});
			if(g_result == 1)
			{
				DrawTextEx(font, "You Win!!", (Vector2){GAME_WIDTH/2-50, GAME_HEIGHT/2-30}, 18, 4.0f, WHITE);
			}
			else
			{
				DrawTextEx(font, "You Lose..", (Vector2){GAME_WIDTH/2-55, GAME_HEIGHT/2-30}, 18, 4.0f, WHITE);
			}

			// Continue
			if(g_result_frame + 60 < g_frame)
			{
				if(IsMouseButtonPressed(0) || IsKeyPressed(KEY_SPACE))
				{
					reset_game();
				}
			}
		}

	EndTextureMode();

	BeginDrawing();

		ClearBackground(RAYWHITE);

		int shakeSize = 5;
		int sx = 0;
		int sy = 0;
		if(screenEffect.frame > 0)
		{
			if(screenEffect.shaking)
			{
				sx = GetRandomValue(-shakeSize, shakeSize);
				sy = GetRandomValue(-shakeSize, shakeSize);
			}
			screenEffect.frame--;
			if(screenEffect.frame <= 0)
			{
				screenEffect.shaking = 0;
			}
		}
		DrawTexturePro(rt.texture, (Rectangle){0,0,240,-320}, (Rectangle){sx,sy,SCREEN_WDITH,SCREEN_HEIGHT}, (Vector2){0, 0}, 0.0f, WHITE);
	EndDrawing();
	g_frame++;
}

int main(void)
{
	// Initialization
	//--------------------------------------------------------------------------------------

	InitWindow(SCREEN_WDITH,SCREEN_HEIGHT, "game");

	bg = LoadTexture("data/bgmap.png");
	tex_ball = LoadTexture("data/ball.png");
	rt = LoadRenderTexture(240, 320);
	SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);

	shader_flash = LoadShader(0, TextFormat("data/shaders/glsl%i/flash.fs", GLSL_VERSION));

	// Player
	tex_player = LoadTexture("data/batter.png");
	player_init();


	// Enemy
	tex_enemy = LoadTexture("data/enemy.png");
	// loopi(MAX_ENEMIES)
	// {
	// 	enemies[i].sprite = (Sprite){
	// 		.texture = tex_enemy,
	// 		.width = 32,
	// 		.height = 32,
	// 		.animations = {
	// 			[0] = {.start=0, .count=1, .wait=0},
	// 			[1] = {.start=1, .count=1, .wait=0}
	// 		}
	// 	};
	// }
	
	//enemy_new((Vector2){120, 180}, ETYPE_NORMAL);

	tex_spark = LoadTexture("data/spark.png");

	// Font
	font = LoadFont("data/pixantiqua.png");
	SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
	
	InitAudioDevice();
	snd_hit = LoadSound("data/sounds/hit.wav");
	snd_hurt = LoadSound("data/sounds/hurt.wav");
	snd_swing = LoadSound("data/sounds/swing.wav");
	snd_spawn = LoadSound("data/sounds/spawn.wav");
	snd_throw = LoadSound("data/sounds/throw.wav");
	snd_win = LoadSound("data/sounds/win.wav");
	snd_lose = LoadSound("data/sounds/lose.wav");
	bgm_main = LoadMusicStream("data/sounds/bgm.ogg");

	bgm_main.looping = true;
	SetMusicVolume(bgm_main, 0.5f);
	PlayMusicStream(bgm_main);

	//--------------------------------------------------------------------------------------

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(update_frame, 60, 1);
#else

	SetTargetFPS(60);

	// Main game loop
	while (!WindowShouldClose())    // Detect window close button or ESC key
	{
		update_frame();
	}
#endif

	// De-Initialization
	//--------------------------------------------------------------------------------------
	CloseWindow();        // Close window and OpenGL context
	//--------------------------------------------------------------------------------------

	return 0;
}