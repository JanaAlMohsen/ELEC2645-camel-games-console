#ifndef SHARED_GAME_STATE_H
#define SHARED_GAME_STATE_H

#include <stdint.h>

typedef enum {
    GAME1_RESULT_NONE = 0,
    GAME1_RESULT_WIN,
    GAME1_RESULT_TIRED,
    GAME1_RESULT_HURT,
    GAME1_RESULT_DIED
} Game1Result;

typedef struct {
    Game1Result result;
    uint16_t score;
    uint16_t distance;
} Game1Summary;

void SharedGameState_Reset(void);
void SharedGameState_RecordGame1Result(Game1Result result, uint16_t score, uint16_t distance);
uint8_t SharedGameState_TakeGame1Result(Game1Summary *summary);

#endif
