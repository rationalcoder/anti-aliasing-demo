TEMPLATE = app
CONFIG += c++11
CONFIG -= app_bundle
CONFIG -= qt

DEFINES -= UNICODE

HEADERS += tanks.h \
    platform.h \
    memory.h \
    mesh.h \
    input.h \
    primitives.h \
    common.h \
    camera.h \
    stb_image.h \
    stb_truetype.h \
    tanks.cpp \
    stb.cpp \
    win32_tanks.h \
    globals.h \
    platform.cpp \
    renderer.h \
    opengl_renderer.h \
    opengl_renderer.cpp \
    game_rendering.h \
    buffer.h \
    obj_file.h \
    obj_file.cpp

SOURCES += win32_tanks.cpp \

INCLUDEPATH += D:/projects/middleware/assimp4/include
#INCLUDEPATH += D:/projects/middleware/SDL2-2.0.4/include
INCLUDEPATH += D:/projects/middleware/gl3w/include
INCLUDEPATH += D:/projects/middleware/glm
INCLUDEPATH += D:/projects/middleware/

LIBS += -LD:/projects/middleware/assimp4/lib -lassimp
#LIBS += -LD:/projects/middleware/SDL2-2.0.4/x86_64-w64-mingw32/lib -lSDL2
LIBS += -LD:\projects\middleware\gl3w\lib64 -lgl3w
LIBS += -lopengl32 -lgdi32 -luser32
