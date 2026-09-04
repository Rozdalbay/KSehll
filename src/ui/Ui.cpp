#include "ui/Ui.h"
#include "ui/Key.h"

#include <mutex>
#include <algorithm>

namespace kshell::ui
{

//----------------------------------------------------------------------------
// Drawing helpers
//----------------------------------------------------------------------------
namespace draw
{

void clear(RenderContext& rc, render::Role role)
{
    const auto& t = rc.theme;
    rc.screen.fillRect(rc.bounds.y, rc.bounds.x, rc.bounds.h, rc.bounds.w,
                       L' ', t.color(role), t.color(role));
}

void header(RenderContext& rc, std::wstring_view title)
{
    const auto& t = rc.theme;
    const render::Color bg = t.color(render::Role::Sidebar);
    const render::Color fg = t.color(render::Role::PanelTitle);
    rc.screen.fillLine(rc.bounds.y, rc.bounds.x, rc.bounds.w, L' ', bg, bg);
    rc.screen.putText(rc.bounds.y, rc.bounds.x + 1, title, fg, bg, true);
}

void text(RenderContext& rc, int row, int col, std::wstring_view t,
          render::Role fg, render::Role bg, bool bold)
{
    if (row < 0 || row >= rc.bounds.h || rc.bounds.h <= 0)
    {
        return;
    }
    const auto& theme = rc.theme;
    rc.screen.putText(rc.bounds.y + row, rc.bounds.x + col, t,
                      theme.color(fg), theme.color(bg), bold);
}

void tableRow(RenderContext& rc, int row, const std::vector<std::wstring>& cells,
              const std::vector<int>& widths, render::Role textRole,
              render::Role bgRole, bool selected)
{
    const auto& t = rc.theme;
    const render::Color bg = selected ? t.color(render::Role::Selection)
                                      : t.color(bgRole);
    rc.screen.fillLine(rc.bounds.y + row, rc.bounds.x, rc.bounds.w, L' ', bg, bg);

    if (row < 0 || row >= rc.bounds.h)
    {
        return;
    }
    int x = 0;
    std::wstring indent;
    for (size_t i = 0; i < cells.size(); ++i)
    {
        int w = (i < widths.size()) ? widths[i] : 12;
        std::wstring cell = i < cells.size() ? cells[i] : L"";
        if ((int)cell.size() > w)
        {
            if (w > 1)
            {
                cell = cell.substr(0, (size_t)(w - 1)) + L"\u2026";
            }
            else
            {
                cell = L"\u2026";
            }
        }
        else if ((int)cell.size() < w)
        {
            cell.append((size_t)(w - (int)cell.size()), L' ');
        }
        rc.screen.putText(rc.bounds.y + row, rc.bounds.x + x, cell,
                          t.color(textRole), bg, selected);
        x += w;
    }
}

void bar(RenderContext& rc, int row, int x, int totalW, double fraction,
         render::Role fillRole, render::Role /*trailRole*/)
{
    const auto& t = rc.theme;
    if (fraction < 0.0)
    {
        fraction = 0.0;
    }
    if (fraction > 1.0)
    {
        fraction = 1.0;
    }
    int filled = static_cast<int>(fraction * totalW);
    if (filled < 0)
    {
        filled = 0;
    }
    if (filled > totalW)
    {
        filled = totalW;
    }
    const render::Color fg = t.color(fillRole);
    for (int i = 0; i < totalW; ++i)
    {
        if (i < filled)
        {
            rc.screen.put(rc.bounds.y + row, rc.bounds.x + x + i, L'\u2588', fg, t.color(render::Role::Background));
        }
        else
        {
            rc.screen.put(rc.bounds.y + row, rc.bounds.x + x + i, L'\u2591', t.color(render::Role::Muted), t.color(render::Role::Background));
        }
    }
}

void inputLine(RenderContext& rc, std::wstring_view label, std::wstring_view value, int cursorPos)
{
    const auto& t = rc.theme;
    const render::Color barBg = t.color(render::Role::CommandBar);
    const render::Color barFg = t.color(render::Role::CommandBarText);
    rc.screen.fillLine(rc.bounds.y + rc.bounds.h - 1, rc.bounds.x, rc.bounds.w, L' ', barBg, barBg);
    rc.screen.putText(rc.bounds.y + rc.bounds.h - 1, rc.bounds.x + 1, label,
                      t.color(render::Role::Accent), barBg);
    rc.screen.putText(rc.bounds.y + rc.bounds.h - 1, rc.bounds.x + 1 + label.size(), value,
                      barFg, barBg);
    if (cursorPos >= 0)
    {
        int cx = rc.bounds.x + 1 + (int)label.size() + cursorPos;
        if (cx < rc.bounds.x + rc.bounds.w)
        {
            rc.screen.put(rc.bounds.y + rc.bounds.h - 1, cx, L'\u258e', t.color(render::Role::Accent), barBg);
        }
    }
}

} // namespace draw

//----------------------------------------------------------------------------
// OutputBuffer
//----------------------------------------------------------------------------
struct OutputBuffer::Impl
{
    std::mutex mtx;
};

OutputBuffer::OutputBuffer() : impl_(new Impl()) {}
OutputBuffer::~OutputBuffer() { delete impl_; }

void OutputBuffer::append(const std::wstring& line)
{
    std::lock_guard<std::mutex> lk(impl_->mtx);
    lines_.push_back(line);
}

void OutputBuffer::appendText(const std::wstring& text)
{
    std::lock_guard<std::mutex> lk(impl_->mtx);
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t nl = text.find(L'\n', pos);
        if (nl == std::wstring::npos)
        {
            nl = text.size();
        }
        std::wstring line = text.substr(pos, nl - pos);
        if (!line.empty() && line.back() == L'\r')
        {
            line.pop_back();
        }
        lines_.push_back(line);
        pos = nl + 1;
    }
}

void OutputBuffer::clear()
{
    std::lock_guard<std::mutex> lk(impl_->mtx);
    lines_.clear();
}

size_t OutputBuffer::lineCount() const
{
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return lines_.size();
}

void OutputBuffer::lock()
{
    impl_->mtx.lock();
}

void OutputBuffer::unlock()
{
    impl_->mtx.unlock();
}

void OutputBuffer::trimTo(size_t maxLines)
{
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (lines_.size() > maxLines)
    {
        lines_.erase(lines_.begin(), lines_.begin() + (lines_.size() - maxLines));
    }
}

} // namespace kshell::ui
