#include "test_framework.h"

#include "git/Git.h"
#include "ui/Panels.h"
#include "ui/Ui.h"
#include "ui/Key.h"
#include "render/Screen.h"
#include "render/Theme.h"
#include "utils/StringUtils.h"

#include <windows.h>

#include <fstream>
#include <string>
#include <vector>

using namespace kshell;
using namespace kshell::git;

namespace
{

// Create and return a unique empty temp directory. %TEMP% lives under the
// Administrator home, which itself is a git repo on this machine, so use
// C:\Windows\Temp (guaranteed outside any work tree).
std::wstring makeTempDir()
{
    std::wstring dir = std::wstring(L"C:\\Windows\\Temp\\ksgit_test_") +
                       std::to_wstring(::GetCurrentProcessId()) + L"_" +
                       std::to_wstring(::GetTickCount64());
    for (auto& c : dir)
    {
        if (c == L' ') c = L'_';
    }
    ::CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::string narrow(const std::wstring& s)
{
    int n = ::WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)n, '\0');
    if (n > 0)
    {
        ::WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
    }
    return out;
}

void writeFile(const std::wstring& path, const std::wstring& content)
{
    std::ofstream out(narrow(path), std::ios::out | std::ios::trunc | std::ios::binary);
    out << narrow(content);
    out.close();
}

std::wstring readFile(const std::wstring& path)
{
    std::ifstream in(narrow(path));
    std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return std::wstring(all.begin(), all.end());
}

// Run a git command via the engine itself so we also exercise runSimple().
bool setup(const std::wstring& dir, const std::wstring& argsLine)
{
    Git g;
    GitResult r = g.runSimple(dir, argsLine);
    return r.ok;
}

// A fresh repo with two committed files and the initial commit "first".
std::wstring createRepo()
{
    std::wstring dir = makeTempDir();
    writeFile(dir + L"\\a.txt", L"line1\nline2\n");
    writeFile(dir + L"\\d.txt", L"data\n");
    setup(dir, L"init -b main");
    setup(dir, L"config user.name t");
    setup(dir, L"config user.email t@t");
    setup(dir, L"config core.autocrlf false");
    setup(dir, L"add -A");
    setup(dir, L"commit -m \"first\"");
    return dir;
}

std::wstring lower(std::wstring s)
{
    for (auto& c : s)
    {
        if (c == L'/') c = L'\\';
    }
    return stringutils::toLower(s);
}

bool testStatusInitial()
{
    std::wstring dir = makeTempDir();
    Git g;
    GitStatus st = g.status(dir);
    TEST_ASSERT(!(st.isRepo), L"Plain dir should not be a repo");
    TEST_ASSERT_EQ(g.findRepoRoot(dir), std::wstring(L""), L"No repo root for plain dir");

    dir = createRepo();
    st = g.status(dir);
    TEST_ASSERT(st.isRepo, L"init should create a repo");
    TEST_ASSERT_EQ(lower(g.findRepoRoot(dir)), lower(dir), L"findRepoRoot should match repo dir");
    TEST_ASSERT_EQ(st.branch, std::wstring(L"main"), L"Default branch is main");
    TEST_ASSERT_EQ(st.ahead, 0, L"No upstream yet");
    TEST_ASSERT_EQ(st.lastCommitSubject, std::wstring(L"first"), L"Last commit subject");
    TEST_ASSERT(!(st.detachedHead), L"Not detached on main");
    return true;
}

bool testStatusCounts()
{
    std::wstring dir = createRepo();
    Git g;
    // Untracked file.
    writeFile(dir + L"\\c.txt", L"c\n");
    GitStatus st = g.status(dir);
    TEST_ASSERT_EQ(st.untracked, 1, L"One untracked file expected");
    TEST_ASSERT_EQ(st.staged, 0, L"Nothing staged yet");
    TEST_ASSERT_EQ(st.modified, 0, L"Nothing modified yet");

    // Stage c.txt (add) and modify a.txt in the work tree.
    setup(dir, L"add c.txt");
    writeFile(dir + L"\\a.txt", L"line1\nline2\nmodified\n");
    st = g.status(dir);
    TEST_ASSERT_EQ(st.staged, 1, L"c.txt staged as added");
    TEST_ASSERT_EQ(st.modified, 1, L"a.txt modified in work tree");
    TEST_ASSERT_EQ((int)st.fileEntries.size(), 2, L"Two file entries");

    for (const auto& e : st.fileEntries)
    {
        if (e.path == L"c.txt")
        {
            TEST_ASSERT_EQ(e.indexStatus, L'A', L"c.txt staged add");
            TEST_ASSERT(e.staged, L"c.txt flagged staged");
        }
        else if (e.path == L"a.txt")
        {
            TEST_ASSERT_EQ(e.indexStatus, L' ', L"a.txt not staged");
            TEST_ASSERT_EQ(e.workTreeStatus, L'M', L"a.txt modified");
        }
    }
    return true;
}

bool testDiffWorkingAndCached()
{
    std::wstring dir = createRepo();
    Git g;
    writeFile(dir + L"\\d.txt", L"data\nmore\n");
    auto wd = g.diff(dir, L"d.txt", false);
    TEST_ASSERT(wd.size() >= 2, L"Working-tree diff should have lines");
    bool hasPlus = false;
    for (const auto& dl : wd)
    {
        if (dl.type == L'+' && dl.text.find(L"more") != std::wstring::npos) hasPlus = true;
    }
    TEST_ASSERT(hasPlus, L"Diff should contain added 'more' line");

    // Untouched file has no working-tree diff.
    writeFile(dir + L"\\a.txt", L"line1\nline2\n");
    auto none = g.diff(dir, L"a.txt", false);
    TEST_ASSERT_EQ(none.size(), size_t(0), L"No diff for untouched file");

    // Staged diff for an added file.
    writeFile(dir + L"\\new.txt", L"n1\nn2\n");
    setup(dir, L"add new.txt");
    auto cached = g.diff(dir, L"new.txt", true);
    TEST_ASSERT(cached.size() >= 2, L"Cached diff should have lines");
    return true;
}

bool testLogAndGraph()
{
    std::wstring dir = createRepo();
    writeFile(dir + L"\\a.txt", L"line1\nline2\nmodified\n");
    setup(dir, L"add a.txt");
    setup(dir, L"commit -m \"second commit\"");
    Git g;
    auto commits = g.log(dir, 10);
    TEST_ASSERT_EQ(commits.size(), size_t(2), L"Two commits expected");
    TEST_ASSERT_EQ(commits[0].subject, std::wstring(L"second commit"), L"Newest first");
    TEST_ASSERT(commits[0].isHead, L"HEAD commit flagged");
    TEST_ASSERT_EQ(commits[1].subject, std::wstring(L"first"), L"Older commit");

    std::wstring graph = g.graphText(dir, 20);
    TEST_ASSERT(graph.find(L"second commit") != std::wstring::npos, L"Graph shows second");
    TEST_ASSERT(graph.find(L"first") != std::wstring::npos, L"Graph shows first");

    GitResult sh = g.show(dir, L"HEAD");
    TEST_ASSERT(sh.ok, L"git show HEAD should succeed");
    TEST_ASSERT(sh.stdoutText.find(L"second commit") != std::wstring::npos, L"Show contains subject");
    TEST_ASSERT(g.showText(dir, L"HEAD").find(L"modified") != std::wstring::npos, L"Show diff contains change");
    return true;
}

bool testBranches()
{
    std::wstring dir = createRepo();
    setup(dir, L"branch feature");
    Git g;
    auto locals = g.localBranches(dir);
    TEST_ASSERT_EQ(locals.size(), size_t(2), L"Two local branches");
    bool sawMain = false, sawFeature = false, mainIsCurrent = false;
    for (const auto& b : locals)
    {
        if (b.name == L"main") { sawMain = true; mainIsCurrent = b.isCurrent; }
        if (b.name == L"feature") sawFeature = true;
    }
    TEST_ASSERT(sawMain && sawFeature, L"Both branches present");
    TEST_ASSERT(mainIsCurrent, L"main is current");

    auto all = g.allBranches(dir);
    TEST_ASSERT(all.size() >= 2, L"allBranches includes local branches");
    return true;
}

bool testStash()
{
    std::wstring dir = createRepo();
    writeFile(dir + L"\\a.txt", L"line1\nline2\nstashed\n");
    TEST_ASSERT(setup(dir, L"add a.txt"), L"Stage for stash");
    TEST_ASSERT(setup(dir, L"stash push -m \"wip stash\""), L"Stash push");
    Git g;
    auto stashes = g.stashList(dir);
    TEST_ASSERT_EQ(stashes.size(), size_t(1), L"One stash entry");
    TEST_ASSERT(stashes[0].message.find(L"wip stash") != std::wstring::npos, L"Stash message");

    GitStatus st = g.status(dir);
    TEST_ASSERT_EQ(st.staged + st.modified, 0, L"Clean after stash");
    TEST_ASSERT(setup(dir, L"stash pop"), L"Stash pop");
    TEST_ASSERT(readFile(dir + L"\\a.txt").find(L"stashed") != std::wstring::npos, L"Change restored");
    return true;
}

bool testDetachedHead()
{
    std::wstring dir = createRepo();
    setup(dir, L"branch feature");
    Git g;
    GitResult rev = g.runSimple(dir, L"rev-parse --short HEAD");
    std::wstring shortHash = stringutils::trim(rev.stdoutText);
    TEST_ASSERT(!(shortHash.empty()), L"Have a short hash");

    TEST_ASSERT(setup(dir, L"checkout " + shortHash), L"Checkout hash detaches");
    TEST_ASSERT(g.isDetachedHead(dir), L"HEAD detached after checkout");
    TEST_ASSERT_EQ(g.currentBranch(dir), std::wstring(L""), L"No branch name when detached");

    TEST_ASSERT(setup(dir, L"checkout main"), L"Return to main");
    TEST_ASSERT(!(g.isDetachedHead(dir)), L"Back on a branch");
    TEST_ASSERT_EQ(g.currentBranch(dir), std::wstring(L"main"), L"main again");
    return true;
}

bool testRemoteOps()
{
    std::wstring dir = createRepo();
    GitStatus st;
    Git g;
    auto remotes0 = g.remotes(dir);
    TEST_ASSERT_EQ(remotes0.size(), size_t(0), L"No remotes initially");
    TEST_ASSERT_EQ(st.remoteUrl, std::wstring(L""), L"No remote url in status");

    TEST_ASSERT(setup(dir, L"remote add origin https://github.com/acme/repo.git"), L"Add remote");
    auto remotes = g.remotes(dir);
    TEST_ASSERT_EQ(remotes.size(), size_t(1), L"One remote");
    TEST_ASSERT_EQ(remotes[0].name, std::wstring(L"origin"), L"Remote name");
    TEST_ASSERT(remotes[0].url.find(L"acme/repo.git") != std::wstring::npos, L"Remote url");

    st = g.status(dir);
    TEST_ASSERT_EQ(st.remoteName, std::wstring(L"origin"), L"Status knows remote");
    TEST_ASSERT(st.remoteUrl.find(L"acme") != std::wstring::npos, L"Status remote url");
    return true;
}

bool testParseDiff()
{
    const std::wstring text =
        L"diff --git a/a.txt b/a.txt\n"
        L"index 000..111 100644\n"
        L"--- a/a.txt\n"
        L"+++ b/a.txt\n"
        L"@@ -1,2 +1,3 @@\n"
        L" line1\n"
        L"-gone\n"
        L"+added\n"
        L" line2\n";
auto lines = Git::parseDiff(text);
    TEST_ASSERT(lines.size() >= 7, L"Expected several diff lines");
    int hunk = 0;
    bool hasAdded = false, hasRemoved = false;
    for (const auto& dl : lines)
    {
        if (dl.type == L'@')
        {
            ++hunk;
            TEST_ASSERT_EQ(dl.oldNo, -1, L"Hunk lines carry no line numbers");
        }
        else if (dl.type == L'-')
        {
            if (dl.text == L"gone") hasRemoved = true;
        }
        else if (dl.type == L'+')
        {
            if (dl.text == L"added") hasAdded = true;
        }
    }
    TEST_ASSERT_EQ(hunk, 1, L"One hunk header");
    TEST_ASSERT(hasAdded, L"Added line parsed");
    TEST_ASSERT(hasRemoved, L"Removed line parsed");
    return true;
}

// ---------------------------------------------------------------------------
// Headless GitPane tests. Drive the real pane with a fake screen buffer and
// synthesized key events against a temp repo, waiting for async ops to settle.
// ---------------------------------------------------------------------------

kshell::ui::KeyEvent keyPress(kshell::ui::Key k)
{
    return kshell::ui::KeyEvent{k, 0, false, false, false};
}

kshell::ui::KeyEvent keyText(wchar_t ch)
{
    return kshell::ui::KeyEvent{kshell::ui::Key::None, ch, false, false, false};
}

struct PaneHarness
{
    render::Screen        screen;
    render::Theme         theme{L"TestPane"};
    ui::RenderContext     rc{theme, screen, {0, 0, 120, 24}};
    ui::GitPane           pane;

    PaneHarness() { screen.resize(24, 120); }

    // Draw + wait until all background ops have completed.
    void settle()
    {
        for (int i = 0; i < 300; ++i)
        {
            pane.draw(rc);
            if (!pane.operationActive())
            {
                return;
            }
            ::Sleep(15);
        }
    }
};

bool testPaneSections()
{
    PaneHarness h;
    std::wstring dir = createRepo();
    h.pane.setWorkDir(dir);
    h.pane.refresh();
    h.settle();
    TEST_ASSERT(h.pane.isRepo(), L"Pane detects repo");
    TEST_ASSERT_EQ(h.pane.section(), 0, L"Starts on Overview");

    // Right: Overview -> Changes -> Branches -> History -> Graph -> Remotes.
    const int expectedAfterRight[5] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; ++i)
    {
        h.pane.onKey(keyPress(ui::Key::Right));
        h.settle();
        TEST_ASSERT_EQ(h.pane.section(), expectedAfterRight[i], L"Right navigation");
    }
    // Branches/History/Graph/Remotes loaded data from the temp repo.
    TEST_ASSERT(h.pane.branchCount() >= 1, L"Branches loaded");
    TEST_ASSERT(h.pane.commitCount() >= 1, L"History loaded");
    TEST_ASSERT_EQ(h.pane.remoteCount(), 0, L"Remotes empty");
    TEST_ASSERT_EQ(h.pane.section(), 5, L"On Remotes");

    // Left: Remotes -> Graph -> History -> Branches -> Changes -> Overview.
    for (int i = 0; i < 5; ++i)
    {
        h.pane.onKey(keyPress(ui::Key::Left));
        h.settle();
    }
    TEST_ASSERT_EQ(h.pane.section(), 0, L"Back on Overview");
    return true;
}

bool testPaneStageCommit()
{
    PaneHarness h;
    std::wstring dir = createRepo();
    writeFile(dir + L"\\a.txt", L"line1\nline2\nmodified\n");
    h.pane.setWorkDir(dir);
    h.pane.refresh();
    h.settle();
    h.pane.onKey(keyPress(ui::Key::Right)); // -> Changes
    h.settle();
    TEST_ASSERT_EQ(h.pane.section(), 1, L"Changes section");
    TEST_ASSERT_EQ(h.pane.fileCount(), 1, L"One modified file");
    TEST_ASSERT_EQ(h.pane.statusStaged(), 0, L"Nothing staged yet");

    // 's' toggles staging for the selected file.
    h.pane.onKey(keyText(L's'));
    h.settle();
    TEST_ASSERT_EQ(h.pane.statusStaged(), 1, L"File staged");

    // 'u' toggles it back off.
    h.pane.onKey(keyText(L'u'));
    h.settle();
    TEST_ASSERT_EQ(h.pane.statusStaged(), 0, L"File unstaged");

    // Stage again, then commit via the message field.
    h.pane.onKey(keyText(L's'));
    h.settle();
    h.pane.onKey(keyPress(ui::Key::Tab)); // -> CommitMsg
    h.settle();
    for (auto ch : { L'a', L'd', L'd', L' ', L'c', L'h', L'a', L'n', L'g', L'e' })
    {
        h.pane.onKey(keyText(ch));
    }

    h.pane.onKey(keyPress(ui::Key::Enter)); // commit
    h.settle();
    TEST_ASSERT_EQ(h.pane.statusStaged(), 0, L"Clean after commit");

    // Visit History (Changes -> Branches -> History) and confirm the commit landed.
    h.pane.onKey(keyPress(ui::Key::Right));
    h.settle();
    h.pane.onKey(keyPress(ui::Key::Right));
    h.settle();
    TEST_ASSERT_EQ(h.pane.section(), 3, L"History section");
    TEST_ASSERT_EQ(h.pane.commitCount(), 2, L"Two commits after commit");
    return true;
}

bool testPaneCommitDetached()
{
    PaneHarness h;
    std::wstring dir = createRepo();
    writeFile(dir + L"\\a.txt", L"line1\nline2\nsecond\n");
    setup(dir, L"add -A");
    setup(dir, L"commit -m \"second\"");
    setup(dir, L"checkout HEAD~1"); // detached at "first"
    writeFile(dir + L"\\a.txt", L"line1\nline2\ndetached edit\n");
    h.pane.setWorkDir(dir);
    h.pane.refresh();
    h.settle();
    TEST_ASSERT(h.pane.isRepo(), L"Pane sees detached repo");

    // Changes, type a message, Tab back to Files, trigger Commit & Push ('p').
    h.pane.onKey(keyPress(ui::Key::Right)); // -> Changes
    h.settle();
    TEST_ASSERT_EQ(h.pane.fileCount(), 1, L"One modified file on detached HEAD");
    h.pane.onKey(keyPress(ui::Key::Tab)); // -> CommitMsg
    for (auto ch : { L'd', L'e', L't', L'a', L'c', L'h' })
    {
        h.pane.onKey(keyText(ch));
    }
    h.pane.onKey(keyPress(ui::Key::Tab)); // -> CommitDesc
    h.pane.onKey(keyPress(ui::Key::Tab)); // -> Files
    h.pane.onKey(keyText(L'p'));          // commit & push
    h.settle();

    // Commit landed, but a detached HEAD must NOT spin up a push (which would be
    // a silent no-op), and no remote may be created.
    TEST_ASSERT_EQ(h.pane.statusStaged(), 0, L"Commit made on detached HEAD");
    Git g;
    TEST_ASSERT(g.remotes(dir).empty(), L"No remote created for detached push");
    return true;
}

} // namespace

int main()
{
    std::vector<TestCase> tests {
        { L"GitStatusInitial", testStatusInitial },
        { L"GitStatusCounts", testStatusCounts },
        { L"GitDiffWorkingAndCached", testDiffWorkingAndCached },
        { L"GitLogAndGraph", testLogAndGraph },
        { L"GitBranches", testBranches },
        { L"GitStash", testStash },
        { L"GitDetachedHead", testDetachedHead },
        { L"GitRemoteOps", testRemoteOps },
        { L"GitParseDiff", testParseDiff },
        { L"GitPaneSections", testPaneSections },
        { L"GitPaneStageCommit", testPaneStageCommit },
        { L"GitPaneCommitDetached", testPaneCommitDetached },
    };
    return runTests(tests);
}
