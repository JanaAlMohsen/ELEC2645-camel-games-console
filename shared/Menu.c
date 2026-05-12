#include "Menu.h"
#include "LCD.h"
#include "InputHandler.h"
#include "Joystick.h"
#include "Buzzer.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>

extern ST7789V2_cfg_t cfg0;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;
extern Buzzer_cfg_t buzzer_cfg;

// Menu options
static const char* menu_options[] = {
    "Desert Run",
    "Care Mode"
};

#define NUM_MENU_OPTIONS 2
#define MENU_FRAME_TIME_MS 30

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
    // LED1 is wired as: PA9=green, PC7=blue, PC8=red.
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);

    // LED2 mirrors LED1 using the planned wiring: PC9=red, PD2=green, PA5=blue.
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
}

static void render_home_menu(MenuSystem* menu)
{
    LCD_Fill_Buffer(2);

    LCD_Draw_Rect(0, 148, 240, 92, 3, 1);
    for (uint8_t i = 0; i < 7; i++) {
        LCD_Draw_Line(40 + i * 24, 178, 28 + i * 24, 188, 7);
    }
    LCD_Draw_Circle(120, 160, 48, 7, 1);
    LCD_Draw_Rect(70, 160, 100, 26, 7, 1);

    LCD_printString("Tame", 72, 18, 3, 4);
    LCD_printString("game console", 58, 52, 0, 1);

    for (int i = 0; i < NUM_MENU_OPTIONS; i++) {
        uint16_t y_pos = 82 + (i * 48);
        uint8_t fill_colour = (i == menu->selected_option) ? 6 : 7;
        uint8_t text_colour = (i == menu->selected_option) ? 3 : 0;

        LCD_Draw_Rect(30, y_pos, 180, 34, fill_colour, 1);
        LCD_Draw_Rect(30, y_pos, 180, 34, 3, 0);

        if (i == menu->selected_option) {
            LCD_Draw_Rect(36, y_pos + 8, 12, 18, 3, 1);
            LCD_printString(">", 40, y_pos + 13, 1, 1);
        }

        LCD_printString((char*)menu_options[i], 58, y_pos + 10, text_colour, 2);
    }

    LCD_printString("up/down", 76, 206, 1, 1);
    LCD_printString("BTN2 select", 66, 222, 1, 1);

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

        // PC2 button = BTN2 = selector
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
