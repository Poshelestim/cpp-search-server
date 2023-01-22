#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) :
    search_server_(search_server),
    counter_id_(0)
{

}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query,
                                                   DocumentStatus status)
{
    return AddFindRequest(raw_query,
                          [status](int  /*unused*/,
                          DocumentStatus document_status,
                          int  /*unused*/)
    {
        return document_status == status;
    });
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query)
{
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const
{
    return count_if(requests_.begin(), requests_.end(),
                    [](const QueryResult &result)
    {
        return result.founded_docs_ == 0;
    });
}

void RequestQueue::CheckEndOfDay()
{
    if (requests_.size() > min_in_day_)
    {
        requests_.pop_front();
    }
}
