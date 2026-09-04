#pragma once
#include "render/Color.h"

#include <string>
#include <string_view>
#include <vector>

namespace kshell::render {

// A single text cell: a wide character plus foreground/background colors.
struct Cell
{
    wchar_t ch = L' ';
    Color   fg;
    Color   bg;
    bool    bold = false;

    bool operator==(const Cell& o) const { return ch == o.ch && fg == o.fg && bg == o.bg && bold == o.bold; }
    bool operator!=(const Cell& o) const { return !(*this == o); }
};

// A dummy color used to represent "transparent / inherit background".
inline const Color kTransparent = Color(0, 0, 0);

// Screen is a virtual double-buffered grid (rows x cols). Drawing operations
// mutate the front buffer; present() diff's it against the previous back
// buffer and returns only the changed line spans (or applies to a writer).
//
// The screen never talks to the Win32 console directly; a thin writer layer
// applies a screen to the real console. This keeps the buffer fully testable.
class Screen
{
public:
    void resize(int rows, int cols);
    int  rows() const { return rows_; }
    int  cols() const { return cols_; }

    void clear(const Color& bg, const Color& fg);
    void clearCell(int row, int col, const Color& bg, const Color& fg);

    void put(int row, int col, wchar_t ch, const Color& fg, const Color& bg, bool bold = false);
    void putCell(int row, int col, const Cell& cell);

    // Draw string at row starting at col. Strings longer than the line are
    // clipped. Handles wide characters by printing them as one cell.
    void putText(int row, int col, std::wstring_view text, const Color& fg, const Color& bg, bool bold = false);
    void fillRect(int row, int col, int h, int w, wchar_t ch, const Color& fg, const Color& bg);
    void fillLine(int row, int col, int len, wchar_t ch, const Color& fg, const Color& bg);

    void drawBorder(int row, int col, int h, int w, const Color& fg, const Color& bg);

    const Cell& cellAt(int row, int col) const { return cells_[(size_t)row * cols_ + col]; }
    Cell&       cellAt(int row, int col) { return cells_[(size_t)row * cols_ + col]; }

    // Copy the front buffer into `back` and return which rows changed.
    // `back` must be pre-sized by countDirty row since last present.
    std::vector<int> present(std::vector<Cell>& back);

    // Access the current front buffer (testing / writer use).
    const std::vector<Cell>& front() const { return cells_; }

private:
    int                  rows_ = 0;
    int                  cols_ = 0;
    std::vector<Cell>    cells_;
};

} // namespace kshell::render
