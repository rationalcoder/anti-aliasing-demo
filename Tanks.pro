TEMPLATE = app
CONFIG += c++11
CONFIG -= app_bundle
CONFIG -= qt

DEFINES -= UNICODE
DEFINES += GLM_EXPERIMENTAL_ENABLED

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
    win32_tanks.h \
    globals.h \
    tanks.cpp \
    platform.cpp \
    renderer.h \
    opengl_renderer.h \
    opengl_renderer.cpp \
    game_rendering.h \
    buffer.h \
    obj_file.h \
    obj_file.cpp \
    containers.h \
    stb.h \
    imconfig.h \
    imgui_internal.h \
    stb_rectpack.h \
    stb_sprintf.h \
    stb_textedit.h \
    imgui.h \
    imgui_demo.cpp \
    imgui_draw.cpp \
    imgui_widgets.cpp \
    imgui.cpp \
    imgui_impl_win32.h \
    imgui_impl_win32.cpp \
    developer_ui.cpp \
    spk_file.h

SOURCES += win32_tanks.cpp \

#INCLUDEPATH += D:/projects/middleware/assimp4/include
#INCLUDEPATH += D:/projects/middleware/SDL2-2.0.4/include
INCLUDEPATH += $$PWD/middleware/gl3w/include
INCLUDEPATH += $$PWD/middleware/glm
INCLUDEPATH += $$PWD/middleware/

#LIBS += -LD:/projects/middleware/assimp4/lib -lassimp
#LIBS += -LD:/projects/middleware/SDL2-2.0.4/x86_64-w64-mingw32/lib -lSDL2
LIBS +=  -L$$PWD/middleware/gl3w/lib64 -lgl3w
LIBS += -lopengl32 -lgdi32 -luser32
