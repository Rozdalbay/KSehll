#pragma once

#include "render/Screen.h"
#include "render/Theme.h"
#include "ui/Geometry.h"

#include <string>
#include <string_view>
#include <vector>

namespace kshell::ui
{

// Everything a view needs to paint itself: the target buffer, the active
// theme, and the rectangle it owns.
struct RenderContext
{
    const render::Theme& theme;
    render::Screen&      screen;
    Rect                 bounds;
};

// A pane is a rectangular interactive region (shell, file list, etc).
class Pane
{
public:
    explicit Pane(std::wstring id) : id_(std::move(id)) {}
    virtual ~Pane() = default;

    const std::wstring& id() const { return id_; }
    void                setTitle(std::wstring t) { title_ = std::move(t); }
    const std::wstring& title() const { return title_; }

    virtual void draw(RenderContext& rc) = 0;
    // Return true if the key was consumed by the pane.
    virtual bool onKey(const struct KeyEvent& key) { (void)key; return false; }
    // Handle mouse wheel scroll (delta > 0 = up, < 0 = down).
    virtual void onMouseWheel(int delta) { (void)delta; }
    // Handle a mouse click within the pane. Coordinates are relative to the
    // pane's top-left: rowInPane (0=top) and colInPane (0=left). doubleClick
    // is true for a double click. Return true if the click was consumed.
    virtual bool onMouseClick(int rowInPane, int colInPane, bool doubleClick)
    {
        (void)rowInPane; (void)colInPane; (void)doubleClick;
        return false;
    }
    // Periodic refresh hook (metrics, file lists, jobs).
    virtual void refresh() {}

protected:
    std::wstring id_;
    std::wstring title_;
};

// Drawing helpers shared by panels.
namespace draw
{

void clear(RenderContext& rc, render::Role role);

// Draw a title bar / header strip inside the pane bounds.
void header(RenderContext& rc, std::wstring_view title);

// Draw a text line at (row offset from pane top), clipped to bounds.
void text(RenderContext& rc, int row, int col, std::wstring_view t,
          render::Role fg, render::Role bg = render::Role::Background, bool bold = false);

// Draw a tabular row where columns are separated by fixed widths; long cells
// are truncated with "...".
void tableRow(RenderContext& rc, int row, const std::vector<std::wstring>& cells,
              const std::vector<int>& widths, render::Role textRole,
              render::Role bg, bool selected);

// Draw a horizontal bar (e.g. CPU usage) with the given fill fraction.
void bar(RenderContext& rc, int row, int x, int totalW, double fraction,
         render::Role fillRole, render::Role trailRole);

// Draw a filter / search input line at the bottom of the pane.
void inputLine(RenderContext& rc, std::wstring_view label, std::wstring_view value, int cursorPos);

} // namespace draw

// Thread-safe append-only text buffer used for pane scrollback.
class OutputBuffer
{
public:
    OutputBuffer();
    ~OutputBuffer();

    void append(const std::wstring& line);
    void appendText(const std::wstring& text); // may contain newlines
    void clear();

    size_t lineCount() const;

    // Read access: caller holds the lock via these helpers.
    void lock();
    void unlock();
    const std::vector<std::wstring>& lines() const { return lines_; }

    void trimTo(size_t maxLines);

private:
    std::vector<std::wstring> lines_;
    struct Impl;
    Impl* impl_;
};

} // namespace kshell::ui
