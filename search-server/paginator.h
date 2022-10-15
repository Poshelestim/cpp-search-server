#pragma once
#include <iostream>
#include <vector>

template <typename Iterator>
class IteratorRange
{
public:

    IteratorRange<Iterator>(Iterator _range_begin, Iterator _range_end) :
        range_begin_(_range_begin),
        range_end_(_range_end)
    {

    }

    Iterator begin() const
    {
        return range_begin_;
    }

    Iterator end() const
    {
        return range_end_;
    }

    auto size() const
    {
        return distance(range_begin_, range_end_);
    }

private:

    Iterator range_begin_;
    Iterator range_end_;

};


template <typename Iterator>
std::ostream& operator<<(std::ostream& os, const IteratorRange<Iterator>& page)
{
    for (auto it = page.begin(); it != page.end(); advance(it, 1))
    {
        os << *it;
    }
    return os;
}

template <typename Iterator>
class Paginator
{
public:
    Paginator<Iterator>(Iterator _it_begin, Iterator _it_end, size_t _page_size)
    {
        while (distance(_it_begin, _it_end - 1) >= _page_size)
        {
            pages_.push_back({_it_begin, _it_begin + _page_size});
            advance(_it_begin, _page_size);
        }

        if (distance(_it_begin, _it_end) > 0)
        {
            pages_.push_back({_it_begin, _it_end});
        }
    }

    auto begin() const
    {
        return pages_.begin();
    }

    auto end() const
    {
        return pages_.end();
    }

    size_t size() const
    {
        return pages_.size();
    }

private:

    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size)
{
    return Paginator(begin(c), end(c), page_size);
}
