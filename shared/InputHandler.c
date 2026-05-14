#include "InputHandler.h"
#include "main.h"

// Button state used by the menu and games.
InputState current_input = {0};

// Raw button flags are set inside the interrupt, then consumed once per frame.
static volatile uint8_t btn2_raw_press = 0;
static volatile uint8_t btn3_raw_press = 0;

void Input_Init(void) {
    // GPIO/EXTI setup happens in main.c; this just clears the button state.
    current_input.btn2_pressed = 0;
    current_input.btn3_pressed = 0;
    btn2_raw_press = 0;
    btn3_raw_press = 0;
}

void Input_Read(void) {
    // Copy interrupt flags into the frame state.
    current_input.btn2_pressed = btn2_raw_press;
    current_input.btn3_pressed = btn3_raw_press;
    
    // Clear the raw flags so each press is only handled once.
    btn2_raw_press = 0;
    btn3_raw_press = 0;
}

// Hardware callback for the button interrupt lines.
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    static uint32_t last_btn2_interrupt = 0;
    static uint32_t last_btn3_interrupt = 0;
    uint32_t current_time = HAL_GetTick();
    
    // BT2: start/select/restart depending on the current screen.
    if (GPIO_Pin == BTN2_Pin) {
        // Simple debounce so one press does not become several presses.
        if ((current_time - last_btn2_interrupt) > 200) {
            last_btn2_interrupt = current_time;
            
            btn2_raw_press = 1;
        }
    }
    
    // BT3: mostly used to return to the menu.
    if (GPIO_Pin == BTN3_Pin) {
        // Simple debounce so one press does not become several presses.
        if ((current_time - last_btn3_interrupt) > 200) {
            last_btn3_interrupt = current_time;
            
            btn3_raw_press = 1;
        }
    }
}
