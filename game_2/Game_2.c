#include "Game_2.h"
#include "InputHandler.h"
#include "Menu.h"
#include "LCD.h"
#include "Buzzer.h"
#include "Joystick.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include "camel_sprite.h"

extern ST7789V2_cfg_t cfg0;
extern Buzzer_cfg_t buzzer_cfg;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;

// ===== CARE MODE SETTINGS =====
#define GAME2_FRAME_TIME_MS 80

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

// ===== RGB LED PINS =====
// Common cathode RGB LEDs: HIGH = ON

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

// Pet stats
static int hunger = 70;
static int energy = 70;
static int clean = 70;

// 0 = Feed, 1 = Sleep, 2 = Clean, 3 = Back
static int selected_action = 0;

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
    HAL_GPIO_WritePin(LED1_R_PORT, LED1_R_PIN, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED1_G_PORT, LED1_G_PIN, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED1_B_PORT, LED1_B_PIN, b ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LED2_R_PORT, LED2_R_PIN, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_G_PORT, LED2_G_PIN, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_B_PORT, LED2_B_PIN, b ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void update_rgb_status(void)
{
    // Red = bad stats
    if (hunger < 30 || energy < 30 || clean < 30) {
        RGB_Set(1, 0, 0);
    }
    // Yellow = medium stats
    else if (hunger < 60 || energy < 60 || clean < 60) {
        RGB_Set(1, 1, 0);
    }
    // Green = good stats
    else {
        RGB_Set(0, 1, 0);
    }
}

static void clamp_stats(void)
{
    if (hunger > 100) hunger = 100;
    if (energy > 100) energy = 100;
    if (clean > 100) clean = 100;

    if (hunger < 0) hunger = 0;
    if (energy < 0) energy = 0;
    if (clean < 0) clean = 0;
}

static void draw_bar(int x, int y, int value, uint8_t colour)
{
    LCD_Draw_Rect(x, y, 70, 8, COL_BLACK, 0);
    LCD_Draw_Rect(x + 1, y + 1, (value * 68) / 100, 6, colour, 1);
}

static void draw_pet(void)
{
    // 32x32 sprite scaled by 3 = 96x96
    LCD_Draw_Sprite_Scaled(70, 50, 32, 32, camel_sprite, 3);
}

static void render_care_mode(void)
{
    LCD_Fill_Buffer(COL_BG);

    LCD_printString("CARE MODE", 45, 8, COL_BROWN, 2);

    draw_pet();

    LCD_printString("Hunger", 5, 155, COL_BLACK, 1);
    draw_bar(65, 155, hunger, COL_ORANGE);

    LCD_printString("Energy", 5, 173, COL_BLACK, 1);
    draw_bar(65, 173, energy, COL_BLUE);

    LCD_printString("Clean", 5, 191, COL_BLACK, 1);
    draw_bar(65, 191, clean, COL_SAGE);

    LCD_printString("Feed", 5, 220, selected_action == 0 ? COL_BROWN : COL_BLACK, 1);
    LCD_printString("Sleep", 55, 220, selected_action == 1 ? COL_BROWN : COL_BLACK, 1);
    LCD_printString("Clean", 115, 220, selected_action == 2 ? COL_BROWN : COL_BLACK, 1);
    LCD_printString("Back", 180, 220, selected_action == 3 ? COL_BROWN : COL_BLACK, 1);

    if (selected_action == 0) {
        LCD_printString("^", 18, 232, COL_BROWN, 1);
    } 
    else if (selected_action == 1) {
        LCD_printString("^", 75, 232, COL_BROWN, 1);
    } 
    else if (selected_action == 2) {
        LCD_printString("^", 135, 232, COL_BROWN, 1);
    } 
    else {
        LCD_printString("^", 195, 232, COL_BROWN, 1);
    }

    LCD_Refresh(&cfg0);
}

static void action_feedback(const char* message, uint8_t bg_colour, uint32_t freq)
{
    LCD_Fill_Buffer(bg_colour);
    LCD_printString(message, 45, 90, COL_BLACK, 2);
    LCD_Refresh(&cfg0);

    buzzer_tone(&buzzer_cfg, freq, 30);
    HAL_Delay(250);
    buzzer_off(&buzzer_cfg);
}

MenuState Game2_Run(void)
{
    static Direction last_direction = CENTRE;
    static uint32_t last_decay = 0;

    RGB_Off();

    buzzer_tone(&buzzer_cfg, 800, 30);
    HAL_Delay(80);
    buzzer_off(&buzzer_cfg);

    while (1)
    {
        uint32_t frame_start = HAL_GetTick();

        Input_Read();
        Joystick_Read(&joystick_cfg, &joystick_data);

        Direction current_direction = joystick_data.direction;

        // Joystick left/right moves selector
        if (current_direction == E && last_direction != E) {
            selected_action++;

            if (selected_action > 3) {
                selected_action = 0;
            }
        }
        else if (current_direction == W && last_direction != W) {
            if (selected_action == 0) {
                selected_action = 3;
            } else {
                selected_action--;
            }
        }

        last_direction = current_direction;

        // PC2 / BTN2 selects action
        if (current_input.btn2_pressed) {
            if (selected_action == 0) {
                hunger += 15;
                action_feedback("Eating!", COL_CREAM, 900);
            }
            else if (selected_action == 1) {
                energy += 15;
                action_feedback("Sleeping", COL_LAVENDER, 500);
            }
            else if (selected_action == 2) {
                clean += 15;
                action_feedback("Bath time", COL_SAGE, 1200);
            }
            else if (selected_action == 3) {
                RGB_Off();
                return MENU_STATE_HOME;
            }
        }

        // Stats slowly decrease
        if (HAL_GetTick() - last_decay > 3000) {
            last_decay = HAL_GetTick();
            hunger--;
            energy--;
            clean--;
        }

        clamp_stats();
        update_rgb_status();
        render_care_mode();

        uint32_t frame_time = HAL_GetTick() - frame_start;

        if (frame_time < GAME2_FRAME_TIME_MS) {
            HAL_Delay(GAME2_FRAME_TIME_MS - frame_time);
        }
    }
}