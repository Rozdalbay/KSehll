#include "ui/Fuzzy.h"

#include <algorithm>
#include <cctype>

namespace kshell::ui
{

namespace
{

int lowerChar(wchar_t c)
{
    return static_cast<int>(towlower(static_cast<wint_t>(c)));
}

} // namespace

FuzzyMatch fuzzyMatch(std::wstring_view needle, std::wstring_view haystack, bool caseSensitive)
{
    FuzzyMatch result;
    if (needle.empty())
    {
        result.matched = true;
        result.score = 1.0f;
        return result;
    }
    if (haystack.empty())
    {
        return result;
    }

    auto eq = [caseSensitive](wchar_t a, wchar_t b) {
        if (caseSensitive)
        {
            return a == b;
        }
        return lowerChar(a) == lowerChar(b);
    };

    size_t n = 0;
    float score = 0.0f;
    size_t lastMatch = static_cast<size_t>(-1);
    size_t gapPenalty = 0;
    bool prevMatched = false;

    for (size_t h = 0; h < haystack.size() && n < needle.size(); ++h)
    {
        if (eq(needle[n], haystack[h]))
        {
            // Consecutive matches are worth more.
            float bonus = 1.0f;
            if (prevMatched)
            {
                bonus += 2.0f;
            }
            else if (lastMatch != static_cast<size_t>(-1))
            {
                gapPenalty += (h - lastMatch - 1);
            }
            // Start-of-word bonus.
            if (h == 0 || haystack[h - 1] == L' ' || haystack[h - 1] == L'/' ||
                haystack[h - 1] == L'\\' || haystack[h - 1] == L'-' || haystack[h - 1] == L'_')
            {
                bonus += 1.0f;
            }
            score += bonus;
            result.indices.push_back(h);
            lastMatch = h;
            prevMatched = true;
            ++n;
        }
        else
        {
            prevMatched = false;
        }
    }

    if (n < needle.size())
    {
        result.matched = false;
        result.indices.clear();
        return result;
    }

    float lengthBoost = static_cast<float>(haystack.size()) / static_cast<float>(haystack.size() + 1);
    result.score = score - static_cast<float>(gapPenalty) * 0.5f + lengthBoost;
    result.matched = true;
    return result;
}

std::vector<FuzzyMatch> fuzzyFilter(std::wstring_view needle,
                                    const std::vector<std::wstring>& haystacks,
                                    bool caseSensitive)
{
    std::vector<FuzzyMatch> out;
    out.reserve(haystacks.size());
    for (const auto& h : haystacks)
    {
        FuzzyMatch m = fuzzyMatch(needle, h, caseSensitive);
        if (m.matched)
        {
            out.push_back(std::move(m));
        }
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const FuzzyMatch& a, const FuzzyMatch& b) { return a.score > b.score; });
    return out;
}

} // namespace kshell::ui
