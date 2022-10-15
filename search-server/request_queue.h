#pragma once

#include <queue>

#include "search_server.h"

class RequestQueue
{
public:

    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query,
                                         DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query,
                                         DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:

    const static size_t min_in_day_ = 1440;

    const SearchServer& search_server_;

    struct QueryResult
    {
        uint64_t id_;
        uint32_t founded_docs_;
    };

    std::deque<QueryResult> requests_;

    uint64_t counter_id_;

    void CheckEndOfDay();

};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query,
                                                   DocumentPredicate document_predicate)
{
    std::vector<Document> result =
            search_server_.FindTopDocuments(raw_query, document_predicate);
    ++counter_id_;
    requests_.push_back({counter_id_, static_cast<uint32_t>(result.size())});
    CheckEndOfDay();
    return result;
}
