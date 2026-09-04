#pragma once

#include <cstdint>
#include <string>

namespace kshell::ui
{

enum class Key : uint8_t
{
    None = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4,
    Digit5, Digit6, Digit7, Digit8, Digit9,
    Enter, Tab, Backspace, Delete, Escape,
    Space, Up, Down, Left, Right, Home, End,
    PageUp, PageDown, F1, F2, F3, F4, F5, F6,
    F7, F8, F9, F10, F11, F12,
    Insert, Backtick, Slash, Backslash, Minus, Equals,
    BracketOpen, BracketClose, Semicolon, Quote, Comma, Period,
};

// A parsed key event: a key plus modifier flags. Character-input events
// (printable text) carry a Unicode `ch` instead of a Key.
struct KeyEvent
{
    Key       key = Key::None;
    wchar_t   ch = 0;             // set when this is a text-input event
    bool      ctrl = false;
    bool      shift = false;
    bool      alt = false;

    bool isText() const { return key == Key::None && ch != 0; }
    bool isPrint() const { return ch != 0 && !ctrl && !alt; }
};

struct Modifiers
{
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool rightAlt = false;
};

} // namespace kshell::ui
