#pragma once
#include <algorithm>
#include <execution>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "concurrent_map.h"
#include "document.h"
#include "string_processing.h"

class SearchServer {
public:

    explicit SearchServer(const std::string& stop_words_text);

    template <typename stringContainer>
    explicit SearchServer(const stringContainer& stop_words);

    void AddDocument(int document_id,
                     const std::string_view document,
                     DocumentStatus status,
                     const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy,
                                           std::string_view raw_query,
                                           DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy,
                                           std::string_view raw_query,
                                           DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query,
                                           DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy,
                                           std::string_view raw_query,
                                           DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::execution::parallel_policy,
                                           std::string_view raw_query,
                                           DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query,
                                           DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy,
                                           std::string_view raw_query) const;

    std::vector<Document> FindTopDocuments(std::execution::parallel_policy,
                                           std::string_view raw_query) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    int GetDocumentCount() const;

    std::set<int>::iterator begin();

    std::set<int>::iterator end();

    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(std::execution::sequenced_policy,
                  std::string_view raw_query,
                  int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(std::execution::parallel_policy,
                  std::string_view raw_query,
                  int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(std::string_view raw_query,
                  int document_id) const;

    const std::map<std::string_view, double> &GetWordFrequencies(int document_id) const;

    void RemoveDocument(const std::execution::sequenced_policy policy,
                        int document_id);

    void RemoveDocument(const std::execution::parallel_policy policy,
                        int document_id);

    void RemoveDocument(int document_id);

    static void RemoveWordDuplecates(std::vector<std::string_view> &sourse);

private:
    const double EPSILON = 1e-6;
    const size_t MAX_RESULT_DOCUMENT_COUNT = 5;

    struct DocumentData
    {
        int rating;
        DocumentStatus status;
        std::string text;
        std::set<std::string_view> view;
    };

    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    struct QueryWord
    {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    struct Query
    {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

private:
    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    QueryWord ParseQueryWord(std::string_view text) const;

    Query ParseQuery(std::string_view text,
                     bool need_remove_duplecates = false) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy /*unused*/,
                                           const Query& query,
                                           DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy /*unused*/,
                                           const Query& query,
                                           DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query /*unused*/,
                                           DocumentPredicate document_predicate) const;
};

template <typename stringContainer>
SearchServer::SearchServer(const stringContainer &stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    using namespace std::literals::string_literals;
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord))
    {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template<typename DocumentPredicate>
std::vector<Document>
SearchServer::FindTopDocuments(std::execution::sequenced_policy /*unused*/,
                               std::string_view raw_query,
                               DocumentPredicate document_predicate) const
{
    return FindTopDocuments(raw_query, document_predicate);
}

template<typename DocumentPredicate>
std::vector<Document>
SearchServer::FindTopDocuments(std::execution::parallel_policy /*unused*/,
                               std::string_view raw_query,
                               DocumentPredicate document_predicate) const
{
    const auto query = ParseQuery(raw_query, true);

    auto matched_documents = FindAllDocuments(std::execution::par,
                                              query,
                                              document_predicate);

    sort(std::execution::par, matched_documents.begin(), matched_documents.end(),
         [this](const Document& lhs, const Document& rhs)
    {
        if (std::abs(lhs.relevance - rhs.relevance) < SearchServer::EPSILON)
        {
            return lhs.rating > rhs.rating;
        }
        return lhs.relevance > rhs.relevance;
    });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
    {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document>
SearchServer::FindTopDocuments(std::string_view raw_query,
                               DocumentPredicate document_predicate) const
{
    const auto query = ParseQuery(raw_query, true);

    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
         [this](const Document& lhs, const Document& rhs)
    {
        if (std::abs(lhs.relevance - rhs.relevance) < SearchServer::EPSILON)
        {
            return lhs.rating > rhs.rating;
        }
        return lhs.relevance > rhs.relevance;
    });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
    {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template<typename DocumentPredicate>
std::vector<Document>
SearchServer::FindAllDocuments(std::execution::sequenced_policy /*unused*/,
                               const Query &query,
                               DocumentPredicate document_predicate) const
{
    return FindAllDocuments(query, document_predicate);
}

template<typename DocumentPredicate>
std::vector<Document>
SearchServer::FindAllDocuments(std::execution::parallel_policy /*unused*/,
                               const Query &query,
                               DocumentPredicate document_predicate) const
{
    const size_t COUNT_BUCKETS = 120;
    ConcurrentMap<int, double> map_document_to_relevance(COUNT_BUCKETS);

    std::for_each(std::execution::par,
                  query.plus_words.begin(),
                  query.plus_words.end(),
                  [this, &map_document_to_relevance, &document_predicate](auto &item)
    {
        if (word_to_document_freqs_.count(item) != 0)
        {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(item);
            for (const auto &[document_id, term_freq] : word_to_document_freqs_.at(item))
            {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating))
                {
                    map_document_to_relevance[document_id].ref_to_value +=
                            term_freq * inverse_document_freq;
                }
            }
        }
    });

    auto document_to_relevance = map_document_to_relevance.BuildOrdinaryMap();

    std::for_each(std::execution::par,
                  query.minus_words.begin(),
                  query.minus_words.end(),
                  [this, &document_to_relevance](auto &item)
    {
        if (word_to_document_freqs_.count(item) != 0)
        {
            for (const auto [document_id, _] : word_to_document_freqs_.at(item))
            {
                document_to_relevance.erase(document_id);
            }
        }
    });

    std::vector<Document> matched_documents(document_to_relevance.size());
    for (const auto [document_id, relevance] : document_to_relevance)
    {
        matched_documents.emplace_back(Document{document_id,
                                                relevance,
                                                documents_.at(document_id).rating});
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document>
SearchServer::FindAllDocuments(const Query& query,
                               DocumentPredicate document_predicate) const
{
    std::map<int, double> document_to_relevance;
    for (const auto word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
        {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating))
            {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const auto& word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word))
        {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance)
    {
        matched_documents.emplace_back(Document{document_id,
                                                relevance,
                                                documents_.at(document_id).rating});
    }
    return matched_documents;
}
