#include "tanks.cpp"

#include <GL/gl3w.h>
#include <GL/gl.h>
#include <GL/wglext.h>

#include "imgui_impl_win32.h"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>

#include <cassert>
#include <cstdio>


struct Win32_Mouse_State
{
    s32 x = 0;
    s32 y = 0;
    u32 buttons = 0;
    float mouseWheel = 0;
    float mouseHWheel = 0;
    b32 leave = false;
};

struct Win32_Keyboard_State
{
    bool keys[512]       = {};
    u16  textInput[16+1] = {};
    u8   textSize        = 0;
};

struct Win32_State
{
    void* contiguousRegion = NULL;
    u32 pageSize  = 4096;
    u32 coreCount = 2;

    Game* game = NULL;
    HWND  hwnd = NULL;
    HDC   dc   = NULL;
    HGLRC glrc = NULL;

    HANDLE console = INVALID_HANDLE_VALUE;

    LARGE_INTEGER frequency = {};
    LARGE_INTEGER imguiPrev = {};

    Win32_Mouse_State mouse;
    Win32_Mouse_State prevMouse;

    s32 dragStartX = 0;
    s32 dragStartY = 0;
    b32 dragging   = false;

    Win32_Keyboard_State keyboard;
    Win32_Keyboard_State prevKeyboard;

    // NOTE(blake): latest WM_SIZE information so we can only call game_resize once after
    // all events have been handled for a frame. Otherwise, draging the window
    // around enough will spam game_resize. In the current implementation of that, that
    // overflows a render command buffer. That probably won't be true forever, but this is
    // still the sane way to go until resizing the window doesn't block.
    //
    Game_Resolution latestClientRes;

    Game_Resolution clientRes;
    u32 xCenter = 0;
    u32 yCenter = 0;

    b32 shouldQuit = false;
};

struct Win32_Window_Position
{
    s32 x = 0;
    s32 y = 0;
};

// global so that the WinProc can access it.
static Win32_State gWin32State;

static inline void
win32_add_input_character(Win32_State* state, u16 c)
{
    u8 size = state->keyboard.textSize;
    if (size + 1 < ArraySize(state->keyboard.textInput)) {
        state->keyboard.textInput[size]     = c;
        state->keyboard.textInput[size + 1] = '\0';
        state->keyboard.textSize++;
    }
}

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
    if (GetKeyState(VK_RSHIFT)   & 0x8000) modStates[key] |= GKM_RSHIFT;
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

static inline bool
win32_handle_non_input_events(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(wParam);

    switch (message) {
    case WM_CLOSE:
        gWin32State.shouldQuit = true;
        *result = 0;
        return true;
    case WM_DESTROY:
        gWin32State.shouldQuit = true;
        PostQuitMessage(0);
        *result = 0;
        return true;
    case WM_SIZE: {
        // NOTE: only resize the game if the resolution has actually changed.
        // Otherwise, we will end up creating all FBO's twice on init.
        // The game is responsible for deciding when to actually resize buffers.
        // It might need pixel-perfect resize informatin for mouse interactions.

        u32 w = LOWORD(lParam);
        u32 h = HIWORD(lParam);

        gWin32State.latestClientRes.w  = w;
        gWin32State.latestClientRes.h  = h;

        *result = 0;
        return true;
    }
    default: break;
    }

    return false;
}

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK
win32_event_callback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#if 0 // Old using imgui implementation
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
        return 1;

    LRESULT result = 0;
    if (win32_handle_non_input_events(hwnd, message, wParam, lParam, &result))
        return result;

    return DefWindowProcA(hwnd, message, wParam, lParam);
#else
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

        gWin32State.latestClientRes.w  = w;
        gWin32State.latestClientRes.h  = h;

        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        u32 button = 0;
        if (message == WM_LBUTTONDOWN) button = MOUSE_LEFT;
        if (message == WM_RBUTTONDOWN) button = MOUSE_RIGHT;
        if (!mouse.buttons && ::GetCapture() == NULL)
            SetCapture(hwnd);

        mouse.buttons |= button;
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
        u32 button = 0;
        if (message == WM_LBUTTONUP) button = MOUSE_LEFT;
        if (message == WM_RBUTTONUP) button = MOUSE_RIGHT;
        mouse.buttons &= ~button;
        if (!mouse.buttons && GetCapture() == hwnd)
            ReleaseCapture();
        return 0;
    }
    case WM_MOUSELEAVE: {
        mouse.leave = true;
        return 0;
    }
    case WM_MOUSEWHEEL:
        mouse.mouseWheel += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
        return 0;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        if (wParam < 256) gWin32State.keyboard.keys[wParam] = 1;
        return 0;
    case WM_SYSKEYUP:
    case WM_KEYUP:
        if (wParam < 256) gWin32State.keyboard.keys[wParam] = 0;
        return 0;
    case WM_CHAR:
        // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
        if (wParam > 0 && wParam < 0x10000)
            win32_add_input_character(&gWin32State, (u16)wParam);
        return 0;
    default:
        break;
    }

    return DefWindowProcA(hwnd, message, wParam, lParam);
#endif
}

static inline void
win32_poll_events(Win32_State* state)
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    win32_poll_xinput(state);
}

static inline b32
win32_mouse_button_pressed(Win32_State* state, u32 button)
{
    return !(state->prevMouse.buttons & button) && (state->mouse.buttons & button);
}

static inline b32
win32_mouse_button_released(Win32_State* state, u32 button)
{
    return (state->prevMouse.buttons & button) && !(state->mouse.buttons & button);
}

static inline void
win32_new_frame(Win32_State* state)
{
    state->prevMouse    = state->mouse;
    state->prevKeyboard = state->keyboard;

    state->mouse.mouseWheel  = 0;
    state->mouse.mouseHWheel = 0;
    state->mouse.leave = false;

    // @BugProne
    //memset(state->keyboard.keys,      0, sizeof(state->keyboard.keys));
    memset(state->keyboard.textInput, 0, sizeof(state->keyboard.textInput));
    state->keyboard.textSize = 0;


    win32_poll_events(state);

    Game_Resolution& oldResolution = state->clientRes;
    Game_Resolution  curResolution = state->latestClientRes;
    if (oldResolution != curResolution) {
        game_resize(curResolution);

        state->clientRes = curResolution;
        state->xCenter   = curResolution.w/2;
        state->yCenter   = curResolution.h/2;
    }

    Win32_Mouse_State&    mouse    = state->mouse;
    Win32_Keyboard_State& keyboard = state->keyboard;

    POINT p = {};
    GetCursorPos(&p);
    ScreenToClient(state->hwnd, &p);

    mouse.x = p.x;
    mouse.y = p.y;

    ImGuiIO& io = ImGui::GetIO();

    // imgui stuff that we want to fill out regardless.
    io.MousePos      = ImVec2(-FLT_MAX, -FLT_MAX);
    io.DisplaySize.x = (float)curResolution.w;
    io.DisplaySize.y = (float)curResolution.h;

    LARGE_INTEGER now;;
    QueryPerformanceCounter(&now);

    LARGE_INTEGER elapsed;
    elapsed.QuadPart = now.QuadPart - state->imguiPrev.QuadPart;
    elapsed.QuadPart *= 1000000;
    elapsed.QuadPart /= state->frequency.QuadPart;
    io.DeltaTime = elapsed.QuadPart/1000000.0f;
    state->imguiPrev = now;


    bool lcontrol = GetKeyState(VK_LCONTROL) & 0x8000;
    bool rcontrol = GetKeyState(VK_RCONTROL) & 0x8000;
    bool lshift   = GetKeyState(VK_LSHIFT)   & 0x8000;
    bool rshift   = GetKeyState(VK_RSHIFT)   & 0x8000;
    bool lalt     = GetKeyState(VK_LMENU)    & 0x8000;
    bool ralt     = GetKeyState(VK_RMENU)    & 0x8000;

    io.ClearInputCharacters();

    if (!state->dragging) {
        io.MousePos     = ImVec2((f32)mouse.x, (f32)mouse.y);
        io.MouseDown[0] = mouse.buttons & MOUSE_LEFT;
        io.MouseDown[1] = mouse.buttons & MOUSE_RIGHT;
        io.MouseDown[2] = mouse.buttons & MOUSE_MIDDLE;
        io.MouseWheel   = mouse.mouseWheel;
        io.MouseWheelH  = mouse.mouseHWheel;
        io.KeyCtrl  = lcontrol || rcontrol;
        io.KeyShift = lshift   || rshift;
        io.KeyAlt   = lalt     || rshift;
        io.KeySuper = false;
        memcpy(io.InputCharacters, keyboard.textInput, sizeof(keyboard.textInput));
        memcpy(io.KeysDown,        keyboard.keys,      sizeof(keyboard.keys));
    }

    // ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    Game_Mouse&    gameMouse    = state->game->input.mouse;
    Game_Keyboard& gameKeyboard = state->game->input.keyboard;

    gameMouse.prev    = gameMouse.cur;
    gameKeyboard.prev = gameKeyboard.cur;

    if (!io.WantCaptureMouse) {
        if (mouse.leave) SetCursor(LoadCursorA(NULL, IDC_ARROW));

        if (win32_mouse_button_pressed(state, MOUSE_RIGHT)) {
            state->dragStartX = mouse.x;
            state->dragStartY = mouse.y;

            SetCursor(NULL);
            win32_center_cursor();

            mouse.x = state->xCenter;
            mouse.y = state->yCenter;
            state->dragging = true;
        }
        else if (win32_mouse_button_released(state, MOUSE_RIGHT) && state->dragging) {
            POINT p = { state->dragStartX, state->dragStartY };
            ClientToScreen(state->hwnd, &p);
            SetCursorPos(p.x, p.y);
            SetCursor(LoadCursorA(NULL, IDC_ARROW));

            state->dragging = false;
        }

        gameMouse.cur.buttons  = 0;
        gameMouse.cur.dragging = state->dragging;
        gameMouse.cur.x        = mouse.x;
        gameMouse.cur.y        = mouse.y;
        gameMouse.cur.xDrag    = mouse.x - state->xCenter;
        gameMouse.cur.yDrag    = state->yCenter - mouse.y;

        if (state->dragging)
            win32_center_cursor();
    }

    bit32& keys = gameKeyboard.cur.keys;
    flag8* mod  = gameKeyboard.cur.mod;

    keys.clear();
    memset(mod, 0, sizeof(gameKeyboard.cur.mod));

    if (!io.WantCaptureKeyboard) {
        flag8 repeatedMod = 0;
        if (lcontrol) repeatedMod |= GKM_LCNTL;
        if (rcontrol) repeatedMod |= GKM_RCNTL;
        if (lshift)   repeatedMod |= GKM_LCNTL;
        if (ralt)     repeatedMod |= GKM_RALT;
        if (lalt)     repeatedMod |= GKM_LALT;

        // @BugProne
        memset(mod, repeatedMod, sizeof(gameKeyboard.cur.mod));
        if (keyboard.keys[VK_ESCAPE]) keys.set(GK_ESCAPE);
        if (keyboard.keys['W'])       keys.set(GK_W);
        if (keyboard.keys['A'])       keys.set(GK_A);
        if (keyboard.keys['S'])       keys.set(GK_S);
        if (keyboard.keys['D'])       keys.set(GK_D);
        if (keyboard.keys['E'])       keys.set(GK_E);
        if (keyboard.keys['1'])       keys.set(GK_1);
        if (keyboard.keys['2'])       keys.set(GK_2);
        if (keyboard.keys['3'])       keys.set(GK_3);
        if (keyboard.keys['4'])       keys.set(GK_4);
    }
}

static inline void
win32_imgui_init(Win32_State* state)
{
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(state->hwnd);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigResizeWindowsFromEdges = true;

    ImGui::StyleColorsDark(0);

    // NOTE(blake): initting imgui is done in game_init() so we can do it after renderer_init().
}

static void 
win32_game_loop(Win32_State* state)
{
    LARGE_INTEGER prev;
    QueryPerformanceCounter(&prev);

    LARGE_INTEGER soundPrev = prev;
    state->imguiPrev        = prev;

    u32 lagMicro  = 0;
    u32 stepMicro = us_per_update();

    b32 firstFrame = true;
    for (;;) {
        win32_new_frame(state);

        LARGE_INTEGER current;
        QueryPerformanceCounter(&current);

        LARGE_INTEGER elapsed;
        elapsed.QuadPart = current.QuadPart - prev.QuadPart;
        elapsed.QuadPart *= 1000000;
        elapsed.QuadPart /= state->frequency.QuadPart;

        prev      = current;
        lagMicro += (u32)elapsed.QuadPart;

        f32 dt = lagMicro/1000000.0f;

        game_update(dt);
        if (state->shouldQuit || gGame->shouldQuit)
            return;

        // Make sure that the first frame average isn't bogus, not that it really matters.
        if (!firstFrame) { gGame->frameStats.frameTimeWindow.add((u32)elapsed.QuadPart); }
        else             { firstFrame = false; }

        // Consume lag for updating in fixed steps.
        for (; lagMicro >= stepMicro; lagMicro -= stepMicro) {
            if (should_step())
                game_step();
        }

        LARGE_INTEGER soundCurrent;
        QueryPerformanceCounter(&soundCurrent);

        LARGE_INTEGER soundElapsed;
        soundElapsed.QuadPart = soundCurrent.QuadPart - soundPrev.QuadPart;
        soundElapsed.QuadPart *= 1000000;
        soundElapsed.QuadPart /= state->frequency.QuadPart;

        soundPrev = soundCurrent;
        game_play_sound(soundElapsed.QuadPart);

#if USING_IMGUI
        ImGui::Render();

        if (should_step()) { game_render(lagMicro/(f32)stepMicro, ImGui::GetDrawData()); }
        else               { game_render(1, ImGui::GetDrawData()); }
#else
        if (should_step()) { game_render(lagMicro/(f32)stepMicro, nullptr); }
        else               { game_render(1, nullptr); }
#endif

        game_end_frame();

        win32_swap_buffers(state);
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
    // NOTE: there is almost no chance that the above won't be present.
    // TODO: check for WGL_EXT_swap_control_tear

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
    wglSwapIntervalEXT(-1);

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
        assert(arena.size % state->pageSize == 0 && arena.max % state->pageSize == 0 &&
               "Arena size and max values must be in multiples of the page size.");

        firstChunkOffsets[i] = firstChunkOffset;
        fullContiguousSize  += arena.max;
        firstChunkOffset    += arena.max + state->pageSize;
    }

    fullContiguousSize += kArenaCount * state->pageSize;

    // Allocate the big contiguous block.
    u8* contiguousRegion = (u8*)VirtualAlloc(NULL, fullContiguousSize, MEM_RESERVE, PAGE_NOACCESS);
    assert(contiguousRegion);

    // Go through and commit the first chunks of each arena and fill out the structs.
    for (int i = 0; i < kArenaCount; i++) {
        Memory_Arena& arena = request->arenas[i];
        arena.start = VirtualAlloc(contiguousRegion + firstChunkOffsets[i], arena.size, MEM_COMMIT, PAGE_READWRITE);
        assert(arena.start);

        arena.at   = arena.start;
        arena.next = (u8*)arena.start + arena.size;
    }

    state->contiguousRegion = contiguousRegion;
}

static b32
win32_failed_expand_arena(Memory_Arena* arena, umm size)
{
    fprintf(stderr, "(WIN32): Failed to expand the '%s' arena! Last allocation was %llu bytes.\n", arena->tag, size);
    assert(!"arena expansion failure");

    return false;
}

static b32
win32_expand_arena(Memory_Arena* arena, umm size)
{
    u8* at = (u8*)arena->at;
    if (at + size > at + arena->max)
        return win32_failed_expand_arena(arena, size);

    RareAssert(is_aligned(arena->next, 4096));
    RareAssert((u8*)arena->next >= at);

    // Acount for the space still left in the current chunk.
    umm neededSize       = size - ((u8*)arena->next - at);
    umm neededSizePadded = (umm)align_up((void*)neededSize, (s32)arena->size);

    if (!VirtualAlloc((u8*)arena->next, neededSizePadded, MEM_COMMIT, PAGE_READWRITE))
        return win32_failed_expand_arena(arena, size);

    (u8*&)arena->next += neededSizePadded;

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
    GetFileSizeEx(file, &fileSize);

    void* start = arena->at;

    DWORD bytesRead = 0;
    if (!ReadFile(file, push(*arena, fileSize.QuadPart, alignment),
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

    LARGE_INTEGER frequency = {};
    QueryPerformanceFrequency(&frequency);

    state->pageSize  = sysInfo.dwPageSize;
    state->coreCount = sysInfo.dwNumberOfProcessors;
    state->frequency = frequency;

    win32_setup_console(state);
    win32_register_window_classes(instance);
    win32_create_opengl_window(state, instance, windowRes.w, windowRes.h);
    win32_grab_opengl_functions(state);

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

    win32_imgui_init(state);
}

static void
win32_shutdown(Win32_State* state, Game_Memory*)
{
    // @Leak game_memory b/c it will get cleaned up just fine on exit.
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

    game_init(&memory, &platform, clientRes);
    win32_show_window(&gWin32State);

    gWin32State.game = gGame;
    win32_game_loop(&gWin32State);

    game_quit();
    win32_shutdown(&gWin32State, &memory);

    return 0;
}

#define STB_IMPLEMENTATION
#include "stb.h"

#include "imgui.cpp"
#include "imgui_widgets.cpp"
#include "imgui_draw.cpp"
#include "imgui_demo.cpp"
#include "imgui_impl_win32.cpp"
