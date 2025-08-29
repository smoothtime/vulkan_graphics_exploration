#include <windows.h>
#include <stdio.h>
#include <cstdlib>
#include <cassert>
#include "platform.h"
#include "win32_main.h"

#include "math_types.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#include "ui.h"
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "imgui_impl_vulkan.h"
#define RENDER_WINDOW_CALLBACK(functionName) VkResult functionName(VkInstance *pInstance, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
typedef RENDER_WINDOW_CALLBACK(RenderWindowCallback);
#include "vk.h"

static bool32 GLOBAL_RUNNING = true;
static int64 GLOBAL_PERF_COUNT_FREQ = 0;
static WINDOWPLACEMENT GLOBAL_WINDOW_POS = { sizeof(GLOBAL_WINDOW_POS)};
static HWND GLOBAL_WINDOW_HANDLE = 0;
static HINSTANCE GLOBAL_INSTANCE_HANDLE = 0;
GameInput inputLastFrame;
GameInput inputForFrame;

PLATFORM_LOG(win32Log)
{
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}

RENDER_WINDOW_CALLBACK(createVulkanSurface)
{
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.flags = 0;
	surfaceCreateInfo.hinstance = GLOBAL_INSTANCE_HANDLE;
	surfaceCreateInfo.hwnd = GLOBAL_WINDOW_HANDLE;
	
	VkResult createSurfaceResult = vkCreateWin32SurfaceKHR(*pInstance, &surfaceCreateInfo, pAllocator, pSurface);
	
	return createSurfaceResult;
}


static LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
						UINT Message,
						WPARAM WParam,
						LPARAM LParam)
{
	LRESULT result = 0;
	if (ImGui_ImplWin32_WndProcHandler(Window, Message, WParam, LParam))
	{
		return true;
	}
	switch(Message)
	{
		case WM_CLOSE:
		{
			GLOBAL_RUNNING = false;
		} break;
		
		case WM_DESTROY:
		{
			GLOBAL_RUNNING = false;
		} break;
		
		default:
		{
			result = DefWindowProcA(Window, Message, WParam, LParam);
		} break;
	}
	
	return result;
}

inline LARGE_INTEGER
Win32GetWallClock(void)
{
	LARGE_INTEGER result;
	QueryPerformanceCounter(&result);
	return result;
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
	real32 result = ((real32)(end.QuadPart - start.QuadPart) / (real32)GLOBAL_PERF_COUNT_FREQ);
	return result;
}

static void
Win32ProcessPendingMessages()
{
	MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch(Message.message)
        {
            case WM_QUIT:
            {
                GLOBAL_RUNNING = false;
            } break;
            
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
				
				ImGuiIO& io = ImGui::GetIO();

				if (io.WantCaptureKeyboard)
				{
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
					continue;
				}
                uint32 VKCode = (uint32)Message.wParam;

                bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);
                if(WasDown != IsDown)
                {
                    if(VKCode == 'W')
                    {
                        
                    }
                    else if(VKCode == 'A')
                    {
                        
                    }
                    else if(VKCode == 'S')
                    {
                        
                    }
                    else if(VKCode == 'D')
                    {
                        
                    }
                }

                bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                if((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GLOBAL_RUNNING = false;
                }
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }
    }
}

int CALLBACK
WinMain(HINSTANCE instance,
        HINSTANCE prevInstance,
        LPSTR commandLine,
        int showCode)
{
	LARGE_INTEGER perfCountFrequencyResult;
    QueryPerformanceFrequency(&perfCountFrequencyResult);
	GLOBAL_PERF_COUNT_FREQ = perfCountFrequencyResult.QuadPart;
	
	// Set up Window
	HINSTANCE Instance = GetModuleHandle(0);
	WNDCLASSA WindowClass {};
	WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
	WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	WindowClass.lpszClassName = "JunkWindowClass";
	
	if(!RegisterClassA(&WindowClass)) return -1;
	HWND Window = CreateWindowExA(0,
									WindowClass.lpszClassName,
									"Jimbo's Junkyard",
									WS_OVERLAPPEDWINDOW,
									CW_USEDEFAULT,
									CW_USEDEFAULT,
									CW_USEDEFAULT,
									CW_USEDEFAULT,
									0,
									0,
									Instance,
									0);
	if (!Window) return -1;
	GLOBAL_WINDOW_HANDLE = Window;
	ShowWindow(Window, SW_SHOW);
	UpdateWindow(Window);
	
	DWORD Style = GetWindowLong(Window, GWL_STYLE);
    if(Style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO MonitorInfo = {sizeof(MonitorInfo)};
        if(GetWindowPlacement(Window, &GLOBAL_WINDOW_POS) &&
           GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
        {
            SetWindowPos(Window, HWND_TOP,
                         MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowPlacement(Window, &GLOBAL_WINDOW_POS);
        SetWindowPos(Window, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

	//get path for hot reload dll based on exe name
    char platformExecutableName[WIN32_FILE_NAME_SIZE];
    DWORD platExeNameSize = GetModuleFileName(0, platformExecutableName, sizeof(platformExecutableName));
    uint32 onePastLastSlash = 0;
    for(uint32 i = platExeNameSize - 1; i > 0; --i)
    {
        if(platformExecutableName[i] == '\\')
        {
            onePastLastSlash = i+1;
            break;
        }
    }
    char gameDLLPath[WIN32_FILE_NAME_SIZE];
    strncpy_s(gameDLLPath, platformExecutableName, onePastLastSlash);
    strcat_s(gameDLLPath, "game.dll");

    char tempDLLPath[WIN32_FILE_NAME_SIZE];
    strncpy_s(tempDLLPath, platformExecutableName, onePastLastSlash);
    strcat_s(tempDLLPath, "game_temp.dll");

    char dllLockFilePath[WIN32_FILE_NAME_SIZE];
    strncpy_s(dllLockFilePath, platformExecutableName, onePastLastSlash);
    strcat_s(dllLockFilePath, "game.lock");

    Win32GameDLL gameDLL = win32LoadGameCode(gameDLLPath, tempDLLPath, dllLockFilePath);    

    uint32 HARDCODED_RES_X = 800;
    UINT32 HARDCODED_RES_Y = 600;

	// Prepare the memory before the game loop
    GameMemory memory = {};
    memory.isInitialized = false;
    memory.log = win32Log;
    memory.permanentStorageSize = Megabytes(512);
    memory.transientStorageSize = Gigabytes(1);
    uint64 totalSize = memory.permanentStorageSize + memory.transientStorageSize;

    void *allTheDamnedMemory = VirtualAlloc(0, (size_t) totalSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    memory.permStorage = allTheDamnedMemory;
    memory.transStorage = ((uint8 *)allTheDamnedMemory + memory.permanentStorageSize);

    real32 TargetSecondsPerFrame = 1.0f / (real32)30.0f;
	
	// initialize render backend
	const char* instanceExtensions[2] = { "VK_KHR_surface", "VK_KHR_win32_surface"};
	const char* enabledLayers[1] = { "VK_LAYER_KHRONOS_validation" };
	const char* deviceExtensions[1] = { "VK_KHR_swapchain" };
	VulkanBackend backend = initializeVulkan(ArrayCount(instanceExtensions), instanceExtensions, ArrayCount(enabledLayers), enabledLayers, ArrayCount(deviceExtensions), deviceExtensions, createVulkanSurface, nullptr);
	EffectUI effectUI = {};
	// this initializes the core structures of imgui
	ImGui::CreateContext();
	// this initializes imgui for SDL
	ImGui_ImplWin32_Init(Window);
	initializeImgui(&backend);
	
	OutputDebugStringA("Renderer backend initialized");

	setvbuf(stdout, NULL, _IONBF, 0);
    while(GLOBAL_RUNNING)
    {
		// reload dll if needed
		bool32 isDLLReloadRequired = win32CheckForCodeChange(&gameDLL);
		if (isDLLReloadRequired)
		{
			win32UnloadGameCode(&gameDLL);
			gameDLL = win32LoadGameCode(gameDLLPath, tempDLLPath, dllLockFilePath);
		}
		
		// process windows messages
		Win32ProcessPendingMessages();
		
        inputForFrame.resX = HARDCODED_RES_X;
        inputForFrame.resY = HARDCODED_RES_Y;
        
		LARGE_INTEGER LastCounter = Win32GetWallClock();
        if(gameDLL.gameUpdate)
        {
            gameDLL.gameUpdate(&memory, &inputForFrame, TargetSecondsPerFrame);
        }
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		vulkanPopulateUIData(&backend, &effectUI);
		buildUI(effectUI);
		ImGui::Render();
		
		vulkanDraw(&backend);
		
		
        inputLastFrame = inputForFrame;
        inputForFrame.mouseDeltaX = 0;
        inputForFrame.mouseDeltaY = 0;
        inputForFrame.newLeftClick = false;
        inputForFrame.newRightClick = false;

		// framerate wait
		LARGE_INTEGER WorkCounter = Win32GetWallClock();
		real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
		
		if (WorkSecondsElapsed < TargetSecondsPerFrame)
		{
			real32 SecondsElapsedForFrame = WorkSecondsElapsed;
			DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
			if (SleepMS > 0)
			{
				//Sleep(SleepMS);
			}
			
			while(SecondsElapsedForFrame < TargetSecondsPerFrame)
			{
				SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
			}
		}
    }
	cleanupVulkan(&backend, nullptr);
	return 0;
}
