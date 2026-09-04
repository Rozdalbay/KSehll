#pragma once

#include <string>
#include <map>

namespace kshell
{

enum class Language
{
    English,
    Russian,
};

// Lightweight gettext-style localization. A single global locale holds the
// current language and a translation table. tr("id") returns the localized
// string (falls back to the id itself). UI code calls tr() on every render so
// switching language takes effect immediately.
class Locale
{
public:
    static Locale& instance();

    void setLanguage(Language lang);
    Language language() const { return lang_; }
    const wchar_t* code() const { return lang_ == Language::Russian ? L"RU" : L"EN"; }

    std::wstring tr(const wchar_t* id) const;

    void toggle() { setLanguage(lang_ == Language::English ? Language::Russian
                                                            : Language::English); }

private:
    Locale();
    const std::map<std::wstring, std::wstring>& table() const;

    Language lang_ = Language::English;
};

inline std::wstring tr(const wchar_t* id)
{
    return Locale::instance().tr(id);
}

} // namespace kshell
