#pragma once
#include <string>
#include <vector>
#include <set>

std::vector<std::string> SplitIntoWords(const std::string& text);

std::vector<std::string_view> SplitIntoWords(std::string_view text);

template <typename stringContainer>
std::set<std::string, std::less<>>
MakeUniqueNonEmptyStrings(const stringContainer& strings)
{
    std::set<std::string, std::less<>> non_empty_strings;
    for (const auto& str : strings)
    {
        if (!str.empty())
        {
            non_empty_strings.insert(std::string{str});
        }
    }
    return non_empty_strings;
}

