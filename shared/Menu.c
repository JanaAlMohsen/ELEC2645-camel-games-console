#include "Menu.h"
#include "LCD.h"
#include "InputHandler.h"
#include "Joystick.h"
#include "Buzzer.h"
#include "menu_background_sprite.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>

extern ST7789V2_cfg_t cfg0;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;
extern Buzzer_cfg_t buzzer_cfg;

// The two games shown on the shared menu.
static const char* menu_options[] = {
    "Desert Run",
    "Care Mode"
};

#define NUM_MENU_OPTIONS 2
#define MENU_FRAME_TIME_MS 30
#define MENU_COL_BLACK 0
#define MENU_COL_WHITE 1
#define MENU_COL_SKY 2
#define MENU_COL_BROWN 3
#define MENU_COL_DARK_BROWN 4
#define MENU_COL_GOLD 5

static void menu_print_outlined(char const *text, uint16_t x, uint16_t y,
                                uint8_t colour, uint8_t outline_colour,
                                uint8_t font_size)
{
    LCD_printString((char*)text, x - 1, y, outline_colour, font_size);
    LCD_printString((char*)text, x + 1, y, outline_colour, font_size);
    LCD_printString((char*)text, x, y - 1, outline_colour, font_size);
    LCD_printString((char*)text, x, y + 1, outline_colour, font_size);
    LCD_printString((char*)text, x, y, colour, font_size);
}

static void menu_note(Buzzer_Note_t note, uint32_t duration_ms)
{
    buzzer_note(&buzzer_cfg, note, 9);
    HAL_Delay(duration_ms);
    buzzer_off(&buzzer_cfg);
    HAL_Delay(25);
}

static void play_menu_desert_motif(void)
{
    menu_note(NOTE_D5, 70);
    menu_note(NOTE_F5, 70);
    menu_note(NOTE_GS5, 110);
    menu_note(NOTE_G5, 70);
    menu_note(NOTE_D5, 140);
}

static void play_menu_move_tick(void)
{
    buzzer_note(&buzzer_cfg, NOTE_D5, 7);
    HAL_Delay(25);
    buzzer_off(&buzzer_cfg);
}

static void play_menu_select_tone(void)
{
    menu_note(NOTE_G5, 55);
    menu_note(NOTE_D6, 90);
}

static void menu_led_white(void)
{
    // Turn both RGB LEDs white while the player is on the menu.
    // LED1 wiring: PA9=green, PC7=blue, PC8=red.
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);

    // LED2 wiring: PC9=red, PD2=green, PA5=blue.
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
}

static void render_home_menu(MenuSystem* menu)
{
    LCD_Set_Palette(PALETTE_MENU);
    LCD_Draw_Sprite(0, 0, MENU_BACKGROUND_H, MENU_BACKGROUND_W,
                    menu_background_sprite);

    for (int i = 0; i < NUM_MENU_OPTIONS; i++) {
        uint16_t y_pos = 86 + (i * 46);
        uint8_t selected = (i == menu->selected_option);
        uint8_t fill_colour = selected ? MENU_COL_WHITE : MENU_COL_SKY;
        uint8_t text_colour = selected ? MENU_COL_DARK_BROWN : MENU_COL_WHITE;
        uint8_t outline_colour = selected ? MENU_COL_GOLD : MENU_COL_BROWN;

        LCD_Draw_Rect(28, y_pos, 184, 34, fill_colour, 1);
        LCD_Draw_Rect(28, y_pos, 184, 34, MENU_COL_BROWN, 0);

        if (selected) {
            LCD_Draw_Rect(38, y_pos + 8, 12, 18, MENU_COL_BROWN, 1);
            LCD_printString(">", 42, y_pos + 13, MENU_COL_WHITE, 1);
        }

        menu_print_outlined(menu_options[i], 58, y_pos + 9, text_colour,
                            outline_colour, 2);
    }

    menu_print_outlined("UP/DOWN MOVE", 82, 204, MENU_COL_WHITE,
                        MENU_COL_BROWN, 1);
    menu_print_outlined("BT2 SELECT", 90, 222, MENU_COL_WHITE,
                        MENU_COL_BROWN, 1);

    LCD_Refresh(&cfg0);
}

void Menu_Init(MenuSystem* menu)
{
    menu->selected_option = 0;
}

MenuState Menu_Run(MenuSystem* menu)
{
    static Direction last_direction = CENTRE;
    MenuState selected_game = MENU_STATE_HOME;

    menu_led_white();
    play_menu_desert_motif();

    while (1) {
        uint32_t frame_start = HAL_GetTick();

        Input_Read();
        Joystick_Read(&joystick_cfg, &joystick_data);

        Direction current_direction = joystick_data.direction;

        if (current_direction == S && last_direction != S) {
            menu->selected_option++;

            if (menu->selected_option >= NUM_MENU_OPTIONS) {
                menu->selected_option = 0;
            }

            play_menu_move_tick();
        }
        else if (current_direction == N && last_direction != N) {
            if (menu->selected_option == 0) {
                menu->selected_option = NUM_MENU_OPTIONS - 1;
            } else {
                menu->selected_option--;
            }

            play_menu_move_tick();
        }

        last_direction = current_direction;

        // BT2 selects the highlighted game.
        if (current_input.btn2_pressed) {
            play_menu_select_tone();

            if (menu->selected_option == 0) {
                selected_game = MENU_STATE_GAME_1;
            }
            else if (menu->selected_option == 1) {
                selected_game = MENU_STATE_GAME_2;
            }

            break;
        }

        render_home_menu(menu);
        menu_led_white();

        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < MENU_FRAME_TIME_MS) {
            HAL_Delay(MENU_FRAME_TIME_MS - frame_time);
        }
    }

    return selected_game;
}
