#ifndef CONCURRENT_MAP_H
#define CONCURRENT_MAP_H

#include <map>
#include <mutex>
#include <vector>

template <typename Key, typename Value>
class ConcurrentMap
{
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    using Bucket = std::pair<std::mutex, std::map<Key, Value>>;

    struct Access
    {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(const Key &key, Bucket &part) :
            guard(part.first),
            ref_to_value(part.second[key])
        {

        }
    };

    explicit ConcurrentMap(size_t bucket_count) :
        data_(bucket_count)
    {

    }

    ConcurrentMap(const ConcurrentMap &other) = delete;

    ~ConcurrentMap()
    {
        data_.clear();
    }

    Access operator[](const Key& key)
    {
        return {key, data_[static_cast<uint64_t>(key) % data_.size()]};
    }

    std::map<Key, Value> BuildOrdinaryMap()
    {
        std::map<Key, Value> result;

        for (auto &[mutex, map] : data_)
        {
            std::lock_guard lock(mutex);
            result.insert(map.begin(), map.end());
        }

        return result;
    }

private:
    std::vector<Bucket> data_;
};

#endif // CONCURRENT_MAP_H
