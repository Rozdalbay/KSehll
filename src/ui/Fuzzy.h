#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace kshell::ui
{

struct FuzzyMatch
{
    float         score = 0.0f;
    bool          matched = false;
    // Sorted ascending indices (into the haystack) of matched characters.
    std::vector<size_t> indices;
};

// Fuzzy subsequence matcher used by the command palette and history search.
// Missing or consecutive characters improve the score; case-insensitive by
// default. Pure function, no I/O, easily unit tested.
FuzzyMatch fuzzyMatch(std::wstring_view needle, std::wstring_view haystack,
                      bool caseSensitive = false);

// Returns matches sorted best-first (highest score).
std::vector<FuzzyMatch> fuzzyFilter(std::wstring_view needle,
                                    const std::vector<std::wstring>& haystacks,
                                    bool caseSensitive = false);

} // namespace kshell::ui
