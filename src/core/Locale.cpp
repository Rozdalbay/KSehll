#include "core/Locale.h"

namespace kshell
{

namespace
{

const std::map<std::wstring, std::wstring>& russianTable()
{
    static const std::map<std::wstring, std::wstring> table = {
        {L"Tab", L"\u0412\u043a\u043b\u0430\u0434\u043a\u0430"},

        // Sidebar / views.
        {L"Terminal", L"\u0422\u0435\u0440\u043c\u0438\u043d\u0430\u043b"},
        {L"Files", L"\u0424\u0430\u0439\u043b\u044b"},
        {L"Processes", L"\u041f\u0440\u043e\u0446\u0435\u0441\u0441\u044b"},
        {L"System", L"\u0421\u0438\u0441\u0442\u0435\u043c\u0430"},
        {L"Git", L"Git"},
        {L"Jobs", L"\u0417\u0430\u0434\u0430\u0447\u0438"},
        {L"History", L"\u0418\u0441\u0442\u043e\u0440\u0438\u044f"},
        {L"Environment", L"\u041e\u043a\u0440\u0443\u0436\u0435\u043d\u0438\u0435"},
        {L"Variables", L"\u041f\u0435\u0440\u0435\u043c\u0435\u043d\u043d\u044b\u0435"},
        {L"Trace", L"\u0422\u0440\u0430\u0441\u0441\u0438\u0440\u043e\u0432\u043a\u0430"},

        // Panels.
        {L"System Monitor", L"\u041c\u043e\u043d\u0438\u0442\u043e\u0440 \u0441\u0438\u0441\u0442\u0435\u043c\u044b"},
        {L"Command History", L"\u0418\u0441\u0442\u043e\u0440\u0438\u044f \u043a\u043e\u043c\u0430\u043d\u0434"},
        {L"Environment Variables  [F5]refresh  [/]filter",
         L"\u041f\u0435\u0440\u0435\u043c\u0435\u043d\u043d\u044b\u0435 \u043e\u043a\u0440\u0443\u0436\u0435\u043d\u0438\u044f  [F5]\u043e\u0431\u043d\u043e\u0432\u0438\u0442\u044c  [/]\u0444\u0438\u043b\u044c\u0442\u0440"},
        {L"Tracked Variables  [Enter]history  [F5]refresh",
         L"\u041e\u0442\u0441\u043b\u0435\u0436\u0438\u0432\u0430\u0435\u043c\u044b\u0435 \u043f\u0435\u0440\u0435\u043c\u0435\u043d\u043d\u044b\u0435  [Enter]\u0438\u0441\u0442\u043e\u0440\u0438\u044f  [F5]\u043e\u0431\u043d\u043e\u0432\u0438\u0442\u044c"},
        {L"Trace  [1]all [2]cmd [3]exec [4]var [5]dir  [F5]refresh",
         L"\u0422\u0440\u0430\u0441\u0441\u0438\u0440\u043e\u0432\u043a\u0430  [1]\u0432\u0441\u0435 [2]\u043a\u043c\u0434 [3]\u0432\u044b\u043f [4]\u043f\u0435\u0440 [5]\u043a\u0430\u0442  [F5]\u043e\u0431\u043d\u043e\u0432\u0438\u0442\u044c"},
        {L"[F5]refresh  CPU/RAM history updates live",
         L"[F5]\u043e\u0431\u043d\u043e\u0432\u0438\u0442\u044c  \u0433\u0438\u0441\u0442\u043e\u0440\u0438\u044f CPU/RAM \u043e\u0431\u043d\u043e\u0432\u043b\u044f\u0435\u0442\u0441\u044f"},
        {L"No environment set", L"\u041e\u043a\u0440\u0443\u0436\u0435\u043d\u0438\u0435 \u043d\u0435 \u0437\u0430\u0434\u0430\u043d\u043e"},
        {L"No tracked variables. Use 'set NAME=value'.",
         L"\u041d\u0435\u0442 \u043e\u0442\u0441\u043b\u0435\u0436\u0438\u0432\u0430\u0435\u043c\u044b\u0445 \u043f\u0435\u0440\u0435\u043c\u0435\u043d\u043d\u044b\u0445. \u0418\u0441\u043f\u043e\u043b\u044c\u0437\u0443\u0439\u0442\u0435 'set NAME=value'."},
        {L"No variable tracker set", L"\u041e\u0442\u0441\u043b\u0435\u0436\u0438\u0432\u0430\u0442\u0435\u043b\u044c \u043d\u0435 \u0437\u0430\u0434\u0430\u043d"},
        {L"No trace log set", L"\u0416\u0443\u0440\u043d\u0430\u043b \u0442\u0440\u0430\u0441\u0441\u0438\u0440\u043e\u0432\u043a\u0438 \u043d\u0435 \u0437\u0430\u0434\u0430\u043d"},
        {L"Total: ", L"\u0418\u0442\u043e\u0433\u043e: "},

        // Process / file footer keys.
        {L"[F]force kill  [Del]kill  [T]tree",
         L"[F]\u043f\u0440\u0438\u043d\u0443\u0434\u0438\u0442\u0435\u043b\u044c\u043d\u043e \u0443\u0431\u0438\u0442\u044c  [Del]\u0443\u0431\u0438\u0442\u044c  [T]\u0434\u0435\u0440\u0435\u0432\u043e"},

        // Welcome / terminal.
        {L"Type \"help\" for available commands.",
         L"\u0412\u0432\u0435\u0434\u0438\u0442\u0435 \"help\" \u0434\u043b\u044f \u0441\u043f\u0438\u0441\u043a\u0430 \u043a\u043e\u043c\u0430\u043d\u0434."},

        // Command palette.
        {L"Command Palette", L"\u041f\u0430\u043b\u0438\u0442\u0440\u0430 \u043a\u043e\u043c\u0430\u043d\u0434"},
        {L"Search History", L"\u041f\u043e\u0438\u0441\u043a \u0438\u0441\u0442\u043e\u0440\u0438\u0438"},
        {L" items", L" \u044d\u043b\u0435\u043c\u0435\u043d\u0442\u043e\u0432"},

        // Builtin / general.
        {L"No file(s) found", L"\u0424\u0430\u0439\u043b\u044b \u043d\u0435 \u043d\u0430\u0439\u0434\u0435\u043d\u044b"},
    };
    return table;
}

} // namespace

Locale& Locale::instance()
{
    static Locale inst;
    return inst;
}

Locale::Locale() = default;

void Locale::setLanguage(Language lang)
{
    lang_ = lang;
}

const std::map<std::wstring, std::wstring>& Locale::table() const
{
    return russianTable();
}

std::wstring Locale::tr(const wchar_t* id) const
{
    if (lang_ == Language::Russian)
    {
        const auto& t = table();
        const auto it = t.find(id);
        if (it != t.end())
        {
            return it->second;
        }
    }
    return id;
}

} // namespace kshell
