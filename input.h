#pragma once
#include "primitives.h"

enum Game_Key
{
    GK_ESCAPE,
    GK_W,
    GK_A,
    GK_S,
    GK_D,
    GK_E,
    GK_0,
    GK_1,
    GK_2,
    GK_3,
    GK_4,
    GK_5,
    GK_6,
    GK_7,
    GK_8,
    GK_9,
    GK_NUM_
};

enum Game_Keymod : u8
{
    GKM_LSHIFT = 0x01,
    GKM_RSHIFT = 0x02,
    GKM_LCNTL  = 0x04,
    GKM_RCNTL  = 0x08,
    GKM_LALT   = 0x10,
    GKM_RALT   = 0x20,

    GKM_SHIFT = GKM_LSHIFT | GKM_RSHIFT,
    GKM_CNTL  = GKM_LCNTL  | GKM_RCNTL,
    GKM_ALT   = GKM_LALT   | GKM_RALT,
    GKM_CS    = GKM_CNTL   | GKM_SHIFT,
    GKM_CA    = GKM_CNTL   | GKM_ALT,
    GKM_SA    = GKM_SHIFT  | GKM_ALT,
    GKM_CSA   = GKM_CNTL | GKM_SHIFT | GKM_ALT,
    GKM_ALL   = GKM_CSA
};

struct Game_Keyboard_State
{
    bit32 keys;
    flag8 mod[GK_NUM_];
};

enum Game_Mouse_Button : u32
{
    MOUSE_LEFT   = 0x01,
    MOUSE_RIGHT  = 0x02,
    MOUSE_MIDDLE = 0x04,
};

struct Game_Mouse_State
{
    u32 buttons = 0;
    u32 x = 0;
    u32 y = 0;

    b32 dragging = false;
    s32 xDrag = 0;
    s32 yDrag = 0;
};

struct Game_Mouse
{
    Game_Mouse_State cur;
    Game_Mouse_State prev;

    b32 drag_started() const { return !prev.dragging && cur.dragging; }
};


enum Game_Controller_Button : u32
{
    BUTTON_A     = 0x01,
    BUTTON_B     = 0x02,
    BUTTON_X     = 0x04,
    BUTTON_Y     = 0x08,
};

enum Game_Axis
{
    AXIS_LEFT_X,
    AXIS_LEFT_Y,
    AXIS_RIGHT_X,
    AXIS_RIGHT_Y,
    AXIS_NUM_
};


struct Game_Controller_State
{
    u32 buttons = 0;
    float axes[AXIS_NUM_] = { 0.0f };
};

struct Game_Controller
{
    Game_Controller_State cur;
    Game_Controller_State prev;
};

struct Game_Keyboard
{
    Game_Keyboard_State cur;
    Game_Keyboard_State prev;

    b32 pressed(Game_Key key) const;
    b32 pressed(Game_Key key, Game_Keymod mod, Game_Keymod mod2 = GKM_ALL, Game_Keymod mod3 = GKM_ALL) const;
    b32 pressed_exactly(Game_Key key, Game_Keymod mod) const;

    b32 released(Game_Key key) const;
    b32 released(Game_Key key, Game_Keymod mod, Game_Keymod mod2 = GKM_ALL, Game_Keymod mod3 = GKM_ALL) const;
    b32 released_exactly(Game_Key key, Game_Keymod mod) const;

    b32 held(Game_Key key) const;
    b32 held(Game_Key key, Game_Keymod mod, Game_Keymod mod2 = GKM_ALL, Game_Keymod mod3 = GKM_ALL) const;
    b32 held_exactly(Game_Key key, Game_Keymod mod) const;

    b32 down(Game_Key key) const;
    b32 down(Game_Key key, Game_Keymod mod, Game_Keymod mod2 = GKM_ALL, Game_Keymod mod3 = GKM_ALL) const;
    b32 down_exactly(Game_Key key, Game_Keymod mod) const;
};

struct Game_Input
{
    Game_Keyboard keyboard;
    Game_Controller controller;
    Game_Mouse mouse;
};

inline b32
Game_Keyboard::pressed(Game_Key key) const
{
    b32 wasDown = prev.keys.is_set(key);
    b32 isDown  = cur.keys.is_set(key);

    return !wasDown && isDown;
}

inline b32
Game_Keyboard::pressed(Game_Key key, Game_Keymod mod, Game_Keymod mod2, Game_Keymod mod3) const
{
    flag8 prevMod = prev.mod[key];
    flag8 curMod  = cur.mod[key];

    b32 wasDown = (prevMod & mod) && (prevMod & mod2) && (prevMod & mod3) && prev.keys.is_set(key);
    b32 isDown  = (curMod  & mod) && (curMod  & mod2) && (curMod  & mod3) && cur.keys.is_set(key);

    return !wasDown && isDown;
}

inline b32
Game_Keyboard::pressed_exactly(Game_Key key, Game_Keymod mod) const
{
    b32 wasDown = (prev.mod[key] & mod) == mod && prev.keys.is_set(key);
    b32 isDown  = (cur.mod[key]  & mod) == mod && cur.keys.is_set(key);

    return !wasDown && isDown;
}

inline b32
Game_Keyboard::released(Game_Key key) const
{
    b32 wasDown = prev.keys.is_set(key);
    b32 isDown  = cur.keys.is_set(key);

    return wasDown && !isDown;
}

inline b32
Game_Keyboard::released(Game_Key key, Game_Keymod mod, Game_Keymod mod2, Game_Keymod mod3) const
{
    flag8 prevMod = prev.mod[key];
    flag8 curMod  = cur.mod[key];

    b32 wasDown = (prevMod & mod) && (prevMod & mod2) && (prevMod & mod3) && prev.keys.is_set(key);
    b32 isDown  = (curMod  & mod) && (curMod  & mod2) && (curMod  & mod3) && cur.keys.is_set(key);

    return wasDown && !isDown;
}

inline b32
Game_Keyboard::released_exactly(Game_Key key, Game_Keymod mod) const
{
    b32 wasDown = (prev.mod[key] & mod) == mod && prev.keys.is_set(key);
    b32 isDown  = (cur.mod[key]  & mod) == mod && cur.keys.is_set(key);

    return wasDown && !isDown;
}

inline b32
Game_Keyboard::held(Game_Key key) const
{
    b32 wasDown = prev.keys.is_set(key);
    b32 isDown  = cur.keys.is_set(key);

    return wasDown && isDown;
}

inline b32
Game_Keyboard::held(Game_Key key, Game_Keymod mod, Game_Keymod mod2, Game_Keymod mod3) const
{
    flag8 prevMod = prev.mod[key];
    flag8 curMod  = cur.mod[key];

    b32 wasDown = (prevMod & mod) && (prevMod & mod2) && (prevMod & mod3) && prev.keys.is_set(key);
    b32 isDown  = (curMod  & mod) && (curMod  & mod2) && (curMod  & mod3) && cur.keys.is_set(key);

    return wasDown && isDown;
}

inline b32
Game_Keyboard::held_exactly(Game_Key key, Game_Keymod mod) const
{
    b32 wasDown = (prev.mod[key] & mod) == mod && prev.keys.is_set(key);
    b32 isDown  = (cur.mod[key]  & mod) == mod && cur.keys.is_set(key);

    return wasDown && isDown;
}

inline b32
Game_Keyboard::down(Game_Key key) const
{
    b32 isDown = cur.keys.is_set(key);

    return isDown;
}

inline b32
Game_Keyboard::down(Game_Key key, Game_Keymod mod, Game_Keymod mod2, Game_Keymod mod3) const
{
    flag8 curMod = cur.mod[key];
    b32 isDown    = (curMod & mod) && (curMod & mod2) && (curMod & mod3) && cur.keys.is_set(key);

    return isDown;
}

inline b32
Game_Keyboard::down_exactly(Game_Key key, Game_Keymod mod) const
{
    b32 isDown = (cur.mod[key] & mod) == mod && cur.keys.is_set(key);

    return isDown;
}

struct Input_Smoother
{
    f32 mouseDragXValues[2] = {};
    f32 mouseDragYValues[2] = {};
    f32_window mouseDragYWindow;
    f32_window mouseDragXWindow;

    Input_Smoother();

    auto smooth_mouse_drag(f32 x, f32 y) -> v2;
    auto reset_mouse_drag() -> void;
};

inline
Input_Smoother::Input_Smoother()
{
    mouseDragXWindow.reset(mouseDragXValues, ArraySize(mouseDragXValues), 0);
    mouseDragYWindow.reset(mouseDragYValues, ArraySize(mouseDragYValues), 0);
}

inline v2
Input_Smoother::smooth_mouse_drag(f32 x, f32 y)
{
    mouseDragXWindow.add(x);
    mouseDragYWindow.add(y);

    return v2(mouseDragXWindow.average, mouseDragYWindow.average);
}

inline void
Input_Smoother::reset_mouse_drag()
{
    mouseDragXWindow.reset(0);
    mouseDragYWindow.reset(0);
}
