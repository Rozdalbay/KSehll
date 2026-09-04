#include "render/Screen.h"

#include <algorithm>
#include <cassert>

namespace kshell::render {

void Screen::resize(int rows, int cols)
{
    if (rows < 0)
    {
        rows = 0;
    }
    if (cols < 0)
    {
        cols = 0;
    }
    rows_ = rows;
    cols_ = cols;
    cells_.assign((size_t)rows * cols, Cell{});
    clear(Color::rgb(0, 0, 0), Color::rgb(220, 220, 220));
}

void Screen::clear(const Color& bg, const Color& fg)
{
    for (auto& c : cells_)
    {
        c = Cell{};
        c.ch = L' ';
        c.fg = fg;
        c.bg = bg;
    }
}

void Screen::clearCell(int row, int col, const Color& bg, const Color& fg)
{
    if (row < 0 || row >= rows_ || col < 0 || col >= cols_)
    {
        return;
    }
    Cell& c = cells_[(size_t)row * cols_ + col];
    c.ch = L' ';
    c.fg = fg;
    c.bg = bg;
    c.bold = false;
}

void Screen::put(int row, int col, wchar_t ch, const Color& fg, const Color& bg, bool bold)
{
    if (row < 0 || row >= rows_ || col < 0 || col >= cols_)
    {
        return;
    }
    Cell& c = cells_[(size_t)row * cols_ + col];
    c.ch = ch;
    c.fg = fg;
    c.bg = bg;
    c.bold = bold;
}

void Screen::putCell(int row, int col, const Cell& cell)
{
    if (row < 0 || row >= rows_ || col < 0 || col >= cols_)
    {
        return;
    }
    cells_[(size_t)row * cols_ + col] = cell;
}

void Screen::putText(int row, int col, std::wstring_view text, const Color& fg, const Color& bg, bool bold)
{
    if (row < 0 || row >= rows_)
    {
        return;
    }
    int x = col;
    for (wchar_t ch : text)
    {
        if (x >= cols_)
        {
            break;
        }
        if (x >= 0)
        {
            Cell& c = cells_[(size_t)row * cols_ + x];
            c.ch = ch;
            c.fg = fg;
            c.bg = bg;
            c.bold = bold;
        }
        ++x;
    }
}

void Screen::fillRect(int row, int col, int h, int w, wchar_t ch, const Color& fg, const Color& bg)
{
    for (int r = row; r < row + h; ++r)
    {
        fillLine(r, col, w, ch, fg, bg);
    }
}

void Screen::fillLine(int row, int col, int len, wchar_t ch, const Color& fg, const Color& bg)
{
    if (row < 0 || row >= rows_)
    {
        return;
    }
    for (int i = 0; i < len; ++i)
    {
        int x = col + i;
        if (x < 0 || x >= cols_)
        {
            continue;
        }
        Cell& c = cells_[(size_t)row * cols_ + x];
        c.ch = ch;
        c.fg = fg;
        c.bg = bg;
        c.bold = false;
    }
}

void Screen::drawBorder(int row, int col, int h, int w, const Color& fg, const Color& bg)
{
    if (h <= 0 || w <= 0)
    {
        return;
    }
    // top & bottom
    put(row, col, L'\u250c', fg, bg); // ┌
    if (w >= 2)
    {
        put(row, col + w - 1, L'\u2510', fg, bg); // ┐
    }
    for (int x = 1; x < w - 1; ++x)
    {
        put(row, col + x, L'\u2500', fg, bg);
    }
    if (h >= 2)
    {
        put(row + h - 1, col, L'\u2514', fg, bg); // └
        put(row + h - 1, col + w - 1, L'\u2518', fg, bg); // ┘
        for (int x = 1; x < w - 1; ++x)
        {
            put(row + h - 1, col + x, L'\u2500', fg, bg);
        }
    }
    for (int y = 1; y < h - 1; ++y)
    {
        put(row + y, col, L'\u2502', fg, bg);
        if (w >= 2)
        {
            put(row + y, col + w - 1, L'\u2502', fg, bg);
        }
    }
}

std::vector<int> Screen::present(std::vector<Cell>& back)
{
    std::vector<int> dirty;
    if (back.size() != cells_.size())
    {
        back = cells_;
        dirty.reserve(rows_);
        for (int r = 0; r < rows_; ++r)
        {
            dirty.push_back(r);
        }
        return dirty;
    }
    for (int r = 0; r < rows_; ++r)
    {
        const size_t base = (size_t)r * cols_;
        bool         rowChanged = false;
        for (int c = 0; c < cols_; ++c)
        {
            if (cells_[base + (size_t)c] != back[base + (size_t)c])
            {
                rowChanged = true;
                break;
            }
        }
        if (rowChanged)
        {
            dirty.push_back(r);
        }
    }
    back = cells_;
    return dirty;
}

} // namespace kshell::render
