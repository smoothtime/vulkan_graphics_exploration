#include "game.h"

extern "C"
GAME_UPDATE(gameUpdate)
{
    GameState *gameState = (GameState *) memory->permStorage;
    if(!memory->isInitialized)
    {
        memory->isInitialized = true;        
        gLog = memory->log;
		gameState->entityCount = 0;

    }
    else
    {
        gameState->deltaTime = deltaTime;

        if(input->space)
        {
            char log[256];
			sprintf_s(log, "\nentity count: %d\n", gameState->entityCount);
			gLog(log);
        }
	}
}