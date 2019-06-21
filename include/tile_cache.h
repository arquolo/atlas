#pragma once

#include <list>
#include <map>
#include <string>

namespace gs {

template <typename T>
class TileCache {
    size_t size_ = 0;
    size_t capacity_ = 0;

    using Key = std::string;
    using Tile = std::shared_ptr<std::vector<T>>;

    // `lru_` is used to quickly identify the last used tiles so they
    // can be removed from the cache,
    // it contains an ordered list of used tiles.
    // When a tile is used it is moved to the front of the list
    std::list<Key> lru_ = {};

    // Structure of the cache is as follow: each entry has a position string as the key (x-y-level)
    // Each value contains (tile, iterator to position in _LRU)
    std::map<Key, std::pair<Tile, std::list<Key>::iterator>> cache_;

    // Removes the least recently used (LRU) tile from the cache
    virtual void evict() {
        auto it = cache_.find(lru_.front());
        if (it == cache_.end())
            return;

        auto& [tile, _] = it->second;
        size_ -= tile->size();
        cache_.erase(it);
        lru_.pop_front();
    }

public:
    TileCache(size_t capacity = 0) : capacity_(capacity) {}

    virtual ~TileCache() { cache_.clear(); }

    virtual Tile get(const Key& k) {
        auto it = cache_.find(k);
        if (it == cache_.end())
            return nullptr;

        auto& [tile, iter] = it->second;
        lru_.splice(lru_.end(), lru_, iter);
        return tile;
    }

    virtual bool set(const Key& k, Tile v) {
        if (cache_.find(k) != cache_.end())
            return false;
        if (v->size() > capacity_)
            return false;
        while (size_ + v->size() > capacity_ && size_ != 0)
            evict();

        auto it = lru_.insert(lru_.end(), k);
        cache_[k] = std::make_pair(v, it);
        size_ += v->size();
        return true;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    void resize(size_t size) {
        capacity_ = size;
        while (size_ > capacity_)
            evict();
    }

    virtual void clear() {
        cache_.clear();
        lru_.clear();
        size_ = 0;
    }

};

} // namespace gs
