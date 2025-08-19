#if !defined(WIN32_MAIN_H)
#undef _HAS_EXCEPTIONS
#define _HAS_EXCEPTIONS 0

#define ASSERT assert

#define WIN32_FILE_NAME_SIZE MAX_PATH

struct Win32GameDLL
{
	bool32 isValid;

	char *DLLFullPath;
	char *DLLTempPath;
	char *DLLLockFilePath;
	
    HMODULE DLL;
    FILETIME lastWriteTime;
	
    // NOTE(james): Either of the callbacks can be 0!  You must
    // check before calling.
    GameUpdate *gameUpdate;

};

inline FILETIME
win32GetLastWriteTime(char *file)
{
    FILETIME lastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA data;
    if(GetFileAttributesEx(file, GetFileExInfoStandard, &data))
    {
        lastWriteTime = data.ftLastWriteTime;
    }

    return lastWriteTime;
}

static bool32
win32CheckForCodeChange(Win32GameDLL *gameDLL)
{
	FILETIME newDLLWriteTime = win32GetLastWriteTime(gameDLL->DLLFullPath);
	bool32 result = CompareFileTime(&newDLLWriteTime, &gameDLL->lastWriteTime);
	return result;
}

static Win32GameDLL
win32LoadGameCode(char *srcDLL, char *tmpDLL, char *lock)
{
    Win32GameDLL result = {};
	result.DLLFullPath = srcDLL;
	result.DLLTempPath = tmpDLL;
	result.DLLLockFilePath = lock;
    WIN32_FILE_ATTRIBUTE_DATA ignored;
    if(!GetFileAttributesEx(lock, GetFileExInfoStandard, &ignored))
    {
        result.lastWriteTime = win32GetLastWriteTime(srcDLL);

        CopyFile(srcDLL, tmpDLL, FALSE);

        result.DLL = LoadLibraryA(tmpDLL);
        if(result.DLL)
        {
            result.gameUpdate = (GameUpdate *) GetProcAddress(result.DLL, "gameUpdate");

            result.isValid = result.gameUpdate && 1;
        }
    }

    if(!result.isValid)
    {
        result.gameUpdate = 0;
    }

    return result;
}

static void
win32UnloadGameCode(Win32GameDLL *gameCode)
{
    if(gameCode->DLL)
    {
        FreeLibrary(gameCode->DLL);
        gameCode->DLL = 0;
    }

    gameCode->isValid = false;
    gameCode->gameUpdate = 0;
}
#define WIN32_MAIN_H
#endif
