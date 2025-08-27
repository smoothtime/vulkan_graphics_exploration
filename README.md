A project for me learning the Vulkan api and graphics programming in general.

I've been going through Johannes Unterguggenberger's TU Wien lectures and they are great! Trying to render a triangle eventually and learn some shaders.

Plans are to get this to the state where I can treat it as a GLSL playground and load some skeletally animated models.

To Build:
1) Get your terminal set up with the MS compiler via
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
or whatever your toolchain is.

2) Make sure the Vulkan SDK is installed and there's a VULKAN_SDK environment variable set.

3) Install VulkanMemoryAllocator somewhere and set a VMA_PATH environment variable.

4) Invoke buildshaders.bat

5) Invoke build.bat

6) output file is ..\build\win32_main.exe

Todo:
- compile a shader
- render a triangle
- render a mesh
- create a concept of render items abstracted from vulkan and translate those draw calls
- sort render items
- moveable camera
- skin mesh
- animate mesh
- look into render graphs