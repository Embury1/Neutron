@echo off

cls

if not exist .\build mkdir .\build
pushd .\build

cl /MD /Zi /EHsc /nologo ^
/ID:/libs/stb /ID:/libs/glad/include /ID:/repo/glm /ID:/repo/glfw/include /ID:/libs/lua-5.3.5_Win64_vc15_lib/include ^
/DNN_INTERNAL ../neutron.cpp user32.lib gdi32.lib shell32.lib glfw3.lib lua53.lib ^
/link /libpath:D:/repo/glfw/build/src/Release /libpath:D:/libs/lua-5.3.5_Win64_vc15_lib

popd
