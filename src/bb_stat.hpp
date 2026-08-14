#pragma once

#ifdef BB_STAT

#include <chrono>
#include <format>
#include <limits>
#include <string>
#include <string_view>

struct Stat {
    size_t count = 0;
    double total_ms = 0.0;
    double min_ms = std::numeric_limits<double>::max();
    double max_ms = 0.0;

    inline void start() { t_start_ = std::chrono::steady_clock::now(); }

    inline void stop() {
        last_ms_ =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_start_)
                .count();
        ++count;
        total_ms += last_ms_;
        if (last_ms_ < min_ms)
            min_ms = last_ms_;
        if (last_ms_ > max_ms)
            max_ms = last_ms_;
    }

    [[nodiscard]] inline double last_ms() const { return last_ms_; }

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

#else

struct Stat {
    inline void start() const {}
    inline void stop() const {}
    [[nodiscard]] inline double last_ms() const { return 0.0; }
};

#endif
