#include "test_framework.h"

#include "builtin/BuiltinCommand.h"
#include "builtin/BuiltinRegistry.h"
#include "builtin/PSAliasCommands.h"
#include "core/ShellContext.h"
#include "core/VariableTracker.h"
#include "core/TraceLog.h"
#include "core/Locale.h"
#include "utils/StringUtils.h"
#include "utils/PathUtils.h"

using namespace kshell;

static bool testStringUtilsTrim()
{
    TEST_ASSERT_EQ(stringutils::trim(L"  hello  "), std::wstring(L"hello"), L"Trim");
    TEST_ASSERT_EQ(stringutils::trim(L""), std::wstring(L""), L"Trim empty");
    TEST_ASSERT_EQ(stringutils::trim(L"  "), std::wstring(L""), L"Trim spaces");
    return true;
}

static bool testStringUtilsToLower()
{
    TEST_ASSERT_EQ(stringutils::toLower(L"HELLO"), std::wstring(L"hello"), L"toLower");
    TEST_ASSERT_EQ(stringutils::toLower(L"Hello"), std::wstring(L"hello"), L"toLower mixed");
    TEST_ASSERT_EQ(stringutils::toLower(L"123"), std::wstring(L"123"), L"toLower digits");
    return true;
}

static bool testStringUtilsSplit()
{
    auto parts = stringutils::split(L"a,b,c", L',');
    TEST_ASSERT_EQ(parts.size(), size_t(3), L"Expected 3 parts");
    TEST_ASSERT_EQ(parts[0], std::wstring(L"a"), L"Part a");
    TEST_ASSERT_EQ(parts[1], std::wstring(L"b"), L"Part b");
    TEST_ASSERT_EQ(parts[2], std::wstring(L"c"), L"Part c");
    return true;
}

static bool testStringUtilsStartsWith()
{
    TEST_ASSERT_TRUE(stringutils::startsWith(L"hello world", L"hello"));
    TEST_ASSERT_FALSE(stringutils::startsWith(L"hello", L"world"));
    TEST_ASSERT_FALSE(stringutils::startsWith(L"hi", L"hello"));
    return true;
}

static bool testStringUtilsEndsWith()
{
    TEST_ASSERT_TRUE(stringutils::endsWith(L"hello.txt", L".txt"));
    TEST_ASSERT_FALSE(stringutils::endsWith(L"hello.txt", L".csv"));
    TEST_ASSERT_FALSE(stringutils::endsWith(L"hi", L"hello"));
    return true;
}

static bool testStringUtilsEqualsIgnoreCase()
{
    TEST_ASSERT_TRUE(stringutils::equalsIgnoreCase(L"HELLO", L"hello"));
    TEST_ASSERT_TRUE(stringutils::equalsIgnoreCase(L"Hello", L"HELLO"));
    TEST_ASSERT_FALSE(stringutils::equalsIgnoreCase(L"Hello", L"World"));
    return true;
}

static bool testStringUtilsSplitCommandLine()
{
    auto parts = stringutils::splitCommandLine(L"echo hello world");
    TEST_ASSERT_EQ(parts.size(), size_t(3), L"Expected 3 parts");
    TEST_ASSERT_EQ(parts[0], std::wstring(L"echo"), L"echo");
    TEST_ASSERT_EQ(parts[1], std::wstring(L"hello"), L"hello");
    TEST_ASSERT_EQ(parts[2], std::wstring(L"world"), L"world");
    return true;
}

static bool testStringUtilsSplitCommandLineQuoted()
{
    auto parts = stringutils::splitCommandLine(L"echo \"hello world\"");
    TEST_ASSERT_EQ(parts.size(), size_t(2), L"Expected 2 parts");
    TEST_ASSERT_EQ(parts[0], std::wstring(L"echo"), L"echo");
    TEST_ASSERT_EQ(parts[1], std::wstring(L"hello world"), L"hello world");
    return true;
}

static bool testPathUtilsIsAbsolutePath()
{
    TEST_ASSERT_TRUE(pathutils::isAbsolutePath(L"C:\\Windows"));
    TEST_ASSERT_TRUE(pathutils::isAbsolutePath(L"D:\\Projects"));
    TEST_ASSERT_FALSE(pathutils::isAbsolutePath(L"relative\\path"));
    return true;
}

static bool testPathUtilsGetFileName()
{
    TEST_ASSERT_EQ(pathutils::getFileNameFromPath(L"C:\\test\\file.txt"), std::wstring(L"file.txt"), L"getFileName from path");
    TEST_ASSERT_EQ(pathutils::getFileNameFromPath(L"file.txt"), std::wstring(L"file.txt"), L"getFileName plain");
    return true;
}

static bool testPathUtilsGetParent()
{
    TEST_ASSERT_EQ(pathutils::getParentDirectory(L"C:\\test\\file.txt"), std::wstring(L"C:\\test"), L"getParent");
    return true;
}

static bool testPSCmdletLookup()
{
    // The Verb-Noun cmdlet names must resolve to a builtin function handle.
    TEST_ASSERT(builtinLookup(L"Get-ChildItem") != nullptr, L"Get-ChildItem");
    TEST_ASSERT(builtinLookup(L"Set-Location") != nullptr, L"Set-Location");
    TEST_ASSERT(builtinLookup(L"Get-Location") != nullptr, L"Get-Location");
    TEST_ASSERT(builtinLookup(L"Get-Content") != nullptr, L"Get-Content");
    TEST_ASSERT(builtinLookup(L"Get-Process") != nullptr, L"Get-Process");
    TEST_ASSERT(builtinLookup(L"Clear-Host") != nullptr, L"Clear-Host");
    TEST_ASSERT(builtinLookup(L"Copy-Item") != nullptr, L"Copy-Item");
    TEST_ASSERT(builtinLookup(L"Move-Item") != nullptr, L"Move-Item");
    TEST_ASSERT(builtinLookup(L"Remove-Item") != nullptr, L"Remove-Item");
    TEST_ASSERT(builtinLookup(L"New-Item") != nullptr, L"New-Item");
    TEST_ASSERT(builtinLookup(L"Write-Output") != nullptr, L"Write-Output");
    TEST_ASSERT(builtinLookup(L"Get-Help") != nullptr, L"Get-Help");
    TEST_ASSERT(builtinLookup(L"Get-Date") != nullptr, L"Get-Date");
    TEST_ASSERT(builtinLookup(L"Get-Command") != nullptr, L"Get-Command");
    return true;
}

static bool testPSCmdletCaseInsensitive()
{
    // Cmdlets are matched case-insensitively (PowerShell convention).
    TEST_ASSERT(builtinLookup(L"get-childitem") != nullptr, L"lowercase");
    TEST_ASSERT(builtinLookup(L"GET-PROCESS") != nullptr, L"uppercase");
    return true;
}

static bool testPSCmdletMapping()
{
    // Verify cmdlet names are present in the registry with a callable wrapper.
    const auto& reg = builtinRegistry();
    auto regHas = [&](const std::wstring& name) {
        for (const auto& e : reg)
        {
            if (stringutils::equalsIgnoreCase(e.name, name) && e.function)
            {
                return true;
            }
        }
        return false;
    };
    TEST_ASSERT(regHas(L"Get-Location"), L"registry Get-Location");
    TEST_ASSERT(regHas(L"Set-Location"), L"registry Set-Location");
    TEST_ASSERT(regHas(L"Get-ChildItem"), L"registry Get-ChildItem");
    TEST_ASSERT(regHas(L"Get-Process"), L"registry Get-Process");
    TEST_ASSERT(regHas(L"Clear-Host"), L"registry Clear-Host");
    return true;
}

static bool testVariableTrackerBasic()
{
    VariableTracker vt;
    TEST_ASSERT_TRUE(vt.empty());
    vt.recordSet(L"foo", L"bar");
    vt.recordSet(L"baz", L"qux");
    TEST_ASSERT_FALSE(vt.empty());
    auto snap = vt.snapshot();
    TEST_ASSERT_EQ(snap.size(), size_t(2), L"two tracked vars");
    bool foundFoo = false, foundBaz = false;
    for (const auto& v : snap)
    {
        if (v.name == L"foo") { foundFoo = true; TEST_ASSERT_EQ(v.currentValue, std::wstring(L"bar"), L"foo=bar"); }
        if (v.name == L"baz") { foundBaz = true; TEST_ASSERT_EQ(v.currentValue, std::wstring(L"qux"), L"baz=qux"); }
    }
    TEST_ASSERT_TRUE(foundFoo);
    TEST_ASSERT_TRUE(foundBaz);
    return true;
}

static bool testVariableTrackerHistory()
{
    VariableTracker vt;
    vt.recordSet(L"x", L"1");
    vt.recordSet(L"x", L"2");
    auto snap = vt.snapshot();
    TEST_ASSERT_EQ(snap.size(), size_t(1), L"one var after updates");
    TEST_ASSERT_EQ(snap[0].currentValue, std::wstring(L"2"), L"latest value");
    TEST_ASSERT_EQ(snap[0].history.size(), size_t(2), L"two history entries");
    TEST_ASSERT_EQ(snap[0].history[0].value, std::wstring(L"1"), L"first entry");
    TEST_ASSERT_EQ(snap[0].history[1].value, std::wstring(L"2"), L"second entry");
    return true;
}

static bool testVariableTrackerUnset()
{
    VariableTracker vt;
    vt.recordSet(L"foo", L"bar");
    vt.recordUnset(L"foo");
    auto snap = vt.snapshot();
    TEST_ASSERT_TRUE(snap.empty());
    return true;
}

static bool testTraceLogBasic()
{
    TraceLog log;
    TEST_ASSERT_FALSE(log.enabled());
    log.setEnabled(true);
    TEST_ASSERT_TRUE(log.enabled());
    log.record(TraceKind::CommandTrace, L"+ echo hi");
    log.record(TraceKind::Variable, L"set foo=bar");
    log.recordExecution(L"cmd.exe", 1234, 0, 15, L"hello");
    auto events = log.events();
    TEST_ASSERT_EQ(events.size(), size_t(3), L"three events");
    TEST_ASSERT_EQ(events[0].kind, TraceKind::CommandTrace, L"first is command trace");
    TEST_ASSERT_EQ(events[2].kind, TraceKind::Execution, L"third is execution");
    TEST_ASSERT_EQ(events[2].pid, uint32_t(1234), L"pid recorded");
    TEST_ASSERT_EQ(events[2].exitCode, 0, L"exit code recorded");
    TEST_ASSERT_EQ(events[2].durationMs, 15ll, L"duration recorded");
    return true;
}

static bool testTraceLogFiltered()
{
    TraceLog log;
    log.setEnabled(true);
    log.record(TraceKind::CommandTrace, L"+ x");
    log.record(TraceKind::Execution, L"y");
    auto exec = log.eventsOf(TraceKind::Execution);
    TEST_ASSERT_EQ(exec.size(), size_t(1), L"one execution event");
    return true;
}

static bool testTraceLogClear()
{
    TraceLog log;
    log.setEnabled(true);
    log.record(TraceKind::CommandTrace, L"+ x");
    log.clear();
    TEST_ASSERT_EQ(log.count(), size_t(0), L"cleared");
    return true;
}

static bool testLocaleDefaultEnglish()
{
    auto& l = Locale::instance();
    l.setLanguage(Language::English);
    TEST_ASSERT_EQ(l.code(), std::wstring(L"EN"), L"english code");
    TEST_ASSERT_EQ(l.tr(L"Files"), std::wstring(L"Files"), L"english passthrough");
    return true;
}

static bool testLocaleRussian()
{
    auto& l = Locale::instance();
    l.setLanguage(Language::Russian);
    TEST_ASSERT_EQ(l.code(), std::wstring(L"RU"), L"russian code");
    // "Files" (Файлы) should be translated and differ from the English key.
    TEST_ASSERT_TRUE(l.tr(L"Files") != std::wstring(L"Files"));
    TEST_ASSERT_TRUE(!l.tr(L"Files").empty());
    l.setLanguage(Language::English);
    return true;
}

int main()
{
    std::vector<TestCase> tests = {
        {L"StringUtils.Trim", testStringUtilsTrim},
        {L"StringUtils.ToLower", testStringUtilsToLower},
        {L"StringUtils.Split", testStringUtilsSplit},
        {L"StringUtils.StartsWith", testStringUtilsStartsWith},
        {L"StringUtils.EndsWith", testStringUtilsEndsWith},
        {L"StringUtils.EqualsIgnoreCase", testStringUtilsEqualsIgnoreCase},
        {L"StringUtils.SplitCommandLine", testStringUtilsSplitCommandLine},
        {L"StringUtils.SplitCommandLineQuoted", testStringUtilsSplitCommandLineQuoted},
        {L"PathUtils.IsAbsolutePath", testPathUtilsIsAbsolutePath},
        {L"PathUtils.GetFileName", testPathUtilsGetFileName},
        {L"PathUtils.GetParent", testPathUtilsGetParent},
        {L"PSCmdlet.Lookup", testPSCmdletLookup},
        {L"PSCmdlet.CaseInsensitive", testPSCmdletCaseInsensitive},
        {L"PSCmdlet.Mapping", testPSCmdletMapping},
        {L"VariableTracker.Basic", testVariableTrackerBasic},
        {L"VariableTracker.History", testVariableTrackerHistory},
        {L"VariableTracker.Unset", testVariableTrackerUnset},
        {L"TraceLog.Basic", testTraceLogBasic},
        {L"TraceLog.Filtered", testTraceLogFiltered},
        {L"TraceLog.Clear", testTraceLogClear},
        {L"Locale.DefaultEnglish", testLocaleDefaultEnglish},
        {L"Locale.Russian", testLocaleRussian},
    };
    return runTests(tests);
}