#include "Menu.h"
#include "LCD.h"
#include "InputHandler.h"
#include "Joystick.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>

extern ST7789V2_cfg_t cfg0;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;

// Menu options
static const char* menu_options[] = {
    "Desert Run",
    "Care Mode"
};

#define NUM_MENU_OPTIONS 2
#define MENU_FRAME_TIME_MS 30

static void render_home_menu(MenuSystem* menu)
{
    LCD_Fill_Buffer(0);

    LCD_printString("MAIN MENU", 50, 10, 1, 3);

    for (int i = 0; i < NUM_MENU_OPTIONS; i++) {
        uint16_t y_pos = 70 + (i * 40);
        uint8_t text_size = 2;

        if (i == menu->selected_option) {
            LCD_printString(">", 40, y_pos, 1, text_size);
        }

        LCD_printString((char*)menu_options[i], 70, y_pos, 1, text_size);
    }

    LCD_printString("Press BTN", 50, 220, 1, 1);
    LCD_printString("to select", 50, 235, 1, 1);

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
        }
        else if (current_direction == N && last_direction != N) {
            if (menu->selected_option == 0) {
                menu->selected_option = NUM_MENU_OPTIONS - 1;
            } else {
                menu->selected_option--;
            }
        }

        last_direction = current_direction;

        // PC2 button = BTN2 = selector
        if (current_input.btn2_pressed) {
            if (menu->selected_option == 0) {
                selected_game = MENU_STATE_GAME_1;
            }
            else if (menu->selected_option == 1) {
                selected_game = MENU_STATE_GAME_2;
            }

            break;
        }

        render_home_menu(menu);

        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < MENU_FRAME_TIME_MS) {
            HAL_Delay(MENU_FRAME_TIME_MS - frame_time);
        }
    }

    return selected_game;
}