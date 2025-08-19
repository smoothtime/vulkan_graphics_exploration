#if !defined(PLATFORM_H)
    
#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif
    
#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#endif
#if defined(__llvm__)
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#include <assert.h>
#include <math.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdint.h>
#include <mutex>
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;
typedef uintptr_t uptr;
#define PI32 3.14159265

#define internalfun static

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))


//Begin abstracted platform apis

#define PLATFORM_LOG(functionName) void functionName(const char *msg)
typedef PLATFORM_LOG(platformLog);

// global logger to be set by platform;
platformLog *gLog;

//End abstracted platform apis


typedef struct GameMemory
{
    bool32 isInitialized;
    uint64 permanentStorageSize;
    uint64 transientStorageSize;
    void * permStorage;
    void * transStorage;

    platformLog *log;
} GameMemory;

typedef struct GameInput
{
    bool32 wantsToTerminate;
    bool32 w, a, s, d, space, leftClick, newLeftClick, rightClick, newRightClick;
    real32 mouseX, mouseY;
    real32 mouseDeltaX, mouseDeltaY;
    uint32 resX, resY;
} GameInput;

#define GAME_UPDATE(functionName) void functionName(GameMemory *memory, GameInput *input, real64 deltaTime)
typedef GAME_UPDATE(GameUpdate);

#define PLATFORM_H
#endif