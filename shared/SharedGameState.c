#include "SharedGameState.h"

static Game1Summary pending_game1_summary = {GAME1_RESULT_NONE, 0, 0};
static uint8_t has_pending_game1_summary = 0;

void SharedGameState_Reset(void)
{
    pending_game1_summary.result = GAME1_RESULT_NONE;
    pending_game1_summary.score = 0;
    pending_game1_summary.distance = 0;
    has_pending_game1_summary = 0;
}

void SharedGameState_RecordGame1Result(Game1Result result, uint16_t score, uint16_t distance)
{
    pending_game1_summary.result = result;
    pending_game1_summary.score = score;
    pending_game1_summary.distance = distance;
    has_pending_game1_summary = 1;
}

uint8_t SharedGameState_TakeGame1Result(Game1Summary *summary)
{
    if (!has_pending_game1_summary) {
        return 0;
    }

    if (summary != 0) {
        *summary = pending_game1_summary;
    }

    SharedGameState_Reset();
    return 1;
}
