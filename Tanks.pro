TEMPLATE = app
CONFIG += c++11
CONFIG -= app_bundle
CONFIG -= qt

DEFINES -= UNICODE

HEADERS += tanks.h \
    platform.h \
    editor.h \
    main_menu.h \
    mesh.h \
    demo.h \
    input.h \
    primitives.h \
    common.h \
    camera.h \
    scene_printer.h \
    mesh_loading.h \
    stb_image.h \
    stb_truetype.h

SOURCES += win32_tanks.cpp \
    tanks.cpp \
    main_menu.cpp \
    editor.cpp \
    demo.cpp \
    mesh_loading.cpp \
    scene_printer.cpp \
    stb_image.cpp \
    stb_truetype.cpp

INCLUDEPATH += D:/projects/middleware/assimp4/include
#INCLUDEPATH += D:/projects/middleware/SDL2-2.0.4/include
INCLUDEPATH += D:/projects/middleware/gl3w/include
INCLUDEPATH += D:/projects/middleware/glm
INCLUDEPATH += D:/projects/middleware/

LIBS += -LD:/projects/middleware/assimp4/lib -lassimp
#LIBS += -LD:/projects/middleware/SDL2-2.0.4/x86_64-w64-mingw32/lib -lSDL2
LIBS += -LD:\projects\middleware\gl3w\lib64 -lgl3w
LIBS += -lopengl32 -lgdi32 -luser32
