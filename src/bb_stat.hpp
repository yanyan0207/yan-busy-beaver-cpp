#pragma once

#ifdef BB_STAT

#include <chrono>
#include <format>
#include <limits>
#include <string>
#include <string_view>

struct Stat {
    Stat(bool enabled = true) : enabled(enabled) {}
    bool enabled;
    size_t count = 0;
    double total_ms = 0.0;
    double min_ms = std::numeric_limits<double>::max();
    double max_ms = 0.0;

    inline void start() {
        if (enabled)
            t_start_ = std::chrono::steady_clock::now();
    }

    inline void stop() {
        if (!enabled)
            return;
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_start_)
                .count();
        ++count;
        total_ms += ms;
        if (ms < min_ms)
            min_ms = ms;
        if (ms > max_ms)
            max_ms = ms;
    }

    [[nodiscard]] inline double last_ms() const { return enabled ? last_ms_ : 0.0; }

    void add(const Stat& other) {
        count += other.count;
        total_ms += other.total_ms;
        if (other.count > 0) {
            if (other.min_ms < min_ms)
                min_ms = other.min_ms;
            if (other.max_ms > max_ms)
                max_ms = other.max_ms;
        }
    }

    [[nodiscard]] std::string format(std::string_view label) const {
        if (count == 0)
            return std::format("{}: (no data)", label);
        return std::format("{}: count={}, total={:.6f} sec, min={:.3f} usec, max={:.3f} usec",
                           label, count, total_ms / 1000.0, min_ms * 1000.0, max_ms * 1000.0);
    }

private:
    std::chrono::steady_clock::time_point t_start_;
    double last_ms_ = 0.0;
};

using CheckerStat = Stat;

#else

struct Stat {
    Stat(bool enabled = true) : enabled(enabled) {}
    bool enabled;
    inline void start() const {}
    inline void stop() const {}
    inline void add(const Stat&) const {}
    [[nodiscard]] inline double last_ms() const { return 0.0; }
};

using CheckerStat = Stat;

#endif
