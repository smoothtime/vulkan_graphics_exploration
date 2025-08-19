#if !defined(GAME_H)
#include "platform.h"


struct GameState
{
    uint32 entityCount;

    uint32 resWidth, resHeight;

    real64 deltaTime;
};

#define GAME_H
#endif
