#ifndef MENU_H
#define MENU_H

#include <stdint.h>

// Shared menu states.

typedef enum {
    MENU_STATE_HOME = 0,        // Main game selection screen.
    MENU_STATE_GAME_1,          // Desert Run.
    MENU_STATE_GAME_2,          // Care Mode.
    MENU_STATE_GAME_3,          // Kept as a spare slot from the template.
} MenuState;

// Current menu selection.
typedef struct {
    uint8_t selected_option;    // Highlighted option on the menu.
} MenuSystem;

/**
 * @brief Start the menu on the first option.
 */
void Menu_Init(MenuSystem* menu);

/**
 * @brief Show the menu and wait until a game is selected.
 * 
 * The menu owns the screen while it is running, then returns the selected game
 * state to main().
 * 
 * @return MenuState The selected game state.
 */
MenuState Menu_Run(MenuSystem* menu);

#endif // MENU_H
