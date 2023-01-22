#include "string_processing.h"

std::vector<std::string> SplitIntoWords(const std::string& text)
{
    std::vector<std::string> words;
    std::string word;
    for (const char c : text)
    {
        if (c == ' ')
        {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else
        {
            word += c;
        }
    }
    if (!word.empty())
    {
        words.push_back(word);
    }

    return words;
}

std::vector<std::string_view> SplitIntoWords(std::string_view text)
{
    std::vector<std::string_view> words;

    int64_t pos = text.find_first_not_of(' ');
    const int64_t pos_end = std::string_view::npos;

    while (pos != pos_end)
    {
        int64_t space = text.find(' ', pos);
        words.push_back(space == pos_end ?
                            text.substr(pos) : text.substr(pos, space - pos));
        pos = text.find_first_not_of(' ', space);
    }

    return words;
}
