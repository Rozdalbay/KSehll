#pragma once

namespace kshell::ui
{

struct Size
{
    int width = 0;
    int height = 0;
};

// Rectangle over the screen grid, origin top-left.
struct Rect
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    int right() const { return x + w; }
    int bottom() const { return y + h; }

    bool contains(int px, int py) const
    {
        return px >= x && px < right() && py >= y && py < bottom();
    }

    // Rect clamped to fit within [0, 0, maxW, maxH].
    Rect clampTo(int maxW, int maxH) const
    {
        Rect r = *this;
        if (r.x < 0)
        {
            r.w += r.x;
            r.x = 0;
        }
        if (r.y < 0)
        {
            r.h += r.y;
            r.y = 0;
        }
        if (r.right() > maxW)
        {
            r.w -= (r.right() - maxW);
        }
        if (r.bottom() > maxH)
        {
            r.h -= (r.bottom() - maxH);
        }
        if (r.w < 0)
        {
            r.w = 0;
        }
        if (r.h < 0)
        {
            r.h = 0;
        }
        return r;
    }

    // Split horizontally (side-by-side) at ratio t in [0,1]; returns [left, right].
    void splitVertical(float t, Rect& left, Rect& right) const
    {
        int lw = static_cast<int>(w * t);
        if (lw < 0)
        {
            lw = 0;
        }
        left = {x, y, lw, h};
        right = {x + lw, y, w - lw, h};
    }

    // Split vertically (stacked) at ratio t; returns [top, bottom].
    void splitHorizontal(float t, Rect& top, Rect& bottom) const
    {
        int th = static_cast<int>(h * t);
        if (th < 0)
        {
            th = 0;
        }
        top = {x, y, w, th};
        bottom = {x, y + th, w, h - th};
    }
};

} // namespace kshell::ui
