@echo off

mkdir build
pushd build
cl -wd4577 -wd4530 -Zi -ID:\projects\middleware\assimp4\include -ID:\projects\middleware -ID:\projects\middleware\gl3w\include -ID:\projects\middleware\glm ..\*.cpp /link /subsystem:windows /LIBPATH:D:\projects\middleware\gl3w\lib64 /LIBPATH:D:\projects\middleware\assimp4\lib gl3w.lib opengl32.lib user32.lib gdi32.lib assimp.lib
popd
