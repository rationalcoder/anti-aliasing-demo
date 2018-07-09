#include "tanks.h"
#include "tanks.cpp"

#include <GL/gl3w.h>
#include <GL/gl.h>
#include <GL/wglext.h>

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>

#include <cassert>
#include <cstdio>

constexpr u32 frames_to_average() { return 100; }

struct Win32_Mouse_State
{
    u32 x = 0;
    u32 y = 0;
    s32 xDragAccum = 0;
    s32 yDragAccum = 0;
    u32 dragStartX = 0;
    u32 dragStartY = 0;
    b32 dragging = false;
    b32 ignoreNextMouseMove = false;
};

struct Win32_State
{
    void* contiguousRegion = NULL;
    u32 pageSize  = 4096;
    u32 coreCount = 2;

    Game* game = NULL;
    HWND hwnd  = NULL;
    HDC dc     = NULL;
    HGLRC glrc = NULL;

    HANDLE console = INVALID_HANDLE_VALUE;

    Win32_Mouse_State mouse;

    b32 shouldQuit = false;
    Game_Resolution clientRes;
    u32 xCenter = 0;
    u32 yCenter = 0;
};

struct Win32_Window_Position
{
    int x = 0;
    int y = 0;
};

// global so that the WinProc can access it.
static Win32_State gWin32State;

static inline void
win32_swap_buffers(Win32_State* state)
{
    SwapBuffers(state->dc);
}

static inline void
win32_poll_xinput(Win32_State* state)
{
    UNREFERENCED_PARAMETER(state);
}

static inline void
win32_game_key_down(Game_Key key, bit32* keys, flag8* modStates)
{
    keys->set(key);

    if (GetKeyState(VK_RCONTROL) & 0x8000) modStates[key] |= GKM_RCNTL;
    if (GetKeyState(VK_LCONTROL) & 0x8000) modStates[key] |= GKM_LCNTL;
    if (GetKeyState(VK_RMENU)    & 0x8000) modStates[key] |= GKM_RALT;
    if (GetKeyState(VK_LMENU)    & 0x8000) modStates[key] |= GKM_LALT;
    if (GetKeyState(VK_RSHIFT)   & 0x8000) modStates[key] |= GMK_RSHIFT;
    if (GetKeyState(VK_LSHIFT)   & 0x8000) modStates[key] |= GKM_LSHIFT;
}

static inline void
win32_game_key_up(Game_Key key, bit32* keys, flag8* modStates)
{
    keys->unset(key);
    modStates[key] = 0;
}

static inline POINT
win32_center_cursor()
{
    POINT point = { down_cast<LONG>(gWin32State.xCenter), down_cast<LONG>(gWin32State.yCenter) };
    ClientToScreen(gWin32State.hwnd, &point);
    SetCursorPos(point.x, point.y);

    return point;
}

static LRESULT CALLBACK
win32_event_callback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Game_Input* input = &gWin32State.game->input;
    bit32* keys = &input->keyboard.cur.keys;
    flag8* mod  = input->keyboard.cur.mod;

    Win32_Mouse_State& mouse = gWin32State.mouse;

    // NOTE: these are useful if we actually want modifier keys on their own.
    // u32 scancode = (lParam & 0x00ff0000) >> 16;
    // b32 ext = (lParam & 0x01000000);

    switch (message) {
    case WM_CLOSE:
        gWin32State.shouldQuit = true;
        return 0;
    case WM_DESTROY:
        gWin32State.shouldQuit = true;
        PostQuitMessage(0);
        return 0;
    case WM_SIZE: {
        // NOTE: only resize the game if the resolution has actually changed.
        // Otherwise, we will end up creating all FBO's twice on init.
        // The game is responsible for deciding when to actually resize buffers.
        // It might need pixel-perfect resize informatin for mouse interactions.

        u32 w = LOWORD(lParam);
        u32 h = HIWORD(lParam);

        Game_Resolution* oldResolution = &gWin32State.clientRes;
        if (oldResolution->w != w || oldResolution->h != h) {
            game_resize({w, h});

            gWin32State.clientRes.w = w;
            gWin32State.clientRes.h = h;
            gWin32State.xCenter = w/2;
            gWin32State.yCenter = h/2;
        }

        return 0;
    }
    case WM_RBUTTONDOWN: {
        mouse.dragging   = true;
        mouse.dragStartX = GET_X_LPARAM(lParam);
        mouse.dragStartY = GET_Y_LPARAM(lParam);

        mouse.x = gWin32State.xCenter;
        mouse.y = gWin32State.yCenter;
        mouse.ignoreNextMouseMove = true;
        win32_center_cursor();

        ShowCursor(FALSE);
        SetCapture(hwnd);
        return 0;
    }
    case WM_RBUTTONUP: {
        mouse.dragging = false;

        POINT p = { down_cast<LONG>(mouse.dragStartX), down_cast<LONG>(mouse.dragStartY) };
        ClientToScreen(hwnd, &p);
        SetCursorPos(p.x, p.y);

        ShowCursor(TRUE);
        ReleaseCapture();
        return 0;
    }
    case WM_MOUSELEAVE: {
        mouse.dragging = false;
        ShowCursor(TRUE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (mouse.ignoreNextMouseMove) {
            mouse.ignoreNextMouseMove = false;
            return 0;
        }

        POINTS pos = MAKEPOINTS(lParam);

        if (mouse.dragging) {
            mouse.xDragAccum += pos.x - gWin32State.xCenter;
            mouse.yDragAccum += gWin32State.yCenter - pos.y;
            mouse.ignoreNextMouseMove = true;
            win32_center_cursor();
        }
        else {
            mouse.x = pos.x;
            mouse.y = pos.y;
        }

        return 0;
    }
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_ESCAPE: win32_game_key_down(GK_ESCAPE, keys, mod); break;
        case 'W':       win32_game_key_down(GK_W,      keys, mod); break;
        case 'A':       win32_game_key_down(GK_A,      keys, mod); break;
        case 'S':       win32_game_key_down(GK_S,      keys, mod); break;
        case 'D':       win32_game_key_down(GK_D,      keys, mod); break;
        case 'E':       win32_game_key_down(GK_E,      keys, mod); break;
        case '1':       win32_game_key_down(GK_1,      keys, mod); break;
        case '2':       win32_game_key_down(GK_2,      keys, mod); break;
        case '3':       win32_game_key_down(GK_3,      keys, mod); break;
        case '4':       win32_game_key_down(GK_4,      keys, mod); break;
        default: break;
        }

        // NOTE: enable to allow windows to handle hotkeys.
#if 1
        if (message == WM_SYSKEYDOWN) break;
        else return 0;
#else
        return 0;
#endif

    case WM_SYSKEYUP:
    case WM_KEYUP:
        switch (wParam) {
        case VK_ESCAPE: win32_game_key_up(GK_ESCAPE, keys, mod); break;
        case 'W':       win32_game_key_up(GK_W,      keys, mod); break;
        case 'A':       win32_game_key_up(GK_A,      keys, mod); break;
        case 'S':       win32_game_key_up(GK_S,      keys, mod); break;
        case 'D':       win32_game_key_up(GK_D,      keys, mod); break;
        case 'E':       win32_game_key_up(GK_E,      keys, mod); break;
        case '1':       win32_game_key_up(GK_1,      keys, mod); break;
        case '2':       win32_game_key_up(GK_2,      keys, mod); break;
        case '3':       win32_game_key_up(GK_3,      keys, mod); break;
        case '4':       win32_game_key_up(GK_4,      keys, mod); break;
        default: break;
        }

#if 1
        if (message == WM_SYSKEYUP) break;
        else return 0;
#else
        return 0;
#endif

    default:
        break;
    }

    return DefWindowProcA(hwnd, message, wParam, lParam);
}

static inline void
win32_poll_events(Win32_State* state)
{
    Game_Input* input = &gWin32State.game->input;
    input->controller.prev = input->controller.cur;
    input->keyboard.prev   = input->keyboard.cur;
    input->mouse.prev      = input->mouse.cur;

    state->mouse.xDragAccum = 0;
    state->mouse.yDragAccum = 0;

    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    Game_Mouse_State& gameMouse = input->mouse.cur;
    gameMouse.x = state->mouse.x;
    gameMouse.y = state->mouse.y;

    gameMouse.dragging = state->mouse.dragging;
    gameMouse.xDrag = state->mouse.xDragAccum;
    gameMouse.yDrag = state->mouse.yDragAccum;

    win32_poll_xinput(state);
}

static inline void
win32_update_frame_stats(Game_Frame_Stats* stats, u32 frameTimeMicro)
{
    // TODO: callback on frame time misses?

    if (stats->framesAveraged == frames_to_average()) {
        stats->framesAveraged = 1;
        stats->frameTimeAverage = frameTimeMicro;
        stats->frameTimeAccumulator = 0;
    }
    else {
        stats->frameTimeAccumulator += frameTimeMicro;
        stats->frameTimeAverage = ((f64)stats->frameTimeAccumulator)/stats->framesAveraged;
        stats->framesAveraged++;
    }
}

static void 
win32_game_loop(Win32_State* win32State)
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER prev;
    QueryPerformanceCounter(&prev);

    u32 lagMicro  = 0;
    u32 stepMicro = us_per_update();

    b32 firstFrame = true;

    for (;;) {
        win32_poll_events(win32State);

        LARGE_INTEGER current;
        QueryPerformanceCounter(&current);

        LARGE_INTEGER elapsed;
        elapsed.QuadPart = current.QuadPart - prev.QuadPart;
        elapsed.QuadPart *= 1000000;
        elapsed.QuadPart /= frequency.QuadPart;

        prev      = current;
        lagMicro += (u32)elapsed.QuadPart;

        game_update(lagMicro/1000000.0f);
        if (win32State->shouldQuit || gGame->shouldQuit)
            return;

        // Make sure that the first frame average isn't bogus, not that it really matters.
        if (!firstFrame) { win32_update_frame_stats(&gGame->frameStats, (u32)elapsed.QuadPart); }
        else             { firstFrame = false; }

        // Consume lag for updating in fixed steps.
        for (; lagMicro >= stepMicro; lagMicro -= stepMicro) {
            if (should_step())
                game_step();
        }

        game_play_sound();

        if (should_step()) { game_render(lagMicro/(f32)stepMicro); }
        else               { game_render(1); }

        game_end_frame();

        win32_swap_buffers(win32State);
    }
}

static inline Win32_Window_Position
win32_centered_window(int w, int h)
{
    Win32_Window_Position result;
    result.x = GetSystemMetrics(SM_CXSCREEN)/2 - w/2;
    result.y = GetSystemMetrics(SM_CYSCREEN)/2 - h/2;

    return result;
}

static inline void
win32_show_window(Win32_State* state)
{
    win32_swap_buffers(state);

    HWND hwnd = state->hwnd;
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
}

static inline b32
win32_setup_console(Win32_State* state)
{
    if (!AllocConsole()) return false;

    state->console = CreateFileA("CONOUT$", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, INVALID_HANDLE_VALUE);

    if (state->console == INVALID_HANDLE_VALUE)
        return false;

#if 0
    // BUG: these break STD_* handles in weird ways, causing writes to them to return invalid handle errors!
    if (!freopen("CON", "w", stdout)) return false;
    if (!freopen("CON", "w", stderr)) return false;
#endif

    return true;
}

// Registers a "Dummy" window class for the fake opengl context
// and a "Real" window class for the final one.
static inline b32
win32_register_window_classes(HINSTANCE instance)
{
    WNDCLASSEXA wc   = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = DefWindowProcA;
    wc.hInstance     = instance;
    wc.hIcon         = LoadIconA(NULL, IDI_WINLOGO);
    wc.hIconSm       = wc.hIcon;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = "Dummy";

    if (!RegisterClassExA(&wc))
        return false;

    // The only differences are the event callback and the class name.
    // We just need to make sure that they have different event callbacks.
    wc.lpfnWndProc   = win32_event_callback;
    wc.lpszClassName = "Real";

    if (!RegisterClassExA(&wc))
        return false;

    return true;
}

static b32
win32_create_window(Win32_State* state, HINSTANCE instance,
                    const char* windClass, const char* title,
                    int w, int h)
{
    DWORD styleFlags = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    Win32_Window_Position pos = win32_centered_window(w, h);

    HWND hwnd = CreateWindowExA(0, windClass, title, styleFlags,
                                pos.x, pos.y, w, h, NULL, NULL, instance, NULL);

    if (!hwnd)
        return false;

    state->hwnd = hwnd;
    state->dc   = GetDC(hwnd);

    return true;
}

static inline void
win32_close_opengl_window(Win32_State* state)
{
    wglMakeCurrent(state->dc, NULL);
    wglDeleteContext(state->glrc);
    ReleaseDC(state->hwnd, state->dc);
    DestroyWindow(state->hwnd);

    state->dc   = NULL;
    state->glrc = NULL;
    state->hwnd = NULL;
}

static b32
win32_create_opengl_window(Win32_State* state, HINSTANCE instance, int w, int h)
{
    if (!win32_create_window(state, instance, "Dummy", NULL, w, h))
        return false;

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;
    pfd.cAlphaBits   = 8;
    // NOTE: as long as we render to an offscreen buffer, we don't need depth/stencil on the main buffer.
    // pfd.cDepthBits   = 0 /* 24 */;
    // pfd.cStencilBits = 0 /* 8 */;
    pfd.cDepthBits   = 24 /* 24 */;
    pfd.cStencilBits = 8 /* 8 */;
    pfd.iLayerType   = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(state->dc, &pfd);
    if (!pf || !SetPixelFormat(state->dc, pf, &pfd))
        return false;

    // Grab the fake context to get/check for necessary extensions.
    state->glrc = wglCreateContext(state->dc);
    wglMakeCurrent(state->dc, state->glrc);

    PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB =
         (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");

    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
         (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
         (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

    if (!wglChoosePixelFormatARB || !wglCreateContextAttribsARB || !wglSwapIntervalEXT) {
        MessageBoxA(state->hwnd, "Missing Required OpenGL Extension(s).",
                    "OpenGL Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // TODO: check for multisampling extension.
    // TODO: check for WGL_ARB_create_context_profile
    // NOTE: there is almost no chance that these won't be present.

    // Done with the fake context. Close the old window, and make a new one with the
    // right opengl settings.

    win32_close_opengl_window(state);
    if (!win32_create_window(state, instance, "Real", "Tanks!", w, h))
        return false;

    // NOTE: this is how we could get a multisampled default framebuffer, but I am
    //       going to use the FBO + blit method for now.
#if 0
    // MSAA 4x format: https://www.khronos.org/opengl/wiki/Multisampling
    int attributes[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_SAMPLE_BUFFERS_ARB, 1, // Number of buffers (must be 1 at time of writing)
        WGL_SAMPLES_ARB, 4,        // Number of samples
        0
    };

    UINT numFormats = 0;
    if (!wglChoosePixelFormatARB(state->dc, attributes, NULL, 1, &pf, &numFormats))
        return false;
#else
    pf = ChoosePixelFormat(state->dc, &pfd);
    if (!pf || !SetPixelFormat(state->dc, pf, &pfd))
        return false;
#endif

    int contextAttribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, target_opengl_version_major(),
        WGL_CONTEXT_MINOR_VERSION_ARB, target_opengl_version_minor(),
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    HGLRC glrc = wglCreateContextAttribsARB(state->dc, NULL, contextAttribs);
    if (!glrc) {
        MessageBoxA(state->hwnd, "Failed to create an OpenGL 4.3 context.",
                    "OpenGL Error", MB_OK | MB_ICONERROR);
        return false;
    }

    state->glrc = glrc;
    wglMakeCurrent(state->dc, glrc);
    wglSwapIntervalEXT(1);

    return true;
}

static inline b32
win32_grab_opengl_functions(Win32_State* state)
{
    if (gl3wInit() != 0) {
        MessageBoxA(state->hwnd, "Failed to initialize OpenGL.",
                    "OpenGL Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

static inline u64
required_pages(u64 bytes, u32 pageSize)
{
    u64 wholePages = bytes / pageSize;
    bool extraPage = (bool)(bytes % pageSize);

    return wholePages + extraPage;
}

// Reserve one big contiguous memory region with room for all arenas + 1 guard page after each.
static inline void
win32_allocate_memory(Win32_State* state, Game_Memory* request)
{
    constexpr umm kArenaCount = ArraySize(request->arenas);

    umm firstChunkOffsets[kArenaCount] = {};
    umm fullContiguousSize = 0;

    // Figure out where the first chunk of each arena is, so we can commit them.
    umm firstChunkOffset = 0;
    for (int i = 0; i < kArenaCount; i++) {
        Memory_Arena& arena  = request->arenas[i];

        firstChunkOffsets[i] = firstChunkOffset;
        fullContiguousSize  += arena.max;
        firstChunkOffset    += arena.max + state->pageSize;
    }

    fullContiguousSize += kArenaCount * state->pageSize;

    // Allocate the big contiguous block.
    u8* contiguousRegion = (u8*)VirtualAlloc(NULL, fullContiguousSize, MEM_RESERVE, PAGE_NOACCESS);
    assert(contiguousRegion);

    // Go through and commit arena memory and fill out the structs.
    for (int i = 0; i < kArenaCount; i++) {
        Memory_Arena& arena = request->arenas[i];
        assert(arena.start = VirtualAlloc(contiguousRegion + firstChunkOffsets[i], arena.size,
                                          MEM_COMMIT, PAGE_READWRITE));
        arena.at     = arena.start;
        arena.next   = (u8*)arena.start + arena.size;
    }

    state->contiguousRegion = contiguousRegion;
}

static b32
win32_failed_expand_arena(Memory_Arena* arena)
{
    fprintf(stderr, "(WIN32): Failed to expand the '%s' arena!\n", arena->tag);
    assert(!"arena expansion failure");

    return false;
}

static b32
win32_expand_arena(Memory_Arena* arena)
{
    if (!VirtualAlloc((u8*)arena->next, arena->size, MEM_COMMIT, PAGE_READWRITE))
        return win32_failed_expand_arena(arena);

    (u8*&)arena->next += arena->size;

    return true;
}

static void*
win32_read_entire_file(const char* name, Memory_Arena* arena, umm* size, u32 alignment)
{
    // TODO: logging
    HANDLE file = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER fileSize = {};
    assert(GetFileSizeEx(file, &fileSize));

    void* start = arena->at;

    DWORD bytesRead = 0;
    if (!ReadFile(file, arena_push(*arena, fileSize.QuadPart, alignment),
                  down_cast<DWORD>(fileSize.QuadPart), &bytesRead, NULL)) {
        return NULL;
    }

    CloseHandle(file);

    *size = bytesRead;
    start = align_up(start, alignment);

    return start;
}

// @HackLeaf
static void
win32_log(Log_Level level, const char* str, s32 len)
{
    UNREFERENCED_PARAMETER(level);

    if (len == -1) { len = down_cast<s32>(strlen(str)); }

    WriteFile(gWin32State.console, str, len, NULL, NULL);
}

static inline void
win32_grab_platform(Platform* platform)
{
    platform->expand_arena        = win32_expand_arena;
    platform->failed_expand_arena = win32_failed_expand_arena;
    platform->read_entire_file    = win32_read_entire_file;
    platform->log                 = win32_log;

    platform->initialized = true;
}

static inline void
win32_init(Win32_State* state, Platform* platformOut, Game_Memory* memoryOut,
           Game_Resolution windowRes, Game_Resolution* clientResOut)
{
    HINSTANCE instance = GetModuleHandleA(NULL);

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    state->pageSize  = sysInfo.dwPageSize;
    state->coreCount = sysInfo.dwNumberOfProcessors;

    // TODO: do better than asserts.
    assert(win32_setup_console(state));
    assert(win32_register_window_classes(instance));
    assert(win32_create_opengl_window(state, instance, windowRes.w, windowRes.h));
    assert(win32_grab_opengl_functions(state));
    // printf("OpenGL Version: %s\n", glGetString(GL_VERSION));

    win32_grab_platform(platformOut);
    win32_allocate_memory(state, &(*memoryOut = game_get_memory_request(platformOut)));

    RECT clientRect;
    GetClientRect(state->hwnd, &clientRect);

    clientResOut->w = clientRect.right  - clientRect.left;
    clientResOut->h = clientRect.bottom - clientRect.top;

    state->clientRes = *clientResOut;
    state->xCenter   = clientResOut->w/2;
    state->yCenter   = clientResOut->h/2;
}

static void
win32_shutdown(Win32_State* state, Game_Memory*)
{
    // @leak game_memory b/c it will get cleaned up just fine on exit.
    win32_close_opengl_window(state);
}

extern int WINAPI
WinMain(HINSTANCE /* hInstance */,
        HINSTANCE /* hPrevInstance */,
        LPSTR /* lpCmdLine */,
        int /* nShowCmd */)
{
    Game_Resolution windowRes = { 1280, 720 };
    Game_Resolution clientRes = windowRes;
    Game_Memory memory;

    Platform platform;
    win32_init(&gWin32State, &platform, &memory, windowRes, &clientRes);

    assert(game_init(&memory, &platform, clientRes));
    win32_show_window(&gWin32State);

    gWin32State.game = gGame;
    win32_game_loop(&gWin32State);

    game_quit();
    win32_shutdown(&gWin32State, &memory);

    return 0;
}
