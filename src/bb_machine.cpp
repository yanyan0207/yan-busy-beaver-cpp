#include "bb_machine.hpp"

#include <cassert>
#include <stdexcept>
// #define BB_DEBUG_PERIODICITY
#ifdef BB_DEBUG_PERIODICITY
#include <iostream>
#define DPLOG(num, msg) \
    std::cout << "[" << name_ << " count=" << last.count << " num=" << (num) << "] " << msg << "\n"
#define DPLOG_TRY(num, msg)                                                                      \
    std::cout << "[" << name_ << " count=" << last.count << " num=" << (num) << "] try: " << msg \
              << "\n"
#define DLOG(msg) std::cout << msg << "\n"
#else
#define DPLOG(num, msg) ((void)0)
#define DPLOG_TRY(num, msg) ((void)0)
#define DLOG(msg) ((void)0)
#endif

const Instruction& BbMachine::move_one_step() {
    count_++;
    const auto& instruction = table_.get_instruction(state_, tape_.read());
    if (instruction.is_halt()) {
        return instruction;  // halt
    }
    tape_.write_and_move(instruction.write, instruction.dir);
    state_ = instruction.next;
    return instruction;
}
Transition BbMachine::run() {
    for (size_t i = count_; i < max_steps_; ++i) {
        const auto& instruction = move_one_step();
        if (instruction.is_halt()) {
            return Transition{
                .state = state_, .value = tape_.read(), .instr = instruction};  // halt
        }
    }
    const auto& instruction = table_.get_instruction(state_, tape_.read());
    return Transition{
        .state = state_, .value = tape_.read(), .instr = instruction};  // max steps exceeded
}

class BbStateHistory {
    struct BBState {
        int64_t count;
        int64_t pos = 0;
        State state = 0;
        Symbol value = Symbol::ZERO;
    };

public:
    explicit BbStateHistory(const char* name = "?") : name_(name) {}

    void add(int64_t count, int64_t pos, State state, Symbol value) {
        history_.push_back(BBState{.count = count, .pos = pos, .state = state, .value = value});
    }
    [[nodiscard]] size_t size() const { return history_.size(); }
    [[nodiscard]] int64_t last_pos() const {
        if (history_.empty())
            return 0;
        return history_.back().pos;
    }

    [[nodiscard]] bool check_periodicity(bool permit_count_diff_positive,
                                         bool permit_pos_diff_positive,
                                         bool permit_pos_negative) const {
        for (size_t num = 1; num <= history_.size() / 3; ++num) {
            if (check_periodicity(num, permit_count_diff_positive, permit_pos_diff_positive,
                                  permit_pos_negative))
                return true;
        }
        return false;
    }

    [[nodiscard]] bool check_periodicity(int num, bool permit_count_diff_positive,
                                         bool permit_pos_diff_positive,
                                         bool permit_pos_negative) const {
        assert(!permit_pos_diff_positive || !permit_pos_negative);
        if (history_.size() < 4 * num)
            return false;

        const auto& last = history_[history_.size() - 1];
        const auto& last1 = history_[history_.size() - 1 - num];
        const auto& last2 = history_[history_.size() - 1 - 2 * num];
        const auto& last3 = history_[history_.size() - 1 - 3 * num];

        int64_t last_diff_count = last.count - last1.count;
        int64_t last1_diff_count = last1.count - last2.count;
        int64_t last2_diff_count = last2.count - last3.count;
        assert(last_diff_count > 0 && last1_diff_count > 0 && last2_diff_count > 0);

        DPLOG_TRY(num, "last=(count="
                           << last.count << ",s=" << last.state << ",pos=" << last.pos
                           << ",v=" << static_cast<int>(last.value) << ")"
                           << " last1=(count=" << last1.count << ",s=" << last1.state
                           << ",pos=" << last1.pos << ",v=" << static_cast<int>(last1.value) << ")"
                           << " last2=(count=" << last2.count << ",s=" << last2.state
                           << ",pos=" << last2.pos << ",v=" << static_cast<int>(last2.value) << ")"
                           << " last3=(count=" << last3.count << ",s=" << last3.state << ",pos="
                           << last3.pos << ",v=" << static_cast<int>(last3.value) << ")");

        // state and value should be the same for the last three transitions
        if (last.state != last1.state || last1.state != last2.state || last2.state != last3.state) {
            DPLOG(num, "state mismatch: " << last.state << " " << last1.state << " " << last2.state
                                          << " " << last3.state);
            return false;
        }
        if (last.value != last1.value || last1.value != last2.value || last2.value != last3.value) {
            DPLOG(num, "value mismatch: " << static_cast<int>(last.value) << " "
                                          << static_cast<int>(last1.value) << " "
                                          << static_cast<int>(last2.value) << " "
                                          << static_cast<int>(last3.value));
            return false;
        }

        // count difference should be consistent for the last three transitions
        if (permit_count_diff_positive) {
            int64_t last_diff2_count = last_diff_count - last1_diff_count;
            int64_t last1_diff2_count = last1_diff_count - last2_diff_count;
            if (last_diff2_count < 0) {
                DPLOG(num, "count diff2 negative: diff2="
                               << last_diff2_count << " diffs=" << last_diff_count << ","
                               << last1_diff_count << "," << last2_diff_count);
                return false;
            }
            if (last_diff2_count != last1_diff2_count &&
                last_diff2_count != last1_diff2_count * 2) {
                DPLOG(num, "count diff2 inconsistent: diff2="
                               << last_diff2_count << " prev_diff2=" << last1_diff2_count
                               << " diffs=" << last_diff_count << "," << last1_diff_count << ","
                               << last2_diff_count);
                return false;
            }
        } else {
            if (last_diff_count != last1_diff_count || last1_diff_count != last2_diff_count) {
                DPLOG(num, "count diff inconsistent: " << last_diff_count << " " << last1_diff_count
                                                       << " " << last2_diff_count);
                return false;
            }
        }

        // head position difference should be consistent for the last three transitions
        int64_t diff_pos = last.pos - last1.pos;
        if (diff_pos != last1.pos - last2.pos || diff_pos != last2.pos - last3.pos) {
            DPLOG(num, "pos diff inconsistent: " << diff_pos << " " << (last1.pos - last2.pos)
                                                 << " " << (last2.pos - last3.pos)
                                                 << " (pos: " << last.pos << "," << last1.pos << ","
                                                 << last2.pos << "," << last3.pos << ")");
            return false;  // the difference in head position should be consistent
        }

        if (permit_pos_diff_positive) {
            if (diff_pos < 0) {
                DPLOG(num, "pos diff negative (expected >=0): diff_pos=" << diff_pos);
                return false;
            }
        } else if (permit_pos_negative) {
            if (diff_pos > 0) {
                DPLOG(num, "pos diff positive (expected <=0): diff_pos=" << diff_pos);
                return false;
            }
        } else {
            if (diff_pos != 0) {
                DPLOG(num, "pos diff non-zero (expected 0): diff_pos=" << diff_pos);
                return false;
            }
        }

        for (int i = 1; i < num; ++i) {
            const auto& last_i = history_[history_.size() - 1 - i];
            const auto& last_i1 = history_[history_.size() - 1 - num - i];
            const auto& last_i2 = history_[history_.size() - 1 - 2 * num - i];
            const auto& last_i3 = history_[history_.size() - 1 - 3 * num - i];

            if (last_i.state != last_i1.state || last_i1.state != last_i2.state ||
                last_i2.state != last_i3.state) {
                DPLOG(num, "i=" << i << " state mismatch: " << last_i.state << " " << last_i1.state
                                << " " << last_i2.state << " " << last_i3.state);
                return false;
            }
            if (last_i.value != last_i1.value || last_i1.value != last_i2.value ||
                last_i2.value != last_i3.value) {
                DPLOG(num, "i=" << i << " value mismatch: " << static_cast<int>(last_i.value) << " "
                                << static_cast<int>(last_i1.value) << " "
                                << static_cast<int>(last_i2.value) << " "
                                << static_cast<int>(last_i3.value));
                return false;
            }

            if (last_i.count - last_i1.count != last_diff_count) {
                DPLOG(num, "i=" << i
                                << " count diff[0] mismatch: got=" << (last_i.count - last_i1.count)
                                << " expected=" << last_diff_count);
                return false;
            }
            if (last_i1.count - last_i2.count != last1_diff_count) {
                DPLOG(num, "i=" << i << " count diff[1] mismatch: got="
                                << (last_i1.count - last_i2.count)
                                << " expected=" << last1_diff_count);
                return false;
            }
            if (last_i2.count - last_i3.count != last2_diff_count) {
                DPLOG(num, "i=" << i << " count diff[2] mismatch: got="
                                << (last_i2.count - last_i3.count)
                                << " expected=" << last2_diff_count);
                return false;
            }

            if (last_i.pos - last_i1.pos != diff_pos || last_i1.pos - last_i2.pos != diff_pos ||
                last_i2.pos - last_i3.pos != diff_pos) {
                DPLOG(num, "i=" << i << " pos diff mismatch: " << (last_i.pos - last_i1.pos) << " "
                                << (last_i1.pos - last_i2.pos) << " " << (last_i2.pos - last_i3.pos)
                                << " expected=" << diff_pos);
                return false;
            }
        }
        DPLOG(num, "*** LOOP DETECTED ***");
        return true;
    }

private:
    const char* name_;
    std::vector<BBState> history_;
};

int64_t BbMachine::check_loop() {
    BbMachine m(num_states_, max_steps_);
    m.table_ = table_;

    BbStateHistory bb_state_history("bb");
    BbStateHistory max_head_pos_history("max");
    BbStateHistory min_head_pos_history("min");
    for (size_t i = 0; i < count_; ++i) {
        const auto& instruction = table_.get_instruction(m.state_, m.tape_.read());
        assert(instruction.is_halt() == false);
        m.tape_.write_and_move(instruction.write, instruction.dir);
        m.state_ = instruction.next;

        auto head_pos = static_cast<int64_t>(m.tape_.head_pos());
        DLOG("--- i=" << i << " state=" << m.state_ << " pos=" << head_pos
                      << " val=" << static_cast<int>(m.tape_.read())
                      << " max=" << max_head_pos_history.last_pos()
                      << " min=" << min_head_pos_history.last_pos());

        bb_state_history.add(i, head_pos, m.state_, m.tape_.read());
        DLOG("  bb.add (size=" << bb_state_history.size() << ")");

        if (head_pos > max_head_pos_history.last_pos()) {
            max_head_pos_history.add(i, head_pos, m.state_, m.tape_.read());
            DLOG("  max.add (size=" << max_head_pos_history.size() << ")");
        } else if (head_pos < min_head_pos_history.last_pos()) {
            min_head_pos_history.add(i, head_pos, m.state_, m.tape_.read());
            DLOG("  min.add (size=" << min_head_pos_history.size() << ")");
        }
    }
    DLOG("--- checking periodicity at end ---");
    if (bb_state_history.check_periodicity(false, false, false))
        return static_cast<int64_t>(count_ - 1);
    if (max_head_pos_history.check_periodicity(true, true, false))
        return static_cast<int64_t>(count_ - 1);
    if (min_head_pos_history.check_periodicity(true, false, true))
        return static_cast<int64_t>(count_ - 1);
    return -1;  // no loop detected
}
