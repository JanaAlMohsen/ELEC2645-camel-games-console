#include "Game_1.h"
#include "camel_action_sprite.h"
#include "camel_back_run_sprite.h"
#include "camel_side_run_sprite.h"
#include "game_over_screen_sprite.h"
#include "gameplay_background_sprite.h"
#include "object_sprites.h"
#include "start_hamoodi_sprite.h"
#include "start_screen_sprite.h"
#include "street_overlay_sprite.h"
#include "InputHandler.h"
#include "Menu.h"
#include "LCD.h"
#include "PWM.h"
#include "Buzzer.h"
#include "Joystick.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>

extern ST7789V2_cfg_t cfg0;
extern PWM_cfg_t pwm_cfg;
extern Buzzer_cfg_t buzzer_cfg;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;

/*
 * GAME 1: CAMEL DESERT RUNNER
 * ---------------------------------
 * A simple 3-lane endless runner inspired by Temple Run.
 * The player controls a camel using the joystick and avoids desert obstacles.
 *
 * Controls:
 *   Joystick LEFT/RIGHT  = move between the 3 lanes
 *   BT2                  = start / restart
 *   BT3                  = return to main menu
 */

#define GAME1_FRAME_TIME_MS 35

// Custom palette indices from main.c / LCD custom palette
#define COL_BLACK      0
#define COL_WHITE      1
#define COL_SKY        2
#define COL_BROWN      3
#define COL_CAMEL      4
#define COL_NOSE       5
#define COL_SAND       6
#define COL_PINK       7
#define COL_LAVENDER   8
#define COL_GREY       9
#define COL_CACTUS     10
#define COL_RED        11
#define COL_GREEN      12
#define COL_BLUE       13
#define COL_YELLOW     14
#define COL_ORANGE     15

#define SCREEN_W 240
#define SCREEN_H 240
#define PLAYER_Y 184
#define PLAYER_W 34
#define PLAYER_H 34
#define OBJECT_W 35
#define OBJECT_H 35
#define MAX_OBJECTS 4
#define MAX_LIVES 3
#define DATE_MULTIPLIER_FRAMES (5000 / GAME1_FRAME_TIME_MS)
#define COIN_BONUS_POINTS 25
#define GAME1_MUSIC_VOLUME 5
#define CAMEL_JUMP_FRAMES 18
#define CAMEL_DUCK_FRAMES 13
#define CAMEL_JUMP_HEIGHT 34
#define CAMEL_DUCK_DROP 12
#define SCORE_FLASH_STEP 500

// RGB LED pins from the gameplay breadboard wiring. Both are common cathode:
// GPIO HIGH turns that colour on.
#define RGB1_R_PORT GPIOA
#define RGB1_R_PIN  GPIO_PIN_9
#define RGB1_G_PORT GPIOC
#define RGB1_G_PIN  GPIO_PIN_7
#define RGB1_B_PORT GPIOC
#define RGB1_B_PIN  GPIO_PIN_8

#define RGB2_R_PORT GPIOC
#define RGB2_R_PIN  GPIO_PIN_9
#define RGB2_G_PORT GPIOD
#define RGB2_G_PIN  GPIO_PIN_2
#define RGB2_B_PORT GPIOA
#define RGB2_B_PIN  GPIO_PIN_5

// 3 fixed lane centre positions. This makes the game easier to control.
static const int lane_x[3] = {55, 120, 185};

typedef enum {
    GAME_START_SCREEN = 0,
    GAME_RUNNING,
    GAME_OVER
} RunnerState;

typedef enum {
    OBJ_ROCK = 0,
    OBJ_CACTUS,
    OBJ_SNAKE,
    OBJ_EAGLE,
    OBJ_CRACK,
    OBJ_DATE,
    OBJ_WATER,
    OBJ_COIN
} ObjectType;

typedef struct {
    uint8_t active;
    int lane;
    int y;
    ObjectType type;
} DesertObject;

static RunnerState runner_state;
static DesertObject objects[MAX_OBJECTS];
static int player_lane;
static int target_lane;
static int player_x;
static int score;
static int high_score;
static int lives;
static int score_multiplier_timer;
static int spawn_timer;
static int spawn_delay;
static Direction last_direction;
static int camel_run_frame;
static int camel_anim_timer;
static int music_step;
static int music_timer;
static int sfx_timer;
static int game_over_sound_step;
static int game_over_sound_timer;
static uint8_t game_over_sound_done;
static int next_score_flash;

typedef enum {
    CAMEL_BACK = 0,
    CAMEL_FACE_LEFT,
    CAMEL_FACE_RIGHT
} CamelPose;

typedef enum {
    CAMEL_ACTION_NORMAL = 0,
    CAMEL_ACTION_JUMP,
    CAMEL_ACTION_DUCK
} CamelAction;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    int timer;
    int blink_frames;
} RgbEffect;

static CamelPose camel_pose;
static CamelAction camel_action;
static int camel_action_timer;
static RgbEffect rgb_effects[2];

static void reset_game(void);
static void update_game(void);
static void render_start_screen(void);
static void render_game(void);
static void render_game_over(void);
static void spawn_object(void);
static void draw_background(void);
static void draw_camel(int x, int y, CamelPose pose);
static void draw_rock(int x, int y);
static void draw_cactus(int x, int y);
static void draw_snake(int x, int y);
static void draw_eagle(int x, int y);
static void draw_crack(int x, int y);
static void draw_date(int x, int y);
static void draw_water_drop(int x, int y);
static void draw_coin(int x, int y);
static void draw_hud_heart(int x, int y);
static void print_outlined(char const *text, uint16_t x, uint16_t y, uint8_t colour, uint8_t outline_colour, uint8_t font_size);
static uint8_t object_is_collectible(ObjectType type);
static uint8_t object_is_dodged(DesertObject const *obj);
static uint8_t object_hits_player(DesertObject *obj);
static uint8_t joystick_left(Direction dir);
static uint8_t joystick_right(Direction dir);
static uint8_t joystick_up(Direction dir);
static uint8_t joystick_down(Direction dir);
static int get_camel_draw_y(void);
static void play_sfx(uint32_t freq_hz, uint8_t volume_percent, int frames);
static void update_tension_music(void);
static void start_game_over_sound(void);
static void update_game_over_sound(void);
static void rgb_all_off(void);
static void rgb_start_flash(int led, uint8_t r, uint8_t g, uint8_t b, int frames, int blink_frames);
static void rgb_update_effects(void);

static void reset_game(void)
{
    player_lane = 1;       // start in middle lane
    target_lane = player_lane;
    player_x = lane_x[player_lane];  // actual camel screen position starts in the middle
    score = 0;
    lives = 1;
    score_multiplier_timer = 0;
    spawn_timer = 0;
    spawn_delay = 34;
    last_direction = CENTRE;
    camel_pose = CAMEL_BACK;
    camel_action = CAMEL_ACTION_NORMAL;
    camel_action_timer = 0;
    camel_run_frame = 0;
    camel_anim_timer = 0;
    music_step = 0;
    music_timer = 0;
    sfx_timer = 0;
    game_over_sound_step = 0;
    game_over_sound_timer = 0;
    game_over_sound_done = 1;
    next_score_flash = SCORE_FLASH_STEP;
    rgb_effects[0].timer = 0;
    rgb_effects[1].timer = 0;
    rgb_all_off();
    buzzer_off(&buzzer_cfg);

    for (int i = 0; i < MAX_OBJECTS; i++) {
        objects[i].active = 0;
        objects[i].lane = 0;
        objects[i].y = -40;
        objects[i].type = OBJ_ROCK;
    }
}

static uint8_t joystick_left(Direction dir)
{
    return (dir == W || dir == NW || dir == SW);
}

static uint8_t joystick_right(Direction dir)
{
    return (dir == E || dir == NE || dir == SE);
}

static uint8_t joystick_up(Direction dir)
{
    return (dir == N || dir == NE || dir == NW);
}

static uint8_t joystick_down(Direction dir)
{
    return (dir == S || dir == SE || dir == SW);
}

static int get_camel_draw_y(void)
{
    if (camel_action == CAMEL_ACTION_JUMP) {
        int elapsed = CAMEL_JUMP_FRAMES - camel_action_timer;
        int half = CAMEL_JUMP_FRAMES / 2;
        int height;

        if (elapsed < half) {
            height = (elapsed * CAMEL_JUMP_HEIGHT) / half;
        }
        else {
            height = ((CAMEL_JUMP_FRAMES - elapsed) * CAMEL_JUMP_HEIGHT) / (CAMEL_JUMP_FRAMES - half);
        }

        return PLAYER_Y - height;
    }

    if (camel_action == CAMEL_ACTION_DUCK) {
        return PLAYER_Y + CAMEL_DUCK_DROP;
    }

    return PLAYER_Y;
}

static void rgb_write_led(int led, uint8_t r, uint8_t g, uint8_t b)
{
    if (led == 0) {
        // Observed RGB1 wiring: PA9=blue, PC7=green, PC8=red.
        HAL_GPIO_WritePin(RGB1_R_PORT, RGB1_R_PIN, b ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(RGB1_G_PORT, RGB1_G_PIN, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(RGB1_B_PORT, RGB1_B_PIN, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
    else {
        HAL_GPIO_WritePin(RGB2_R_PORT, RGB2_R_PIN, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(RGB2_G_PORT, RGB2_G_PIN, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(RGB2_B_PORT, RGB2_B_PIN, b ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

static void rgb_all_off(void)
{
    rgb_write_led(0, 0, 0, 0);
    rgb_write_led(1, 0, 0, 0);
}

static void rgb_start_flash(int led, uint8_t r, uint8_t g, uint8_t b, int frames, int blink_frames)
{
    if (led < 0 || led > 1) {
        return;
    }

    rgb_effects[led].r = r;
    rgb_effects[led].g = g;
    rgb_effects[led].b = b;
    rgb_effects[led].timer = frames;
    rgb_effects[led].blink_frames = blink_frames;
}

static void rgb_update_effects(void)
{
    for (int led = 0; led < 2; led++) {
        if (rgb_effects[led].timer > 0) {
            int blink_frames = rgb_effects[led].blink_frames;
            if (blink_frames < 1) {
                blink_frames = 1;
            }

            uint8_t on = ((rgb_effects[led].timer / blink_frames) % 2) == 1;
            rgb_write_led(led,
                          on ? rgb_effects[led].r : 0,
                          on ? rgb_effects[led].g : 0,
                          on ? rgb_effects[led].b : 0);
            rgb_effects[led].timer--;
        }
        else {
            rgb_write_led(led, 0, 0, 0);
        }
    }
}

static void play_sfx(uint32_t freq_hz, uint8_t volume_percent, int frames)
{
    buzzer_tone(&buzzer_cfg, freq_hz, volume_percent);
    sfx_timer = frames;
    music_timer = 0;
}

static void update_tension_music(void)
{
    static const uint16_t notes[] = {
        NOTE_E4, 0, NOTE_G4, 0, NOTE_AS4, NOTE_B4, 0, NOTE_AS4, NOTE_G4, 0
    };
    static const uint8_t durations[] = {
        3, 1, 3, 1, 3, 4, 1, 3, 3, 2
    };
    const int note_count = (int)(sizeof(notes) / sizeof(notes[0]));

    if (sfx_timer > 0) {
        sfx_timer--;
        return;
    }

    if (music_timer > 0) {
        music_timer--;
        return;
    }

    uint16_t note = notes[music_step];
    if (note == 0) {
        buzzer_off(&buzzer_cfg);
    }
    else {
        uint8_t volume = GAME1_MUSIC_VOLUME + (score / 350);
        if (volume > 9) {
            volume = 9;
        }
        buzzer_tone(&buzzer_cfg, note, volume);
    }

    music_timer = durations[music_step];
    music_step++;
    if (music_step >= note_count) {
        music_step = 0;
    }
}

static void start_game_over_sound(void)
{
    game_over_sound_step = 0;
    game_over_sound_timer = 0;
    game_over_sound_done = 0;
    sfx_timer = 0;
    music_timer = 0;
    buzzer_off(&buzzer_cfg);
}

static void update_game_over_sound(void)
{
    static const uint16_t notes[] = {
        330, 247, 0,
        294, 220, 0,
        262, 196, 0
    };
    static const uint8_t durations[] = {
        2, 2, 1,
        2, 2, 1,
        2, 2, 1
    };
    const int note_count = (int)(sizeof(notes) / sizeof(notes[0]));

    if (game_over_sound_done) {
        return;
    }

    if (game_over_sound_timer > 0) {
        game_over_sound_timer--;
        return;
    }

    if (game_over_sound_step >= note_count) {
        game_over_sound_done = 1;
        buzzer_off(&buzzer_cfg);
        return;
    }

    uint16_t note = notes[game_over_sound_step];
    if (note == 0) {
        buzzer_off(&buzzer_cfg);
    }
    else {
        buzzer_tone(&buzzer_cfg, note, 12);
    }

    game_over_sound_timer = durations[game_over_sound_step];
    game_over_sound_step++;
}

static void spawn_object(void)
{
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) {
            objects[i].active = 1;
            objects[i].lane = rand() % 3;
            objects[i].y = 28;

            // Most objects are dangerous, with occasional collectible bonuses.
            int r = rand() % 18;
            if (r == 0) {
                objects[i].type = OBJ_WATER;
            }
            else if (r == 1 || r == 2) {
                objects[i].type = OBJ_COIN;
            }
            else if (r == 3) {
                objects[i].type = OBJ_DATE;
            }
            else if (r == 4 || r == 5) {
                objects[i].type = OBJ_EAGLE;
            }
            else if (r == 6 || r == 7) {
                objects[i].type = OBJ_CRACK;
            }
            else if (r == 8 || r == 9 || r == 10) {
                objects[i].type = OBJ_CACTUS;
            }
            else if (r == 11 || r == 12) {
                objects[i].type = OBJ_SNAKE;
            }
            else {
                objects[i].type = OBJ_ROCK;
            }
            break;
        }
    }
}

static uint8_t object_is_collectible(ObjectType type)
{
    return (type == OBJ_DATE || type == OBJ_WATER || type == OBJ_COIN);
}

static uint8_t object_is_dodged(DesertObject const *obj)
{
    return ((obj->type == OBJ_EAGLE && camel_action == CAMEL_ACTION_DUCK) ||
            (obj->type == OBJ_CRACK && camel_action == CAMEL_ACTION_JUMP));
}

static uint8_t object_hits_player(DesertObject *obj)
{
    if (!obj->active) {
        return 0;
    }

    if (obj->lane != player_lane) {
        return 0;
    }

    // Simple rectangle collision using y overlap because lanes already match.
    if ((obj->y + OBJECT_H) >= PLAYER_Y && obj->y <= (PLAYER_Y + PLAYER_H)) {
        return 1;
    }

    return 0;
}

static void update_game(void)
{
    Direction dir = joystick_data.direction;

    // Move only once per joystick push, not every frame while held.
    // The camel faces sideways while travelling, then returns to the back view
    // after it arrives in the selected lane.
    uint8_t camel_is_switching_lanes = (player_x != lane_x[target_lane]);

    if (!camel_is_switching_lanes && joystick_left(dir) && !joystick_left(last_direction)) {
        if (target_lane > 0) {
            target_lane--;
            camel_pose = CAMEL_FACE_LEFT;
            camel_run_frame = 0;
            camel_anim_timer = 0;
        }
    }
    else if (!camel_is_switching_lanes && joystick_right(dir) && !joystick_right(last_direction)) {
        if (target_lane < 2) {
            target_lane++;
            camel_pose = CAMEL_FACE_RIGHT;
            camel_run_frame = 0;
            camel_anim_timer = 0;
        }
    }

    if (camel_action == CAMEL_ACTION_NORMAL && joystick_up(dir) && !joystick_up(last_direction)) {
        camel_action = CAMEL_ACTION_JUMP;
        camel_action_timer = CAMEL_JUMP_FRAMES;
    }
    else if (camel_action == CAMEL_ACTION_NORMAL && joystick_down(dir) && !joystick_down(last_direction)) {
        camel_action = CAMEL_ACTION_DUCK;
        camel_action_timer = CAMEL_DUCK_FRAMES;
    }

    last_direction = dir;

    // Smooth lane-change animation.
    int target_x = lane_x[target_lane];
    int lane_step = 16;

    if (player_x < target_x) {
        player_x += lane_step;
        if (player_x > target_x) {
            player_x = target_x;
        }
    }
    else if (player_x > target_x) {
        player_x -= lane_step;
        if (player_x < target_x) {
            player_x = target_x;
        }
    }

    // Once the camel reaches the selected lane, make that lane active and show
    // the normal back view again.
    if (player_x == target_x) {
        player_lane = target_lane;
        camel_pose = CAMEL_BACK;
    }

    int anim_delay = (camel_pose == CAMEL_BACK) ? 4 : 1;
    int frame_count = (camel_pose == CAMEL_BACK) ? CAMEL_BACK_RUN_FRAME_COUNT : CAMEL_SIDE_RUN_FRAME_COUNT;

    camel_anim_timer++;
    if (camel_anim_timer >= anim_delay) {
        camel_anim_timer = 0;
        camel_run_frame++;
        if (camel_run_frame >= frame_count) {
            camel_run_frame = 0;
        }
    }

    if (camel_action_timer > 0) {
        camel_action_timer--;
        if (camel_action_timer == 0) {
            camel_action = CAMEL_ACTION_NORMAL;
        }
    }

    int score_multiplier = (score_multiplier_timer > 0) ? 2 : 1;
    score += score_multiplier;
    if (score_multiplier_timer > 0) {
        score_multiplier_timer--;
    }

    if (score >= next_score_flash) {
        static const uint8_t milestone_colours[][3] = {
            {0, 0, 1},  // blue
            {0, 1, 0},  // green
            {1, 1, 0},  // yellow
            {1, 0, 1},  // purple
            {0, 1, 1},  // cyan
            {1, 1, 1}   // white
        };
        int colour_index = (next_score_flash / SCORE_FLASH_STEP - 1) %
                           (int)(sizeof(milestone_colours) / sizeof(milestone_colours[0]));
        rgb_start_flash(1,
                        milestone_colours[colour_index][0],
                        milestone_colours[colour_index][1],
                        milestone_colours[colour_index][2],
                        24,
                        3);
        next_score_flash += SCORE_FLASH_STEP;
    }

    // Speed increases slowly as the score goes up.
    int speed = 4 + (score / 140);
    if (speed > 11) {
        speed = 11;
    }

    spawn_timer++;
    spawn_delay = 28 - (score / 100);
    if (spawn_delay < 12) {
        spawn_delay = 12;
    }

    if (spawn_timer >= spawn_delay) {
        spawn_timer = 0;
        spawn_object();
    }

    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) {
            continue;
        }

        objects[i].y += speed;

        if (object_hits_player(&objects[i])) {
            if (object_is_dodged(&objects[i])) {
                score += 10 * score_multiplier;
                objects[i].active = 0;
                rgb_start_flash(1, 0, 1, 0, 12, 3);
                play_sfx(980, 8, 3);
            }
            else if (object_is_collectible(objects[i].type)) {
                if (objects[i].type == OBJ_DATE) {
                    score_multiplier_timer = DATE_MULTIPLIER_FRAMES;
                    rgb_start_flash(1, 1, 0, 1, 24, 4);
                    play_sfx(1200, 12, 4);
                }
                else if (objects[i].type == OBJ_WATER) {
                    if (lives < MAX_LIVES) {
                        lives++;
                    }
                    rgb_start_flash(1, 0, 0, 1, 18, 3);
                    play_sfx(1500, 13, 4);
                }
                else {
                    score += COIN_BONUS_POINTS * score_multiplier;
                    rgb_start_flash(1, 0, 0, 1, 16, 3);
                    play_sfx(1700, 13, 4);
                }

                objects[i].active = 0;
            }
            else if (lives > 1) {
                lives--;
                objects[i].active = 0;
                rgb_start_flash(0, 1, 0, 0, 20, 3);
                play_sfx(260, 12, 5);
            }
            else {
                runner_state = GAME_OVER;
                if (score > high_score) {
                    high_score = score;
                }
                PWM_SetDuty(&pwm_cfg, 5);
                rgb_start_flash(0, 1, 0, 0, 48, 2);
                rgb_start_flash(1, 1, 0, 0, 48, 2);
                start_game_over_sound();
                return;
            }
        }

        if (objects[i].y > SCREEN_H) {
            objects[i].active = 0;
        }
    }

    update_tension_music();
    rgb_update_effects();

    // LED gets brighter as the score rises, giving extra visual feedback.
    int brightness = 20 + (score / 8);
    if (brightness > 100) {
        brightness = 100;
    }
    PWM_SetDuty(&pwm_cfg, brightness);
}

static void draw_background(void)
{
    LCD_Draw_Sprite(0, 0, GAMEPLAY_BACKGROUND_H, GAMEPLAY_BACKGROUND_W,
                    gameplay_background_sprite);
    LCD_Draw_Sprite(0, 0, STREET_OVERLAY_H, STREET_OVERLAY_W,
                    street_overlay_sprite);
}

static void draw_camel(int x, int y, CamelPose pose)
{
    // Default view is from behind because the camel is running away from the player.
    // Jump/duck use a special flat Hamoodi sprite drawn by the same action controls.
    if (camel_action != CAMEL_ACTION_NORMAL) {
        LCD_Draw_Sprite(x + 4, y + 2, CAMEL_ACTION_SPRITE_H, CAMEL_ACTION_SPRITE_W,
                        camel_action_sprite);
        return;
    }

    if (pose == CAMEL_FACE_LEFT) {
        int side_frame = camel_run_frame % CAMEL_SIDE_RUN_FRAME_COUNT;
        LCD_Draw_Sprite(x + 4, y + 2, CAMEL_SIDE_RUN_H, CAMEL_SIDE_RUN_W,
                        camel_side_right_frames[side_frame]);
    }
    else if (pose == CAMEL_FACE_RIGHT) {
        int side_frame = camel_run_frame % CAMEL_SIDE_RUN_FRAME_COUNT;
        LCD_Draw_Sprite(x + 4, y + 2, CAMEL_SIDE_RUN_H, CAMEL_SIDE_RUN_W,
                        camel_side_left_frames[side_frame]);
    }
    else {
        int back_frame = camel_run_frame % CAMEL_BACK_RUN_FRAME_COUNT;
        LCD_Draw_Sprite(x + 4, y + 2, CAMEL_BACK_RUN_H, CAMEL_BACK_RUN_W,
                        camel_back_run_frames[back_frame]);
    }
}

static void draw_rock(int x, int y)
{
    LCD_Draw_Sprite(x, y, GAME1_OBJECT_SPRITE_H, GAME1_OBJECT_SPRITE_W, rock_sprite);
}

static void draw_cactus(int x, int y)
{
    LCD_Draw_Sprite(x, y, GAME1_OBJECT_SPRITE_H, GAME1_OBJECT_SPRITE_W, cactus_sprite);
}

static void draw_snake(int x, int y)
{
    LCD_Draw_Sprite(x, y, GAME1_OBJECT_SPRITE_H, GAME1_OBJECT_SPRITE_W, snake_sprite);
}

static void draw_eagle(int x, int y)
{
    LCD_Draw_Sprite(x, y, GAME1_OBJECT_SPRITE_H, GAME1_OBJECT_SPRITE_W, eagle_sprite);
}

static void draw_crack(int x, int y)
{
    LCD_Draw_Sprite(x, y, GAME1_OBJECT_SPRITE_H, GAME1_OBJECT_SPRITE_W, crack_sprite);
}

static void draw_date(int x, int y)
{
    LCD_Draw_Sprite(x, y, GAME1_OBJECT_SPRITE_H, GAME1_OBJECT_SPRITE_W, date_sprite);
}

static void draw_water_drop(int x, int y)
{
    LCD_Draw_Sprite(x, y, GAME1_OBJECT_SPRITE_H, GAME1_OBJECT_SPRITE_W, water_drop_sprite);
}

static void draw_coin(int x, int y)
{
    LCD_Draw_Sprite(x, y, GAME1_OBJECT_SPRITE_H, GAME1_OBJECT_SPRITE_W, coin_sprite);
}

static void draw_hud_heart(int x, int y)
{
    LCD_Draw_Sprite(x, y, GAME1_OBJECT_SPRITE_H, GAME1_OBJECT_SPRITE_W, heart_sprite);
}

static void print_outlined(char const *text, uint16_t x, uint16_t y, uint8_t colour, uint8_t outline_colour, uint8_t font_size)
{
    LCD_printString(text, x - 1, y, outline_colour, font_size);
    LCD_printString(text, x + 1, y, outline_colour, font_size);
    LCD_printString(text, x, y - 1, outline_colour, font_size);
    LCD_printString(text, x, y + 1, outline_colour, font_size);
    LCD_printString(text, x, y, colour, font_size);
}

static void render_start_screen(void)
{
    uint32_t phase = (HAL_GetTick() / 190) % 4;
    int hamoodi_x_offset = 0;
    int hamoodi_frame = 0;

    if (phase == 0) {
        hamoodi_x_offset = -3;
        hamoodi_frame = 0;
    }
    else if (phase == 1) {
        hamoodi_x_offset = -1;
        hamoodi_frame = 0;
    }
    else if (phase == 2) {
        hamoodi_x_offset = 3;
        hamoodi_frame = 1;
    }
    else {
        hamoodi_x_offset = 1;
        hamoodi_frame = 1;
    }

    LCD_Set_Palette(PALETTE_GAME1_START);
    LCD_Draw_Sprite(0, 0, GAME1_START_SCREEN_H, GAME1_START_SCREEN_W, start_screen_sprite);
    LCD_Draw_Sprite(104 + hamoodi_x_offset, 154, START_HAMOODI_H, START_HAMOODI_W,
                    start_hamoodi_frames[hamoodi_frame]);

    print_outlined("MOVE JOYSTICK", 84, 207, COL_WHITE, COL_BLACK, 1);
    print_outlined("BT2 START", 34, 225, COL_WHITE, COL_BLACK, 1);
    print_outlined("BT3 MENU", 154, 225, COL_WHITE, COL_BLACK, 1);

    LCD_Refresh(&cfg0);
}

static void render_game(void)
{
    char text[32];

    LCD_Set_Palette(PALETTE_GAME1_PLAY);
    draw_background();

    sprintf(text, "Score:%d", score);
    print_outlined(text, 5, 6, COL_WHITE, COL_BLACK, 1);

    sprintf(text, "Best:%d", high_score);
    print_outlined(text, 150, 6, COL_WHITE, COL_BLACK, 1);

    for (int i = 0; i < lives; i++) {
        draw_hud_heart(75 + (i * 16), 0);
    }

    if (score_multiplier_timer > 0) {
        LCD_printString("2X", 134, 6, COL_YELLOW, 1);
    }

    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) {
            continue;
        }

        int x = lane_x[objects[i].lane] - (OBJECT_W / 2);
        int y = objects[i].y;

        if (objects[i].type == OBJ_ROCK) {
            draw_rock(x, y);
        }
        else if (objects[i].type == OBJ_CACTUS) {
            draw_cactus(x, y);
        }
        else if (objects[i].type == OBJ_SNAKE) {
            draw_snake(x, y);
        }
        else if (objects[i].type == OBJ_EAGLE) {
            draw_eagle(x, y);
        }
        else if (objects[i].type == OBJ_CRACK) {
            draw_crack(x, y);
        }
        else if (objects[i].type == OBJ_DATE) {
            draw_date(x, y);
        }
        else if (objects[i].type == OBJ_WATER) {
            draw_water_drop(x, y);
        }
        else {
            draw_coin(x, y);
        }
    }

    draw_camel(player_x - 20, get_camel_draw_y(), camel_pose);

    LCD_Refresh(&cfg0);
}

static void render_game_over(void)
{
    char text[32];

    LCD_Set_Palette(PALETTE_GAME1_GAME_OVER);
    LCD_Draw_Sprite(0, 0, GAME1_GAME_OVER_SCREEN_H, GAME1_GAME_OVER_SCREEN_W,
                    game_over_screen_sprite);

    sprintf(text, "Score: %d", score);
    print_outlined(text, 46, 169, COL_WHITE, COL_BLACK, 2);

    sprintf(text, "Best: %d", high_score);
    print_outlined(text, 56, 193, COL_WHITE, COL_BLACK, 2);

    print_outlined("BT2 Restart", 25, 224, COL_RED, COL_BLACK, 1);
    print_outlined("BT3 Menu", 152, 224, COL_RED, COL_BLACK, 1);

    LCD_Refresh(&cfg0);
}

MenuState Game1_Run(void)
{
    runner_state = GAME_START_SCREEN;
    high_score = 0;
    reset_game();

    // Basic random seed. This makes obstacle order different each run.
    srand(HAL_GetTick());

    buzzer_tone(&buzzer_cfg, 950, 10);
    HAL_Delay(70);
    buzzer_off(&buzzer_cfg);

    while (1) {
        uint32_t frame_start = HAL_GetTick();

        Input_Read();
        Joystick_Read(&joystick_cfg, &joystick_data);

        // BT3 always exits back to the shared menu.
        if (current_input.btn3_pressed) {
            PWM_SetDuty(&pwm_cfg, 50);
            buzzer_off(&buzzer_cfg);
            rgb_all_off();
            LCD_Set_Palette(PALETTE_CUSTOM);
            return MENU_STATE_HOME;
        }

        if (runner_state == GAME_START_SCREEN) {
            if (current_input.btn2_pressed) {
                reset_game();
                runner_state = GAME_RUNNING;
            }
            render_start_screen();
        }
        else if (runner_state == GAME_RUNNING) {
            update_game();
            if (runner_state == GAME_OVER) {
                render_game_over();
                update_game_over_sound();
                rgb_update_effects();
            }
            else {
                render_game();
            }
        }
        else if (runner_state == GAME_OVER) {
            if (current_input.btn2_pressed) {
                reset_game();
                runner_state = GAME_RUNNING;
                PWM_SetDuty(&pwm_cfg, 50);
            }
            else {
                render_game_over();
                update_game_over_sound();
                rgb_update_effects();
            }
        }

        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < GAME1_FRAME_TIME_MS) {
            HAL_Delay(GAME1_FRAME_TIME_MS - frame_time);
        }
    }
}
