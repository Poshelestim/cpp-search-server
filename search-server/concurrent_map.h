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

    explicit ConcurrentMap(size_t bucket_count);

    ConcurrentMap(const ConcurrentMap &other) = delete;

    ~ConcurrentMap();

    Access operator[](const Key& key);

    std::map<Key, Value> BuildOrdinaryMap();

    void Erase(const Key &key);

private:
    std::vector<Bucket> data_;

    uint64_t getBucketId(const Key& key) const;
};

template<typename Key, typename Value>
ConcurrentMap<Key, Value>::ConcurrentMap(size_t bucket_count) :
    data_(bucket_count)
{

}

template<typename Key, typename Value>
ConcurrentMap<Key, Value>::~ConcurrentMap()
{
    data_.clear();
}

template<typename Key, typename Value>
typename ConcurrentMap<Key, Value>::Access
ConcurrentMap<Key, Value>::operator[](const Key &key)
{
    return {key, data_[getBucketId(key)]};
}

template<typename Key, typename Value>
std::map<Key, Value> ConcurrentMap<Key, Value>::BuildOrdinaryMap()
{
    std::map<Key, Value> result;

    for (auto &[mutex, map] : data_)
    {
        std::lock_guard lock(mutex);
        result.insert(map.begin(), map.end());
    }

    return result;
}

template<typename Key, typename Value>
void ConcurrentMap<Key, Value>::Erase(const Key& key)
{
    auto &[mutex, map] = data_[getBucketId(key)];
    std::lock_guard guard(mutex);
    map.erase(key);
}

template<typename Key, typename Value>
uint64_t ConcurrentMap<Key, Value>::getBucketId(const Key &key) const
{
    return static_cast<uint64_t>(key) % data_.size();
}


#endif // CONCURRENT_MAP_H
