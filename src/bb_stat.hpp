#pragma once

#ifdef BB_STAT

#include <chrono>
#include <print>
#include <string_view>

class BbStat {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    std::string_view label_;
    TimePoint start_;

public:
    explicit BbStat(std::string_view label) : label_(label), start_(Clock::now()) {}

    ~BbStat() { report(); }

    inline void report() const {
        auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
        std::println("[bb_stat] {}: {:.3f} ms", label_, elapsed);
    }

    inline void reset() { start_ = Clock::now(); }
};

#else

class BbStat {
public:
    explicit BbStat(auto&&...) {}
    inline void report() const {}
    inline void reset() {}
};

#endif
