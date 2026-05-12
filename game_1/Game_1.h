#ifndef GAME_1_H
#define GAME_1_H

#include "Menu.h"

/**
 * @brief Game 1 - Desert Run
 * 
 * Runs Hamoodi's desert runner game. Game 1 has its own loop, sprites,
 * scoring, obstacle handling, sound, and LED feedback while still using the
 * shared menu/input utilities.
 * 
 * The menu system calls this function when Game 1 is selected.
 * The function runs its own loop and returns when the game exits.
 * 
 * @return MenuState - Where to go next (typically MENU_STATE_HOME for menu)
 */

MenuState Game1_Run(void);

#endif // GAME_1_H
