@echo on

IF NOT EXIST ..\shaders mkdir ..\shaders
pushd ..\shaders
cd

REM invoke GLSL validator on all shaders
set extensions=comp frag vert
for %%e in (%extensions%) do (
	for %%s in (*.%%e) do (
		%VULKAN_SDK%\Bin\glslangValidator.exe -V %%s -o %%s.spv
	)
)

popd
