#ifndef GAME_1_H
#define GAME_1_H

#include "Menu.h"

/**
 * @brief Game 1 - Desert Run
 * 
 * Runs Hamoodi's desert runner game. Game 1 handles its own sprites, score,
 * obstacles, sound, and LED feedback, but still uses the shared menu and input
 * code.
 * 
 * The menu calls this when Game 1 is selected. The function keeps control until
 * the player exits back to the menu.
 * 
 * @return MenuState Where the program should go next.
 */

MenuState Game1_Run(void);

#endif // GAME_1_H
