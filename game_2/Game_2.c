#include "Game_2.h"
#include "InputHandler.h"
#include "Menu.h"
#include "LCD.h"
#include "Buzzer.h"
#include "Joystick.h"
#include "SharedGameState.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include "camel_sprite.h"
#include "room_backgrounds.h"
#include "pet_item_sprites.h"
#include "care_cover_sprite.h"

extern ST7789V2_cfg_t cfg0;
extern Buzzer_cfg_t buzzer_cfg;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;

#define GAME2_FRAME_TIME_MS 40

#define COL_BLACK      0
#define COL_WHITE      1
#define COL_BG         2
#define COL_BROWN      3
#define COL_CAMEL      4
#define COL_NOSE       5
#define COL_CREAM      6
#define COL_PINK       7
#define COL_LAVENDER   8
#define COL_GREY       9
#define COL_SAGE       10
#define COL_RED        11
#define COL_GREEN      12
#define COL_BLUE       13
#define COL_YELLOW     14
#define COL_ORANGE     15

#define LED1_R_PORT GPIOA
#define LED1_R_PIN  GPIO_PIN_9
#define LED1_G_PORT GPIOC
#define LED1_G_PIN  GPIO_PIN_7
#define LED1_B_PORT GPIOC
#define LED1_B_PIN  GPIO_PIN_8

#define LED2_R_PORT GPIOC
#define LED2_R_PIN  GPIO_PIN_9
#define LED2_G_PORT GPIOD
#define LED2_G_PIN  GPIO_PIN_2
#define LED2_B_PORT GPIOA
#define LED2_B_PIN  GPIO_PIN_5

typedef enum {
    PET_SCREEN_DASHBOARD = 0,
    PET_SCREEN_KITCHEN,
    PET_SCREEN_BATH,
    PET_SCREEN_DOCTOR,
    PET_SCREEN_BEDROOM
} PetScreen;

typedef enum {
    STAT_HUNGER = 0,
    STAT_HEALTH,
    STAT_CLEANLINESS,
    STAT_ENERGY,
    STAT_BACK
} PetStatOption;

static int hunger = 70;
static int health = 70;
static int cleanliness = 70;
static int energy = 70;

static int selected_stat = STAT_HUNGER;
static PetScreen current_screen = PET_SCREEN_DASHBOARD;
static int tool_x = 36;
static int tool_y = 178;
static uint8_t bedroom_sleeping = 0;
static uint32_t sleep_start = 0;
static uint8_t selected_food = 0;
static uint8_t carrying_food = 0;
static uint8_t full_chime_played[4] = {0, 0, 0, 0};
static uint32_t rainbow_party_until = 0;
static uint8_t care_music_index = 0;
static uint8_t care_music_note_active = 0;
static uint32_t care_music_next_note = 0;
static uint32_t care_music_note_off = 0;

/* -------------------------------------------------------------------------- */
/* LED helpers                                                                 */
/* -------------------------------------------------------------------------- */

static void RGB_Off(void)
{
    HAL_GPIO_WritePin(LED1_R_PORT, LED1_R_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED1_G_PORT, LED1_G_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED1_B_PORT, LED1_B_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_R_PORT, LED2_R_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_G_PORT, LED2_G_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_B_PORT, LED2_B_PIN, GPIO_PIN_RESET);
}

static void RGB_Set(uint8_t r, uint8_t g, uint8_t b)
{
    // LED1 is wired as: PA9=green, PC7=blue, PC8=red.
    HAL_GPIO_WritePin(LED1_B_PORT, LED1_B_PIN, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED1_R_PORT, LED1_R_PIN, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED1_G_PORT, LED1_G_PIN, b ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // LED2 mirrors LED1 using the planned wiring: PC9=red, PD2=green, PA5=blue.
    HAL_GPIO_WritePin(LED2_R_PORT, LED2_R_PIN, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_G_PORT, LED2_G_PIN, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_B_PORT, LED2_B_PIN, b ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* -------------------------------------------------------------------------- */
/* General game helpers                                                        */
/* -------------------------------------------------------------------------- */

static void show_care_cover(void)
{
    RGB_Set(1, 1, 1);
    LCD_Fill_Buffer(COL_BG);
    LCD_Draw_Sprite(0, 0, CARE_COVER_HEIGHT, CARE_COVER_WIDTH, care_cover_sprite);
    LCD_Refresh(&cfg0);
    buzzer_note(&buzzer_cfg, NOTE_A5, 10);
    HAL_Delay(90);
    buzzer_off(&buzzer_cfg);
    HAL_Delay(1110);
}

static void clamp_stats(void)
{
    if (hunger > 100) hunger = 100;
    if (health > 100) health = 100;
    if (cleanliness > 100) cleanliness = 100;
    if (energy > 100) energy = 100;

    if (hunger < 0) hunger = 0;
    if (health < 0) health = 0;
    if (cleanliness < 0) cleanliness = 0;
    if (energy < 0) energy = 0;
}

static void update_rgb_status(void)
{
    int stat_value = 100;
    uint32_t now = HAL_GetTick();

    if (now < rainbow_party_until) {
        uint8_t step = (now / 45) % 12;

        if (step == 0 || step == 1) RGB_Set(1, 0, 0);
        else if (step == 2 || step == 3) RGB_Set(1, 1, 0);
        else if (step == 4 || step == 5) RGB_Set(0, 1, 0);
        else if (step == 6 || step == 7) RGB_Set(0, 1, 1);
        else if (step == 8 || step == 9) RGB_Set(0, 0, 1);
        else RGB_Set(1, 0, 1);
        return;
    }

    if (current_screen == PET_SCREEN_DASHBOARD) {
        if (selected_stat == STAT_HUNGER) stat_value = hunger;
        else if (selected_stat == STAT_HEALTH) stat_value = health;
        else if (selected_stat == STAT_CLEANLINESS) stat_value = cleanliness;
        else if (selected_stat == STAT_ENERGY) stat_value = energy;
    }
    else if (current_screen == PET_SCREEN_KITCHEN) {
        stat_value = hunger;
    }
    else if (current_screen == PET_SCREEN_DOCTOR) {
        stat_value = health;
    }
    else if (current_screen == PET_SCREEN_BATH) {
        stat_value = cleanliness;
    }
    else if (current_screen == PET_SCREEN_BEDROOM) {
        stat_value = energy;
    }

    if (stat_value < 35) {
        RGB_Set(1, 0, 0);
    }
    else if (stat_value < 75) {
        RGB_Set(1, 1, 1);
    }
    else {
        RGB_Set(0, 1, 0);
    }
}

static void draw_bar(int x, int y, int value, uint8_t colour)
{
    LCD_Draw_Rect(x, y, 76, 9, COL_BLACK, 0);
    LCD_Draw_Rect(x + 1, y + 1, (value * 74) / 100, 7, colour, 1);
}

static void draw_pet(int x, int y, uint8_t scale)
{
    for (uint8_t row = 0; row < 32; row++) {
        for (uint8_t col = 0; col < 32; col++) {
            uint8_t pixel = camel_sprite[row * 32 + col];

            if (pixel == 255) {
                continue;
            }

            // The room backgrounds use palette index 10 for green.
            // Older camel art used index 10 for warm shadow, so remap it here.
            if (pixel == 10) {
                pixel = COL_NOSE;
            }

            for (uint8_t dy = 0; dy < scale; dy++) {
                for (uint8_t dx = 0; dx < scale; dx++) {
                    LCD_Set_Pixel(x + col * scale + dx, y + row * scale + dy, pixel);
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Shared drawing helpers                                                      */
/* -------------------------------------------------------------------------- */

static void draw_background(const uint8_t *background)
{
    LCD_Fill_Buffer(COL_BLACK);
    LCD_Draw_Sprite(0, 0, ROOM_BG_HEIGHT, ROOM_BG_WIDTH, background);
}

static uint8_t tool_over_back_button(void)
{
    return tool_x >= 6 && tool_x <= 30 && tool_y >= 6 && tool_y <= 30;
}

static void draw_back_arrow_button(uint8_t selected)
{
    uint8_t fill_colour = selected ? COL_CREAM : COL_WHITE;
    LCD_Draw_Rect(6, 6, 24, 24, fill_colour, 1);
    LCD_Draw_Rect(6, 6, 24, 24, COL_BROWN, 0);
    LCD_Draw_Line(21, 12, 13, 18, COL_BROWN);
    LCD_Draw_Line(13, 18, 21, 24, COL_BROWN);
    LCD_Draw_Line(14, 18, 24, 18, COL_BROWN);
}

static void draw_room_back_button(void)
{
    draw_back_arrow_button(tool_over_back_button());
}

static void draw_room_stat(const char *label, int value, uint8_t colour)
{
    LCD_printString((char*)label, 8, 228, COL_BLACK, 1);
    draw_bar(76, 228, value, colour);
}

static void play_success_chime(void)
{
    buzzer_note(&buzzer_cfg, NOTE_G5, 14);
    HAL_Delay(55);
    buzzer_off(&buzzer_cfg);
    HAL_Delay(25);
    buzzer_note(&buzzer_cfg, NOTE_C6, 14);
    HAL_Delay(85);
    buzzer_off(&buzzer_cfg);
}

/* -------------------------------------------------------------------------- */
/* Care mode background music                                                  */
/* -------------------------------------------------------------------------- */

static void care_music_stop(void)
{
    care_music_note_active = 0;
    buzzer_off(&buzzer_cfg);
}

static void care_music_reset(void)
{
    care_music_index = 0;
    care_music_note_active = 0;
    care_music_next_note = HAL_GetTick() + 900;
    care_music_note_off = 0;
}

static void care_music_update(void)
{
    static const Buzzer_Note_t notes[] = {
        NOTE_E5, NOTE_G5, NOTE_A5, NOTE_G5,
        NOTE_D5, NOTE_E5, NOTE_G5, NOTE_E5
    };
    static const uint16_t durations[] = {120, 120, 190, 150, 140, 120, 180, 320};
    uint32_t now = HAL_GetTick();

    if (current_screen == PET_SCREEN_BEDROOM && bedroom_sleeping) {
        return;
    }

    if (care_music_note_active) {
        if (now >= care_music_note_off) {
            buzzer_off(&buzzer_cfg);
            care_music_note_active = 0;
            care_music_next_note = now + 420;
        }
        return;
    }

    if (now < care_music_next_note || now < rainbow_party_until || buzzer_is_running(&buzzer_cfg)) {
        return;
    }

    buzzer_note(&buzzer_cfg, notes[care_music_index], 5);
    care_music_note_off = now + durations[care_music_index];
    care_music_note_active = 1;
    care_music_index++;

    if (care_music_index >= (sizeof(notes) / sizeof(notes[0]))) {
        care_music_index = 0;
        care_music_next_note = now + 1400;
    }
}

static void draw_stat_row(const char *label, int y, int value, uint8_t colour, int option)
{
    uint8_t text_colour = selected_stat == option ? COL_BROWN : COL_BLACK;

    if (selected_stat == option) {
        LCD_printString(">", 42, y, COL_BROWN, 1);
    }

    LCD_printString((char*)label, 56, y, text_colour, 1);
    draw_bar(132, y, value, colour);
}

static uint8_t current_room_stat_full(void)
{
    if (current_screen == PET_SCREEN_KITCHEN) return hunger >= 100;
    if (current_screen == PET_SCREEN_BATH) return cleanliness >= 100;
    if (current_screen == PET_SCREEN_DOCTOR) return health >= 100;
    if (current_screen == PET_SCREEN_BEDROOM) return energy >= 100;
    return 0;
}

static int current_room_stat_index(void)
{
    if (current_screen == PET_SCREEN_KITCHEN) return STAT_HUNGER;
    if (current_screen == PET_SCREEN_DOCTOR) return STAT_HEALTH;
    if (current_screen == PET_SCREEN_BATH) return STAT_CLEANLINESS;
    if (current_screen == PET_SCREEN_BEDROOM) return STAT_ENERGY;
    return -1;
}

static void update_full_chime_state(void)
{
    int values[] = {hunger, health, cleanliness, energy};

    for (uint8_t i = STAT_HUNGER; i <= STAT_ENERGY; i++) {
        if (values[i] < 100) {
            full_chime_played[i] = 0;
        }
    }

    int room_index = current_room_stat_index();
    if (room_index >= 0 && values[room_index] >= 100 && !full_chime_played[room_index]) {
        full_chime_played[room_index] = 1;
        rainbow_party_until = HAL_GetTick() + 2500;
        care_music_stop();
        play_success_chime();
    }
}

static const char* current_room_full_message(void)
{
    if (current_screen == PET_SCREEN_KITCHEN) return "full hunger!";
    if (current_screen == PET_SCREEN_BATH) return "clean!";
    if (current_screen == PET_SCREEN_DOCTOR) return "full health!";
    return "full energy!";
}

static void draw_confetti_message(void)
{
    if (!current_room_stat_full()) {
        return;
    }

    uint8_t shift = (HAL_GetTick() / 160) % 16;
    LCD_Draw_Rect(50, 48, 140, 28, COL_WHITE, 1);
    LCD_Draw_Rect(50, 48, 140, 28, COL_BROWN, 0);
    LCD_printString((char*)current_room_full_message(), 64, 58, COL_BROWN, 1);

    for (uint8_t i = 0; i < 8; i++) {
        uint16_t x = 28 + i * 25;
        uint16_t y = 28 + ((i * 9 + shift) % 70);
        uint8_t c = (i % 3 == 0) ? COL_ORANGE : ((i % 3 == 1) ? COL_PINK : COL_YELLOW);
        LCD_Draw_Rect(x, y, 4, 4, c, 1);
    }
}

static void render_dashboard(void)
{
    draw_background(room_main_bg);
    draw_back_arrow_button(selected_stat == STAT_BACK);
    draw_stat_row("Hunger", 18, hunger, COL_ORANGE, STAT_HUNGER);
    draw_stat_row("Health", 40, health, COL_SAGE, STAT_HEALTH);
    draw_stat_row("Cleanliness", 62, cleanliness, COL_BLUE, STAT_CLEANLINESS);
    draw_stat_row("Energy", 84, energy, COL_LAVENDER, STAT_ENERGY);

    draw_pet(72, 118, 3);
    LCD_Refresh(&cfg0);
}

/* -------------------------------------------------------------------------- */
/* Room rendering                                                              */
/* -------------------------------------------------------------------------- */

static const char* food_label(uint8_t food)
{
    static const char *labels[] = {"Kabsa", "Dates", "Gahwa", "Basboosa"};
    return labels[food];
}

static int food_value(uint8_t food)
{
    static const int values[] = {25, 12, 8, 18};
    return values[food];
}

static void draw_food_icon(int x, int y, uint8_t food)
{
    LCD_Draw_Sprite(x - 10, y - 10, FOOD_SPRITE_SIZE, FOOD_SPRITE_SIZE, food_sprites[food]);
}

static void draw_cursor_cross(int x, int y, uint8_t colour)
{
    LCD_Draw_Line(x - 7, y, x + 7, y, colour);
    LCD_Draw_Line(x, y - 7, x, y + 7, colour);
    LCD_Draw_Rect(x - 2, y - 2, 5, 5, colour, 0);
}

static void draw_food_selection(void)
{
    const uint16_t xs[] = {28, 84, 144, 204};

    for (uint8_t i = 0; i < 4; i++) {
        if (selected_food == i) {
            LCD_printString("^", xs[i] - 3, 184, COL_BROWN, 1);
        }
        draw_food_icon(xs[i], 202, i);
    }

    LCD_printString((char*)food_label(selected_food), 80, 218, COL_BLACK, 1);
}

static void render_kitchen(void)
{
    draw_background(room_kitchen_bg);
    draw_room_back_button();
    LCD_printString("KITCHEN", 72, 8, COL_BROWN, 2);

    draw_pet(84, 112, 2);
    draw_food_selection();

    if (carrying_food) {
        draw_food_icon(tool_x, tool_y, selected_food);
    }
    else {
        draw_cursor_cross(tool_x, tool_y, COL_BLACK);
    }

    draw_room_stat("Hunger", hunger, COL_ORANGE);
    draw_confetti_message();
    LCD_Refresh(&cfg0);
}

static void render_bath(void)
{
    uint8_t bubble_shift = (HAL_GetTick() / 220) % 18;

    draw_background(room_bathroom_bg);
    draw_room_back_button();
    LCD_printString("BATH", 92, 8, COL_WHITE, 2);

    draw_pet(90, 82, 2);
    for (uint8_t i = 0; i < 34; i++) {
        uint16_t y = 128 + i;
        uint16_t inset = i / 3;
        LCD_Draw_Line(46 + inset, y, 198 - inset, y, COL_WHITE);
    }
    LCD_Draw_Line(46, 128, 198, 128, COL_WHITE);
    LCD_Draw_Line(54, 162, 190, 162, COL_GREY);
    LCD_Draw_Circle(70, 106 - bubble_shift, 3, COL_WHITE, 1);
    LCD_Draw_Circle(168, 116 - bubble_shift, 4, COL_WHITE, 1);
    LCD_Draw_Circle(190, 96 - bubble_shift, 3, COL_WHITE, 1);

    LCD_Draw_Sprite(tool_x - 8, tool_y - 8, SOAP_SPRITE_SIZE, SOAP_SPRITE_SIZE, soap_sprite);
    draw_room_stat("Clean", cleanliness, COL_SAGE);
    draw_confetti_message();
    LCD_Refresh(&cfg0);
}

static void render_doctor(void)
{
    draw_background(room_doctor_bg);
    draw_room_back_button();
    LCD_printString("DOCTOR", 78, 8, COL_BROWN, 2);
    draw_pet(82, 112, 2);
    LCD_Draw_Sprite(tool_x - 16, tool_y - 16, BANDAID_SPRITE_SIZE, BANDAID_SPRITE_SIZE, bandaid_sprite);
    draw_room_stat("Health", health, COL_SAGE);
    draw_confetti_message();
    LCD_Refresh(&cfg0);
}

static void render_bedroom(void)
{
    if (bedroom_sleeping) {
        uint8_t z_shift = (HAL_GetTick() / 180) % 36;

        draw_background(room_lights_off_bg);
        LCD_printString("z", 78, 118 - z_shift, COL_WHITE, 1);
        LCD_printString("z", 96, 126 - z_shift, COL_WHITE, 1);
        LCD_printString("Z", 116, 136 - z_shift, COL_WHITE, 1);
        LCD_Refresh(&cfg0);
        return;
    }

    draw_background(room_bedroom_bg);
    draw_room_back_button();
    LCD_printString("BEDROOM", 70, 8, COL_BROWN, 2);
    draw_pet(82, 118, 2);
    LCD_Draw_Rect(tool_x - 3, tool_y - 3, 7, 7, COL_BLACK, 0);
    LCD_Draw_Line(tool_x - 7, tool_y, tool_x + 7, tool_y, COL_BLACK);
    LCD_Draw_Line(tool_x, tool_y - 7, tool_x, tool_y + 7, COL_BLACK);
    draw_room_stat("Energy", energy, COL_LAVENDER);
    draw_confetti_message();
    LCD_Refresh(&cfg0);
}

static void render_current_screen(void)
{
    if (current_screen == PET_SCREEN_DASHBOARD) {
        render_dashboard();
    }
    else if (current_screen == PET_SCREEN_KITCHEN) {
        render_kitchen();
    }
    else if (current_screen == PET_SCREEN_BATH) {
        render_bath();
    }
    else if (current_screen == PET_SCREEN_DOCTOR) {
        render_doctor();
    }
    else {
        render_bedroom();
    }
}

static void reset_room_tool(void)
{
    tool_x = 36;
    tool_y = 178;
    bedroom_sleeping = 0;
    carrying_food = 0;
}

static void enter_selected_room(void)
{
    reset_room_tool();

    if (selected_stat == STAT_HUNGER) {
        current_screen = PET_SCREEN_KITCHEN;
        tool_x = 36 + selected_food * 56;
        tool_y = 202;
    }
    else if (selected_stat == STAT_HEALTH) {
        current_screen = PET_SCREEN_DOCTOR;
    }
    else if (selected_stat == STAT_CLEANLINESS) {
        current_screen = PET_SCREEN_BATH;
    }
    else if (selected_stat == STAT_ENERGY) {
        current_screen = PET_SCREEN_BEDROOM;
    }
}

/* -------------------------------------------------------------------------- */
/* Room controls and game logic                                                */
/* -------------------------------------------------------------------------- */

static void move_tool(Direction direction)
{
    if (direction == N && tool_y > 24) tool_y -= 10;
    if (direction == S && tool_y < 216) tool_y += 10;
    if (direction == W && tool_x > 18) tool_x -= 10;
    if (direction == E && tool_x < 222) tool_x += 10;
}

static uint8_t close_to(int x, int y, int target_x, int target_y, int range)
{
    int dx = x - target_x;
    int dy = y - target_y;
    return (dx * dx + dy * dy) < (range * range);
}

static void update_room(Direction direction)
{
    if (current_screen == PET_SCREEN_BEDROOM && bedroom_sleeping) {
        if (HAL_GetTick() - sleep_start > 2500) {
            energy = 100;
            clamp_stats();
            if (!full_chime_played[STAT_ENERGY]) {
                full_chime_played[STAT_ENERGY] = 1;
                rainbow_party_until = HAL_GetTick() + 2500;
                care_music_stop();
                play_success_chime();
            }
            bedroom_sleeping = 0;
            current_screen = PET_SCREEN_DASHBOARD;
        }
        return;
    }

    if (current_screen == PET_SCREEN_KITCHEN && !carrying_food && (direction == E || direction == W)) {
        return;
    }

    move_tool(direction);

    if (current_screen == PET_SCREEN_KITCHEN && carrying_food && close_to(tool_x, tool_y, 118, 148, 28)) {
        hunger += food_value(selected_food);
        energy += 1;
        carrying_food = 0;
        tool_x = 36 + selected_food * 56;
        tool_y = 202;
        buzzer_tone(&buzzer_cfg, 900, 25);
        HAL_Delay(35);
        buzzer_off(&buzzer_cfg);
    }
    else if (current_screen == PET_SCREEN_BATH && close_to(tool_x, tool_y, 116, 116, 56)) {
        cleanliness += 1;
    }
}

static void handle_room_button(void)
{
    if (tool_over_back_button()) {
        current_screen = PET_SCREEN_DASHBOARD;
        bedroom_sleeping = 0;
        return;
    }

    if (current_screen == PET_SCREEN_KITCHEN) {
        if (!carrying_food) {
            carrying_food = 1;
            tool_x = 36 + selected_food * 56;
            tool_y = 202;
        }
    }
    else if (current_screen == PET_SCREEN_DOCTOR && close_to(tool_x, tool_y, 116, 116, 64)) {
        health += 12;
        hunger -= 1;
        buzzer_tone(&buzzer_cfg, 700, 25);
        HAL_Delay(45);
        buzzer_off(&buzzer_cfg);
    }
    else if (current_screen == PET_SCREEN_BEDROOM && close_to(tool_x, tool_y, 203, 84, 24)) {
        bedroom_sleeping = 1;
        sleep_start = HAL_GetTick();
        buzzer_tone(&buzzer_cfg, 420, 20);
        HAL_Delay(60);
        buzzer_off(&buzzer_cfg);
    }
}

static void apply_game1_result_if_needed(void)
{
    Game1Summary summary;

    if (!SharedGameState_TakeGame1Result(&summary)) {
        return;
    }

    if (summary.result == GAME1_RESULT_WIN) {
        hunger -= 6;
        energy -= 6;
    }
    else if (summary.result == GAME1_RESULT_TIRED) {
        energy -= 18;
        hunger -= 8;
    }
    else if (summary.result == GAME1_RESULT_HURT) {
        health -= 22;
        cleanliness -= 8;
    }
    else if (summary.result == GAME1_RESULT_DIED) {
        health -= 35;
        hunger -= 18;
        energy -= 18;
        cleanliness -= 12;
    }

    clamp_stats();
}

/* -------------------------------------------------------------------------- */
/* Main care mode loop                                                         */
/* -------------------------------------------------------------------------- */

MenuState Game2_Run(void)
{
    static Direction last_direction = CENTRE;
    static uint32_t last_decay = 0;

    RGB_Off();
    current_screen = PET_SCREEN_DASHBOARD;
    bedroom_sleeping = 0;

    buzzer_tone(&buzzer_cfg, 800, 12);
    HAL_Delay(80);
    buzzer_off(&buzzer_cfg);
    show_care_cover();
    apply_game1_result_if_needed();
    care_music_reset();

    while (1)
    {
        uint32_t frame_start = HAL_GetTick();

        Input_Read();
        Joystick_Read(&joystick_cfg, &joystick_data);
        Direction current_direction = joystick_data.direction;

        if (current_screen == PET_SCREEN_DASHBOARD) {
            // The back arrow sits to the left of the stat list, so a left push
            // deliberately jumps to it instead of moving through the list.
            if (current_direction == W && last_direction != W) {
                selected_stat = STAT_BACK;
            }
            else if (current_direction == E && last_direction != E && selected_stat == STAT_BACK) {
                selected_stat = STAT_HUNGER;
            }
            else if (current_direction == S && last_direction != S) {
                if (selected_stat == STAT_BACK) {
                    selected_stat = STAT_HUNGER;
                }
                else {
                    selected_stat++;
                }
                if (selected_stat > STAT_BACK) selected_stat = STAT_HUNGER;
            }
            else if (current_direction == N && last_direction != N) {
                if (selected_stat == STAT_BACK) {
                    selected_stat = STAT_HUNGER;
                }
                else {
                    selected_stat = selected_stat == STAT_HUNGER ? STAT_ENERGY : selected_stat - 1;
                }
            }

            if (current_input.btn2_pressed) {
                if (selected_stat == STAT_BACK) {
                    care_music_stop();
                    RGB_Off();
                    return MENU_STATE_HOME;
                }
                enter_selected_room();
            }
        }
        else {
            if (current_screen == PET_SCREEN_KITCHEN && !carrying_food) {
                if (tool_over_back_button()) {
                    if ((current_direction == S || current_direction == E) && last_direction != current_direction) {
                        tool_x = 36 + selected_food * 56;
                        tool_y = 202;
                        current_direction = CENTRE;
                    }
                }
                else if (current_direction == N && last_direction != N) {
                    tool_x = 18;
                    tool_y = 18;
                    current_direction = CENTRE;
                }
                else if (current_direction == E && last_direction != E) {
                    selected_food++;
                    if (selected_food > 3) selected_food = 0;
                    tool_x = 36 + selected_food * 56;
                    tool_y = 202;
                    current_direction = CENTRE;
                }
                else if (current_direction == W && last_direction != W) {
                    selected_food = selected_food == 0 ? 3 : selected_food - 1;
                    tool_x = 36 + selected_food * 56;
                    tool_y = 202;
                    current_direction = CENTRE;
                }
            }

            update_room(current_direction);

            if (current_input.btn2_pressed) {
                handle_room_button();
            }

            if (current_input.btn3_pressed) {
                current_screen = PET_SCREEN_DASHBOARD;
                bedroom_sleeping = 0;
            }
        }

        if (current_screen == PET_SCREEN_DASHBOARD && current_input.btn3_pressed) {
            care_music_stop();
            RGB_Off();
            return MENU_STATE_HOME;
        }

        last_direction = current_direction;

        if (HAL_GetTick() - last_decay > 3000) {
            last_decay = HAL_GetTick();

            // A tiny bit of decay keeps the pet feeling alive while the player
            // is deciding what to do next.
            hunger--;
            cleanliness--;
            energy--;

            if (hunger < 20 || cleanliness < 20 || energy < 20) {
                health--;
            }
        }

        clamp_stats();
        update_full_chime_state();
        care_music_update();
        update_rgb_status();
        render_current_screen();

        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < GAME2_FRAME_TIME_MS) {
            HAL_Delay(GAME2_FRAME_TIME_MS - frame_time);
        }
    }
}
