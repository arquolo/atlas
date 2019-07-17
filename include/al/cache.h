#pragma once

#include <functional>
#include <future>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <tuple>
#include <type_traits>

namespace traits {

template <typename Ret, typename Cls, typename... Args>
struct _lambda_types {
    using result = Ret;
    using args = std::tuple<Args...>;
};

template <typename Ld>
struct lambda : lambda<decltype(&Ld::operator())> {};

template <typename Ret, typename Cls, typename... Args>
struct lambda<Ret(Cls::*)(Args...)> : _lambda_types<Ret, Cls, Args...> {};

template <typename Ret, typename Cls, typename... Args>
struct lambda<Ret(Cls::*)(Args...) const> : _lambda_types<Ret, Cls, Args...> {};

} // namespace traits

namespace al {

template <typename Fn>
class Cache {
    using Ret = typename traits::lambda<Fn>::result;
    using Key = typename traits::lambda<Fn>::args;
    using Value = std::shared_ptr<Ret>;

    using Future = std::shared_future<Value>;

    Fn fn_ = {};
    std::mutex mutex_ = {};

    size_t size_ = 0;
    size_t capacity_ = 0;

    std::list<Key> lru_;
    std::map<Key, std::pair<Value, typename std::list<Key>::iterator>> map_;
    std::map<Key, Future> futures_;

    size_t size_hint(Value const& value) noexcept {
        if constexpr (std::is_class_v<Ret>)
            if constexpr(std::is_member_function_pointer_v<decltype(&Ret::size)>)
                return value->size();
            else
                return sizeof(Value);
        else
            return sizeof(Value);
    }

    std::optional<Value> get(Key const& key) {
        auto search = this->map_.find(key);
        if (search == this->map_.end())
            return {};

        auto& [value, it] = search->second;
        this->lru_.splice(this->lru_.end(), this->lru_, it);
        return value;
    }

    void put(Key const& key, Value const& value) {
        auto size = this->size_hint(value);

        if (size > this->capacity_)
            return;
        while (this->size_ + size > this->capacity_) {
            auto nh = this->map_.extract(this->lru_.front());
            this->size_ -= this->size_hint(nh.mapped().first);
            this->lru_.pop_front();
        }

        auto pos = this->lru_.insert(this->lru_.end(), key);
        this->map_[key] = std::make_pair(value, pos);
        this->size_ += size;
    }

    Future get_future(Key const& key) {
        std::unique_lock lk{this->mutex_};
        auto opt = this->get(key);
        if (opt)
            return std::async(std::launch::deferred, [opt](){ return *opt; });

        if (auto run = this->futures_.find(key); run != this->futures_.end())
            return run->second;

        Future pending = std::async(std::launch::deferred, [&key, this](){
            auto value = std::make_shared<Ret>(std::apply(this->fn_, key));
            {
                std::unique_lock lk_{this->mutex_};
                this->put(key, value);
                this->futures_.erase(key);
            }
            return value;
        });
        this->futures_[key] = pending;
        return pending;
    }

public:
    Cache() = default;
    Cache(Fn&& fn, size_t capacity)
      : fn_{fn}
      , capacity_{capacity}
    {}

    template <typename... Args>
    Value operator()(Args... args) {
        return this->get_future({args...}).get();
    }
};

} // namespace al
