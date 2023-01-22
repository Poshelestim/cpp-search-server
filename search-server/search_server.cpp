#include "search_server.h"

#include <algorithm>
#include <cmath>

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{

}

void SearchServer::AddDocument(int document_id,
                               std::string_view document,
                               DocumentStatus status,
                               const std::vector<int>& ratings)
{
    using namespace std::literals::string_literals;
    if ((document_id < 0) || (documents_.count(document_id) > 0))
    {
        throw std::invalid_argument("Invalid document_id"s);
    }

    DocumentData doc_data = {ComputeAverageRating(ratings),
                             status,
                             std::string{document},
                             {}};

    documents_.emplace(document_id, std::move(doc_data));

    const std::vector<std::string_view> splited_words =
            SplitIntoWordsNoStop(std::string_view{documents_.at(document_id).text});

    const double inv_word_count = 1.0 / splited_words.size();
    for (const auto &word : splited_words)
    {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    document_ids_.insert(document_id);

    documents_.at(document_id).view = {std::make_move_iterator(splited_words.begin()),
            std::make_move_iterator(splited_words.end())};
}

std::vector<Document>
SearchServer::FindTopDocuments(std::execution::sequenced_policy /*unused*/,
                               std::string_view raw_query,
                               DocumentStatus status) const
{
    return FindTopDocuments(raw_query, status);
}

std::vector<Document>
SearchServer::FindTopDocuments(std::execution::parallel_policy /*unused*/,
                               std::string_view raw_query,
                               DocumentStatus status) const
{
    return FindTopDocuments(std::execution::par,
                            raw_query,
                            [status](int /*unused*/,
                            DocumentStatus document_status,
                            int /*unused*/)
    {
        return document_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
                                                     DocumentStatus status) const
{
    return FindTopDocuments(raw_query,
                            [status](int /*unused*/,
                            DocumentStatus document_status,
                            int /*unused*/)
    {
        return document_status == status;
    });
}

std::vector<Document>
SearchServer::FindTopDocuments(std::execution::sequenced_policy /*unused*/,
                               std::string_view raw_query) const
{
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document>
SearchServer::FindTopDocuments(std::execution::parallel_policy /*unused*/,
                               std::string_view raw_query) const
{
    return FindTopDocuments(std::execution::par, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document>
SearchServer::FindTopDocuments(std::string_view raw_query) const
{
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const
{
    return documents_.size();
}

std::set<int>::iterator SearchServer::begin()
{
    return document_ids_.begin();
}

std::set<int>::iterator SearchServer::end()
{
    return document_ids_.end();
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(std::execution::sequenced_policy /*unused*/,
                            std::string_view raw_query,
                            int document_id) const
{
    using namespace std;

    if (documents_.count(document_id) == 0)
    {
        throw std::out_of_range("there is no such id");
    }

    Query query = ParseQuery(raw_query, true);

    for (const auto& word : query.minus_words)
    {
        if ( auto search = documents_.at(document_id).view.find(word);
             search != documents_.at(document_id).view.end() )
        {
            return {std::vector<std::string_view>{},
                documents_.at(document_id).status};
        }
    }

    std::vector<std::string_view> matched_words;

    for (const auto& word : query.plus_words)
    {
        if ( auto search = documents_.at(document_id).view.find(word);
             search != documents_.at(document_id).view.end() )
        {
            matched_words.emplace_back(*search);
        }
    }

    return {matched_words, documents_.at(document_id).status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(std::execution::parallel_policy /*unused*/,
                            std::string_view raw_query,
                            int document_id) const
{
    using namespace std;

    if (document_to_word_freqs_.count(document_id) == 0U)
    {
        throw out_of_range("there is no such id");
    }

    const Query query = ParseQuery(raw_query);

    bool result = any_of(execution::par,
                         query.minus_words.begin(), query.minus_words.end(),
                         [this, document_id](const auto &word)
    {
        return document_to_word_freqs_.at(document_id).count(word) != 0U;
    });

    if (result)
    {
        return {vector<string_view>{}, documents_.at(document_id).status};
    }

    vector<string_view> matched_words(document_to_word_freqs_.at(document_id).size());

    transform(execution::par,
              document_to_word_freqs_.at(document_id).begin(),
              document_to_word_freqs_.at(document_id).end(),
              matched_words.begin(),
              [query](const auto& item)
    {
        return find(query.plus_words.begin(),
                    query.plus_words.end(),
                    item.first) != query.plus_words.end() ? item.first : "";
    });

    sort(std::execution::par,
         matched_words.begin(), matched_words.end());

    auto start = upper_bound(matched_words.begin(), matched_words.end(), ""s);

    return {{std::make_move_iterator(start),
                    std::make_move_iterator(matched_words.end())},
        documents_.at(document_id).status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(std::string_view raw_query,
                            int document_id) const
{
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

const std::map<std::string_view, double>
&SearchServer::GetWordFrequencies(int document_id) const
{
    static std::map<std::string_view, double> result = {};

    if (document_to_word_freqs_.count(document_id) != 0U)
    {
        result = document_to_word_freqs_.at(document_id);
    }

    return result;
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy policy,
                                  int document_id)
{
    if (documents_.count(document_id) == 0U)
    {
        return;
    }

    const auto &items =
            document_to_word_freqs_[document_id];

    std::for_each(policy, items.begin(), items.end(),
                  [this, document_id](const auto &word)
    {
        word_to_document_freqs_[word.first].erase(document_id);
        if (word_to_document_freqs_[word.first].empty())
        {
            word_to_document_freqs_.erase(word.first);
        }
    });

    documents_.erase(document_id);
    document_ids_.erase(document_id);
    document_to_word_freqs_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy policy,
                                  int document_id)
{
    using namespace std;

    auto it = document_ids_.find(document_id);
    if (it == document_ids_.end())
    {
        return;
    }

    auto &items = document_to_word_freqs_.at(document_id);

    vector<const string_view *> words(items.size());
    transform(policy, items.begin(), items.end(), words.begin(),
              [](const auto &word_doc)
    {
        return &word_doc.first;
    });

    for_each(policy, words.begin(), words.end(),
             [this, document_id](const auto *word)
    {
        word_to_document_freqs_.at(*word).erase(document_id);
        if (word_to_document_freqs_[*word].empty())
        {
            word_to_document_freqs_.erase(*word);
        }
    });

    document_ids_.erase(document_id);
    documents_.erase(document_id);
    document_to_word_freqs_.erase(document_id);
}

void SearchServer::RemoveDocument(int document_id)
{
    RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveWordDuplecates(std::vector<std::string_view> &sourse)
{
    std::sort(sourse.begin(), sourse.end());
    std::vector<std::string_view>::iterator itv(std::unique(sourse.begin(),
                                                            sourse.end()));
    sourse.erase(itv, sourse.end());
}

bool SearchServer::IsStopWord(std::string_view word) const
{
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word)
{
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c)
    {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const
{
    using namespace std::literals::string_literals;
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text))
    {
        if (!IsValidWord(word))
        {
            throw std::invalid_argument("Word "s + word + " is invalid"s);
        }

        if (!IsStopWord(word))
        {
            words.emplace_back(word);
        }
    }
    return words;
}

std::vector<std::string_view>
SearchServer::SplitIntoWordsNoStop(std::string_view text) const
{
    using namespace std::literals::string_literals;
    std::vector<std::string_view> words;
    for (const std::string_view word : SplitIntoWords(text))
    {
        if (!IsValidWord(word))
        {
            throw std::invalid_argument("Word "s +
                                        std::string{word.data()} +
                                        " is invalid"s);
        }

        if (!IsStopWord(word))
        {
            words.emplace_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
    if (ratings.empty())
    {
        return 0;
    }

    return std::accumulate(ratings.begin(), ratings.end(), 0) /
            static_cast<int>(ratings.size());
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const
{
    return log(GetDocumentCount() * 1.0 /
               word_to_document_freqs_.at(word).size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const
{
    using namespace std::literals::string_literals;

    if (text.empty())
    {
        throw std::invalid_argument("Query word is empty"s);
    }

    auto word = text;
    bool is_minus = false;
    if (word[0] == '-')
    {
        is_minus = true;
        word = word.substr(1);
    }

    if (word.empty() || word[0] == '-' || !IsValidWord(word))
    {
        throw std::invalid_argument("Query word "s + std::string{text} + " is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text,
                                             bool need_remove_duplecates) const
{
    using namespace std;

    if (text.empty())
    {
        return Query{};
    }

    Query result;

    for (const auto& word : SplitIntoWords(text))
    {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
            {
                result.minus_words.emplace_back(query_word.data);
                continue;
            }

            result.plus_words.emplace_back(query_word.data);
        }
    }

    if (need_remove_duplecates)
    {
        RemoveWordDuplecates(result.plus_words);
        RemoveWordDuplecates(result.minus_words);
    }

    return result;
}
