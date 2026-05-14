#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <stdint.h>

// Shared button input state.

/**
 * @brief Button presses captured for one frame.
 * 
 * The interrupt code sets the raw button flags, then Input_Read copies them
 * here so the menu and games can react once per press.
 */
typedef struct {
    uint8_t btn2_pressed;  // BT2 was pressed this frame.
    uint8_t btn3_pressed;  // BT3 was pressed this frame.
} InputState;

// Global button state read by the menu and games.
extern InputState current_input;

/**
 * @brief Reset the button input state after GPIO setup.
 */
void Input_Init(void);

/**
 * @brief Copy any new button presses into current_input.
 */
void Input_Read(void);

#endif // INPUT_HANDLER_H
