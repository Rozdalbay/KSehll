#include "test_framework.h"
#include "history/History.h"

#include <windows.h>

using namespace kshell;

static bool testHistoryAdd()
{
    History history(100);
    history.add(L"echo hello");
    TEST_ASSERT_EQ(history.size(), size_t(1), L"Expected 1 entry");
    TEST_ASSERT_EQ(history.entries()[0], std::wstring(L"echo hello"), L"Entry should be 'echo hello'");
    return true;
}

static bool testHistoryAddDuplicate()
{
    History history(100);
    history.add(L"echo hello");
    history.add(L"echo hello");
    TEST_ASSERT_EQ(history.size(), size_t(1), L"Expected 1 entry (duplicate suppressed)");
    return true;
}

static bool testHistoryAddEmpty()
{
    History history(100);
    history.add(L"");
    history.add(L"  ");
    TEST_ASSERT_EQ(history.size(), size_t(0), L"Expected 0 entries (empty not added)");
    return true;
}

static bool testHistoryMaxSize()
{
    History history(5);
    for (int i = 0; i < 10; ++i)
    {
        history.add(L"cmd" + std::to_wstring(i));
    }
    TEST_ASSERT_EQ(history.size(), size_t(5), L"Expected 5 entries");
    TEST_ASSERT_EQ(history.entries().front(), std::wstring(L"cmd5"), L"Oldest should be cmd5");
    TEST_ASSERT_EQ(history.entries().back(), std::wstring(L"cmd9"), L"Newest should be cmd9");
    return true;
}

static bool testHistoryClear()
{
    History history(100);
    history.add(L"echo a");
    history.add(L"echo b");
    history.clear();
    TEST_ASSERT_EQ(history.size(), size_t(0), L"Expected 0 entries after clear");
    TEST_ASSERT(history.empty(), L"Expected empty after clear");
    return true;
}

static bool testHistorySaveLoad()
{
    const std::wstring filePath = L"kshell_test_history.txt";
    {
        History history(100);
        history.add(L"echo hello");
        history.add(L"dir");
        history.add(L"cd ..");
        bool ok = history.saveToFile(filePath);
        TEST_ASSERT(ok, L"Save should succeed");
    }

    {
        History history(100);
        bool ok = history.loadFromFile(filePath);
        TEST_ASSERT(ok, L"Load should succeed");
        TEST_ASSERT_EQ(history.size(), size_t(3), L"Expected 3 entries");
        TEST_ASSERT_EQ(history.entries()[0], std::wstring(L"echo hello"), L"First entry");
        TEST_ASSERT_EQ(history.entries()[2], std::wstring(L"cd .."), L"Last entry");
    }

    ::DeleteFileW(filePath.c_str());
    return true;
}

static bool testHistorySetMaxSize()
{
    History history(10);
    for (int i = 0; i < 15; ++i)
    {
        history.add(L"cmd" + std::to_wstring(i));
    }
    TEST_ASSERT_EQ(history.size(), size_t(10), L"Expected 10 entries");
    history.setMaxSize(5);
    TEST_ASSERT_EQ(history.size(), size_t(5), L"Expected 5 entries after resize");
    return true;
}

int main()
{
    std::vector<TestCase> tests = {
        {L"History.Add", testHistoryAdd},
        {L"History.AddDuplicate", testHistoryAddDuplicate},
        {L"History.AddEmpty", testHistoryAddEmpty},
        {L"History.MaxSize", testHistoryMaxSize},
        {L"History.Clear", testHistoryClear},
        {L"History.SaveLoad", testHistorySaveLoad},
        {L"History.SetMaxSize", testHistorySetMaxSize},
    };
    return runTests(tests);
}