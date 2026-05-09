#include "Game_1.h"
#include "camel_back_run_sprite.h"
#include "camel_side_run_sprite.h"
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
#define OBJECT_W 28
#define OBJECT_H 26
#define MAX_OBJECTS 4

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
    OBJ_DATE
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
static int spawn_timer;
static int spawn_delay;
static Direction last_direction;
static int camel_run_frame;
static int camel_anim_timer;

typedef enum {
    CAMEL_BACK = 0,
    CAMEL_FACE_LEFT,
    CAMEL_FACE_RIGHT
} CamelPose;

static CamelPose camel_pose;

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
static void draw_date(int x, int y);
static uint8_t object_hits_player(DesertObject *obj);
static uint8_t joystick_left(Direction dir);
static uint8_t joystick_right(Direction dir);

static void reset_game(void)
{
    player_lane = 1;       // start in middle lane
    target_lane = player_lane;
    player_x = lane_x[player_lane];  // actual camel screen position starts in the middle
    score = 0;
    spawn_timer = 0;
    spawn_delay = 34;
    last_direction = CENTRE;
    camel_pose = CAMEL_BACK;
    camel_run_frame = 0;
    camel_anim_timer = 0;

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

static void spawn_object(void)
{
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) {
            objects[i].active = 1;
            objects[i].lane = rand() % 3;
            objects[i].y = 28;

            // Most objects are dangerous. Sometimes a date appears as a bonus item.
            int r = rand() % 5;
            if (r == 0) {
                objects[i].type = OBJ_DATE;
            }
            else if (r == 1 || r == 2) {
                objects[i].type = OBJ_CACTUS;
            }
            else {
                objects[i].type = OBJ_ROCK;
            }
            break;
        }
    }
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
            buzzer_tone(&buzzer_cfg, 700, 15);
        }
    }
    else if (!camel_is_switching_lanes && joystick_right(dir) && !joystick_right(last_direction)) {
        if (target_lane < 2) {
            target_lane++;
            camel_pose = CAMEL_FACE_RIGHT;
            buzzer_tone(&buzzer_cfg, 700, 15);
        }
    }
    else {
        buzzer_off(&buzzer_cfg);
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

    camel_anim_timer++;
    if (camel_anim_timer >= anim_delay) {
        camel_anim_timer = 0;
        camel_run_frame++;
        if (camel_run_frame >= CAMEL_BACK_RUN_FRAME_COUNT) {
            camel_run_frame = 0;
        }
    }

    score++;

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
            if (objects[i].type == OBJ_DATE) {
                // Collecting dates gives bonus points instead of game over.
                score += 50;
                objects[i].active = 0;
                buzzer_tone(&buzzer_cfg, 1200, 30);
            }
            else {
                runner_state = GAME_OVER;
                if (score > high_score) {
                    high_score = score;
                }
                PWM_SetDuty(&pwm_cfg, 5);
                buzzer_tone(&buzzer_cfg, 180, 50);
                HAL_Delay(120);
                buzzer_off(&buzzer_cfg);
                return;
            }
        }

        if (objects[i].y > SCREEN_H) {
            objects[i].active = 0;
        }
    }

    // LED gets brighter as the score rises, giving extra visual feedback.
    int brightness = 20 + (score / 8);
    if (brightness > 100) {
        brightness = 100;
    }
    PWM_SetDuty(&pwm_cfg, brightness);
}

static void draw_pyramid(int x, int y, int w, int h, uint8_t colour, uint8_t shade_colour)
{
    int centre = x + (w / 2);

    for (int row = 0; row < h; row++) {
        int half_width = (row * w) / (2 * h);
        int yy = y + row;

        LCD_Draw_Line(centre - half_width, yy, centre, yy, colour);
        LCD_Draw_Line(centre, yy, centre + half_width, yy, shade_colour);
    }

    LCD_Draw_Line(centre, y, x, y + h, COL_BROWN);
    LCD_Draw_Line(centre, y, x + w, y + h, COL_BROWN);
    LCD_Draw_Line(x, y + h, x + w, y + h, COL_BROWN);
}

static void draw_background_cactus(int x, int y, uint8_t colour)
{
    LCD_Draw_Rect(x + 4, y, 4, 18, colour, 1);
    LCD_Draw_Rect(x, y + 8, 5, 3, colour, 1);
    LCD_Draw_Rect(x + 8, y + 5, 5, 3, colour, 1);
    LCD_Draw_Rect(x, y + 5, 3, 6, colour, 1);
    LCD_Draw_Rect(x + 10, y + 2, 3, 6, colour, 1);
}

static void draw_background(void)
{
    int dash_scroll = score % 24;
    int sand_scroll = (score * 2) % 136;

    LCD_Fill_Buffer(COL_SAND);

    // Sky and sun
    LCD_Draw_Rect(0, 0, SCREEN_W, 78, COL_SKY, 1);
    LCD_Draw_Circle(205, 28, 19, COL_ORANGE, 1);
    LCD_Draw_Circle(205, 28, 15, COL_YELLOW, 1);

    // Distant desert details
    draw_pyramid(18, 49, 48, 34, COL_SAND, COL_ORANGE);
    draw_pyramid(58, 57, 34, 25, COL_SAND, COL_ORANGE);
    draw_pyramid(160, 53, 46, 31, COL_SAND, COL_ORANGE);
    LCD_Draw_Circle(38, 98, 36, COL_SAND, 1);
    LCD_Draw_Circle(112, 100, 42, COL_SAND, 1);
    LCD_Draw_Circle(190, 99, 39, COL_SAND, 1);
    LCD_Draw_Rect(0, 78, SCREEN_W, 6, COL_ORANGE, 1);
    draw_background_cactus(13, 108, COL_CACTUS);
    draw_background_cactus(214, 118, COL_CACTUS);

    // Runner path with slight perspective.
    for (int y = 86; y < 236; y++) {
        int spread = ((y - 86) * 16) / 150;
        LCD_Draw_Line(34 - spread, y, 206 + spread, y, COL_BROWN);
    }

    LCD_Draw_Line(34, 86, 18, 236, COL_BLACK);
    LCD_Draw_Line(206, 86, 222, 236, COL_BLACK);

    // Scrolling path texture marks make the desert feel like it is moving.
    for (int i = 0; i < 7; i++) {
        int y = 96 + ((i * 23 + sand_scroll) % 136);
        int x = 52 + ((i * 37) % 112);
        int length = 10 + ((i % 3) * 4);

        LCD_Draw_Line(x, y, x + length, y, COL_SAND);
    }

    // Dashed lane dividers scroll downward to show forward running.
    for (int y = 92 + dash_scroll - 24; y < 230; y += 24) {
        if (y < 88) {
            continue;
        }

        int y2 = y + 13;
        if (y2 > 236) {
            y2 = 236;
        }

        int left_x1 = 91 - ((y - 88) * 11) / 148;
        int left_x2 = 91 - ((y2 - 88) * 11) / 148;
        int right_x1 = 149 + ((y - 88) * 11) / 148;
        int right_x2 = 149 + ((y2 - 88) * 11) / 148;

        LCD_Draw_Line(left_x1, y, left_x2, y2, COL_SAND);
        LCD_Draw_Line(right_x1, y, right_x2, y2, COL_SAND);
    }
}

static void draw_camel(int x, int y, CamelPose pose)
{
    // Camel drawn using simple shapes so no extra bitmap file is needed.
    // Default view is from behind because the camel is running away from the player.

    if (pose == CAMEL_FACE_LEFT) {
        LCD_Draw_Sprite(x + 4, y + 2, CAMEL_SIDE_RUN_H, CAMEL_SIDE_RUN_W,
                        camel_side_right_frames[camel_run_frame]);
    }
    else if (pose == CAMEL_FACE_RIGHT) {
        LCD_Draw_Sprite(x + 4, y + 2, CAMEL_SIDE_RUN_H, CAMEL_SIDE_RUN_W,
                        camel_side_left_frames[camel_run_frame]);
    }
    else {
        LCD_Draw_Sprite(x + 4, y + 2, CAMEL_BACK_RUN_H, CAMEL_BACK_RUN_W,
                        camel_back_run_frames[camel_run_frame]);
    }
}

static void draw_rock(int x, int y)
{
    LCD_Draw_Circle(x + 14, y + 14, 12, COL_GREY, 1);
    LCD_Draw_Circle(x + 9, y + 10, 6, COL_BLACK, 0);
}

static void draw_cactus(int x, int y)
{
    LCD_Draw_Rect(x + 12, y + 2, 8, 24, COL_CACTUS, 1);
    LCD_Draw_Rect(x + 5, y + 11, 8, 6, COL_CACTUS, 1);
    LCD_Draw_Rect(x + 20, y + 8, 8, 6, COL_CACTUS, 1);
    LCD_Draw_Rect(x + 5, y + 7, 5, 7, COL_CACTUS, 1);
    LCD_Draw_Rect(x + 23, y + 4, 5, 7, COL_CACTUS, 1);
}

static void draw_date(int x, int y)
{
    LCD_Draw_Circle(x + 14, y + 14, 9, COL_ORANGE, 1);
    LCD_Draw_Rect(x + 12, y + 5, 5, 4, COL_CACTUS, 1);
    LCD_Draw_Circle(x + 14, y + 14, 4, COL_BROWN, 0);
}

static void render_start_screen(void)
{
    LCD_Fill_Buffer(COL_SKY);
    LCD_Draw_Rect(0, 120, SCREEN_W, 120, COL_SAND, 1);
    LCD_Draw_Circle(205, 35, 18, COL_YELLOW, 1);

    LCD_printString("CAMEL RUN", 43, 34, COL_BROWN, 3);
    LCD_printString("Desert Runner", 45, 70, COL_BLACK, 2);

    draw_camel(95, 112, CAMEL_FACE_RIGHT);

    LCD_printString("Move: joystick", 45, 170, COL_BLACK, 1);
    LCD_printString("BT2: start", 55, 188, COL_BLACK, 1);
    LCD_printString("BT3: menu", 58, 205, COL_BLACK, 1);

    LCD_Refresh(&cfg0);
}

static void render_game(void)
{
    char text[32];

    draw_background();

    sprintf(text, "Score:%d", score);
    LCD_printString(text, 5, 6, COL_BLACK, 1);

    sprintf(text, "Best:%d", high_score);
    LCD_printString(text, 150, 6, COL_BLACK, 1);

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
        else {
            draw_date(x, y);
        }
    }

    draw_camel(player_x - 20, PLAYER_Y, camel_pose);

    LCD_Refresh(&cfg0);
}

static void render_game_over(void)
{
    char text[32];

    LCD_Fill_Buffer(COL_SAND);
    LCD_printString("GAME OVER", 42, 40, COL_RED, 3);

    sprintf(text, "Score: %d", score);
    LCD_printString(text, 60, 90, COL_BLACK, 2);

    sprintf(text, "Best: %d", high_score);
    LCD_printString(text, 64, 115, COL_BLACK, 2);

    LCD_printString("BT2: restart", 50, 165, COL_BLACK, 1);
    LCD_printString("BT3: menu", 60, 185, COL_BLACK, 1);

    LCD_Refresh(&cfg0);
}

MenuState Game1_Run(void)
{
    runner_state = GAME_START_SCREEN;
    high_score = 0;
    reset_game();

    // Basic random seed. This makes obstacle order different each run.
    srand(HAL_GetTick());

    buzzer_tone(&buzzer_cfg, 950, 25);
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
            render_game();
        }
        else if (runner_state == GAME_OVER) {
            if (current_input.btn2_pressed) {
                reset_game();
                runner_state = GAME_RUNNING;
                PWM_SetDuty(&pwm_cfg, 50);
            }
            render_game_over();
        }

        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < GAME1_FRAME_TIME_MS) {
            HAL_Delay(GAME1_FRAME_TIME_MS - frame_time);
        }
    }
}
