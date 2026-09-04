#include "test_framework.h"

#include "ui/Ui.h"
#include "ui/Fuzzy.h"
#include "ui/Geometry.h"
#include "render/Screen.h"
#include "render/Theme.h"
#include "render/ThemeManager.h"

using namespace kshell::ui;
using namespace kshell::render;

static bool fuzzyBasicMatch()
{
    auto m = fuzzyMatch(L"git", L"git status");
    TEST_ASSERT_TRUE(m.matched);
    TEST_ASSERT_TRUE(m.indices.size() == 3);
    TEST_ASSERT_TRUE(m.score > 0);
    return true;
}

static bool fuzzyNoMatch()
{
    auto m = fuzzyMatch(L"xyz", L"git status");
    TEST_ASSERT_FALSE(m.matched);
    TEST_ASSERT_TRUE(m.indices.empty());
    return true;
}

static bool fuzzyEmptyNeedle()
{
    auto m = fuzzyMatch(L"", L"anything");
    TEST_ASSERT_TRUE(m.matched);
    return true;
}

static bool fuzzyCaseInsensitive()
{
    auto m = fuzzyMatch(L"STATUS", L"git status");
    TEST_ASSERT_TRUE(m.matched);
    return true;
}

static bool fuzzyConsecutiveBonus()
{
    auto m1 = fuzzyMatch(L"gp", L"git push");
    auto m2 = fuzzyMatch(L"gp", L"get pip");
    TEST_ASSERT_TRUE(m1.matched);
    TEST_ASSERT_TRUE(m2.matched);
    TEST_ASSERT_TRUE(m1.score > m2.score);
    return true;
}

static bool fuzzyFilterSort()
{
    std::vector<std::wstring> items = {
        L"git status", L"get", L"git push", L"gist", L"foo"
    };
    auto matches = fuzzyFilter(L"gi", items);
    TEST_ASSERT_TRUE(matches.size() >= 3);
    return true;
}

static bool screenResize()
{
    Screen s;
    s.resize(5, 10);
    TEST_ASSERT_EQ(s.rows(), 5, L"rows");
    TEST_ASSERT_EQ(s.cols(), 10, L"cols");
    return true;
}

static bool screenPut()
{
    Screen s;
    s.resize(2, 3);
    s.put(0, 0, L'A', Color::rgb(255, 0, 0), Color::rgb(0, 0, 0));
    const auto& c = s.cellAt(0, 0);
    TEST_ASSERT_EQ(c.ch, L'A', L"ch");
    TEST_ASSERT_TRUE(c.fg == Color::rgb(255, 0, 0));
    return true;
}

static bool screenPutText()
{
    Screen s;
    s.resize(2, 10);
    s.putText(0, 0, L"Hello", Color::rgb(200, 200, 200), Color::rgb(0, 0, 0));
    TEST_ASSERT_EQ(s.cellAt(0, 0).ch, L'H', L"cellH");
    TEST_ASSERT_EQ(s.cellAt(0, 4).ch, L'o', L"cellO");
    return true;
}

static bool screenPresent()
{
    Screen s;
    s.resize(2, 5);
    s.clear(Color::rgb(0, 0, 0), Color::rgb(200, 200, 200));
    std::vector<Cell> back;
    auto dirty = s.present(back);
    TEST_ASSERT_TRUE(dirty.size() == 2);
    TEST_ASSERT_TRUE(back.size() == 10);

    // No change.
    dirty = s.present(back);
    TEST_ASSERT_TRUE(dirty.empty());

    // Change cell.
    s.put(1, 2, L'X', Color::rgb(100, 100, 100), Color::rgb(0, 0, 0));
    dirty = s.present(back);
    TEST_ASSERT_TRUE(dirty.size() == 1);
    TEST_ASSERT_EQ(dirty[0], 1, L"dirty row");
    return true;
}

static bool screenFillRect()
{
    Screen s;
    s.resize(3, 5);
    s.fillRect(0, 0, 3, 5, L'X', Color::rgb(1, 1, 1), Color::rgb(0, 0, 0));
    TEST_ASSERT_EQ(s.cellAt(0, 0).ch, L'X', L"topLeft");
    TEST_ASSERT_EQ(s.cellAt(2, 4).ch, L'X', L"bottomRight");
    return true;
}

static bool screenDrawBorder()
{
    Screen s;
    s.resize(5, 8);
    s.drawBorder(1, 1, 3, 5, Color::rgb(100, 100, 100), Color::rgb(0, 0, 0));
    TEST_ASSERT_EQ(s.cellAt(1, 1).ch, L'\u250c', L"topLeft");
    TEST_ASSERT_EQ(s.cellAt(1, 5).ch, L'\u2510', L"topRight");
    TEST_ASSERT_EQ(s.cellAt(3, 1).ch, L'\u2514', L"botLeft");
    TEST_ASSERT_EQ(s.cellAt(3, 5).ch, L'\u2518', L"botRight");
    TEST_ASSERT_EQ(s.cellAt(2, 1).ch, L'\u2502', L"vert");
    return true;
}

static bool themeCreation()
{
    Theme t(L"TestTheme");
    t.setBackground(Color::rgb(0, 0, 0));
    t.setForeground(Color::rgb(200, 200, 200));
    TEST_ASSERT_EQ(t.name(), L"TestTheme", L"name");
    TEST_ASSERT_TRUE(t.color(Role::Background) == Color::rgb(0, 0, 0));
    TEST_ASSERT_TRUE(t.color(Role::Foreground) == Color::rgb(200, 200, 200));
    return true;
}

static bool themeSerialization()
{
    Theme t(L"RoundTrip");
    t.setBackground(Color::rgb(10, 20, 30));
    t.setForeground(Color::rgb(40, 50, 60));
    t.setAccent(Color::rgb(70, 80, 90));
    std::wstring text = t.toLines();

    Theme t2(L"");
    t2.fromLines(text);
    TEST_ASSERT_EQ(t2.name(), L"RoundTrip", L"rt name");
    TEST_ASSERT_TRUE(t2.color(Role::Background) == Color::rgb(10, 20, 30));
    TEST_ASSERT_TRUE(t2.color(Role::Foreground) == Color::rgb(40, 50, 60));
    TEST_ASSERT_TRUE(t2.color(Role::Accent) == Color::rgb(70, 80, 90));
    return true;
}

static bool themeManagerDefaults()
{
    ThemeManager tm;
    TEST_ASSERT_TRUE(tm.count() >= 4);
    TEST_ASSERT_TRUE(tm.nameAt(0) == L"KShell Dark");
    TEST_ASSERT_TRUE(tm.nameAt(1) == L"KShell Light");
    TEST_ASSERT_TRUE(tm.nameAt(2) == L"High Contrast");
    TEST_ASSERT_TRUE(tm.nameAt(3) == L"Monochrome");
    return true;
}

static bool themeManagerSwitch()
{
    ThemeManager tm;
    tm.setActive(1);
    TEST_ASSERT_TRUE(tm.activeTheme().name() == L"KShell Light");
    tm.setActiveByName(L"Monochrome");
    TEST_ASSERT_TRUE(tm.activeTheme().name() == L"Monochrome");
    return true;
}

static bool rectClamp()
{
    Rect r{-5, -3, 20, 20};
    Rect c = r.clampTo(10, 10);
    TEST_ASSERT_TRUE(c.x >= 0);
    TEST_ASSERT_TRUE(c.y >= 0);
    TEST_ASSERT_TRUE(c.right() <= 10);
    TEST_ASSERT_TRUE(c.bottom() <= 10);
    return true;
}

static bool rectSplit()
{
    Rect r{0, 0, 100, 50};
    Rect left, right;
    r.splitVertical(0.3f, left, right);
    TEST_ASSERT_EQ(left.w, 30, L"left.w");
    TEST_ASSERT_EQ(right.w, 70, L"right.w");
    TEST_ASSERT_EQ(left.x, 0, L"left.x");
    TEST_ASSERT_EQ(right.x, 30, L"right.x");
    return true;
}

static bool outputBuffer()
{
    OutputBuffer buf;
    buf.append(L"hello");
    buf.append(L"world");
    TEST_ASSERT_EQ(buf.lineCount(), 2u, L"count after 2 appends");
    buf.appendText(L"line1\nline2\n");
    TEST_ASSERT_EQ(buf.lineCount(), 4u, L"count after appendText (no trailing empty line)");
    buf.clear();
    TEST_ASSERT_EQ(buf.lineCount(), 0u, L"count after clear");
    return true;
}

int main()
{
    std::vector<TestCase> tests = {
        {L"Fuzzy: basic match", fuzzyBasicMatch},
        {L"Fuzzy: no match", fuzzyNoMatch},
        {L"Fuzzy: empty needle", fuzzyEmptyNeedle},
        {L"Fuzzy: case insensitive", fuzzyCaseInsensitive},
        {L"Fuzzy: consecutive bonus", fuzzyConsecutiveBonus},
        {L"Fuzzy: filter sort", fuzzyFilterSort},
        {L"Screen: resize", screenResize},
        {L"Screen: put cell", screenPut},
        {L"Screen: putText", screenPutText},
        {L"Screen: present diff", screenPresent},
        {L"Screen: fillRect", screenFillRect},
        {L"Screen: drawBorder", screenDrawBorder},
        {L"Theme: creation", themeCreation},
        {L"Theme: serialization round-trip", themeSerialization},
        {L"ThemeManager: defaults", themeManagerDefaults},
        {L"ThemeManager: switch", themeManagerSwitch},
        {L"Rect: clamp", rectClamp},
        {L"Rect: split", rectSplit},
        {L"OutputBuffer: basic ops", outputBuffer},
    };
    return runTests(tests);
}
