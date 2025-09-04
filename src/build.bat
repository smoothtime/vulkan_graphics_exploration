@echo off
set CommonCompilerFlags=/std:c++20 -MT -nologo /I ..\src\app /I ..\src\platform /I %VULKAN_SDK%\Include /I %VMA_PATH%\include /I ..\thirdparty\imgui /I ..\thirdparty\imgui\backends /I ..\thirdparty\glm /I ..\thirdparty\cgltf -fp:fast -DGLM_FORCE_DEPTH_ZERO_TO_ONE=1 -Gm- -Od -Oi -WX -W4 -wd4100 -wd4189 -wd4201 -wd4244 -wd4577 -wd4505 -wd4127 -wd4530 -wd4324 -wd4996 -FC -Z7 
set CommonLinkerFlags= -incremental:no -opt:ref /LIBPATH:%VULKAN_SDK%\Lib user32.lib gdi32.lib winmm.lib vulkan-1.lib 
set Sources=..\src\platform\win32_main.cpp ..\thirdparty\imgui\backends\imgui_impl_win32.cpp ..\thirdparty\imgui\backends\imgui_impl_vulkan.cpp ..\thirdparty\imgui\imgui*.cpp

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM 64-bit build
del *.pdb > NUL 2> NUL
REM Optimization switches /O2
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% ..\src\app\game.cpp -Fmgame.map -LD /link -incremental:no -opt:ref  -PDB:game_%random%.pdb -EXPORT:gameUpdate
del lock.tmp
cl %CommonCompilerFlags% %SOURCES% -Fmwin32_main.map /link %CommonLinkerFlags%
popd
