#include "bb_machine.hpp"

#include <stdexcept>

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
    void add(int64_t count, int64_t pos, State state, Symbol value) {
        history_.push_back(BBState{.count = count, .pos = pos, .state = state, .value = value});
    }
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

        // state and value should be the same for the last three transitions
        if (last.state != last1.state || last1.state != last2.state || last2.state != last3.state)
            return false;
        if (last.value != last1.value || last1.value != last2.value || last2.value != last3.value)
            return false;

        // count difference should be consistent for the last three transitions
        if (permit_count_diff_positive) {
            int64_t last_diff2_count = last_diff_count - last1_diff_count;
            int64_t last1_diff2_count = last1_diff_count - last2_diff_count;
            if (last_diff2_count < 0) {
                return false;
            }
            if (last_diff2_count != last1_diff2_count &&
                last_diff2_count != last1_diff2_count * 2) {
                return false;
            }
        } else {
            if (last_diff_count != last1_diff_count || last1_diff_count != last2_diff_count)
                return false;
        }

        // head position difference should be consistent for the last three transitions
        int64_t diff_pos = last.pos - last1.pos;
        if (diff_pos != last1.pos - last2.pos || diff_pos != last2.pos - last3.pos)
            return false;  // the difference in head position should be consistent

        if (permit_pos_diff_positive) {
            if (diff_pos < 0)
                return false;
        } else if (permit_pos_negative) {
            if (diff_pos > 0)
                return false;
        } else {
            if (diff_pos != 0)
                return false;
        }

        for (int i = 1; i < num; ++i) {
            const auto& last_i = history_[history_.size() - 1 - i];
            const auto& last_i1 = history_[history_.size() - 1 - num - i];
            const auto& last_i2 = history_[history_.size() - 1 - 2 * num - i];
            const auto& last_i3 = history_[history_.size() - 1 - 3 * num - i];

            if (last_i.state != last_i1.state || last_i1.state != last_i2.state ||
                last_i2.state != last_i3.state)
                return false;
            if (last_i.value != last_i1.value || last_i1.value != last_i2.value ||
                last_i2.value != last_i3.value)
                return false;

            if (last_i.count - last_i1.count != last_diff_count)
                return false;
            if (last_i1.count - last_i2.count != last1_diff_count)
                return false;
            if (last_i2.count - last_i3.count != last2_diff_count)
                return false;

            if (last_i.pos - last_i1.pos != diff_pos || last_i1.pos - last_i2.pos != diff_pos ||
                last_i2.pos - last_i3.pos != diff_pos)
                return false;
        }
        return true;
    }

private:
    std::vector<BBState> history_;
};

int64_t BbMachine::check_loop() {
    BbMachine m(num_states_, max_steps_);
    m.table_ = table_;

    BbStateHistory bb_state_history;
    BbStateHistory max_head_pos_history;
    BbStateHistory min_head_pos_history;
    for (size_t i = 0; i < count_; ++i) {
        const auto& instruction = table_.get_instruction(m.state_, m.tape_.read());
        assert(instruction.is_halt() == false);
        m.tape_.write_and_move(instruction.write, instruction.dir);
        m.state_ = instruction.next;

        auto head_pos = static_cast<int64_t>(m.tape_.head_pos());
        bb_state_history.add(i, head_pos, m.state_, m.tape_.read());
        if (bb_state_history.check_periodicity(false, false, false))
            return static_cast<int64_t>(i);
        if (head_pos > max_head_pos_history.last_pos()) {
            max_head_pos_history.add(i, head_pos, m.state_, m.tape_.read());
            if (max_head_pos_history.check_periodicity(true, true, false))
                return static_cast<int64_t>(i);
        } else if (head_pos < min_head_pos_history.last_pos()) {
            min_head_pos_history.add(i, head_pos, m.state_, m.tape_.read());
            if (min_head_pos_history.check_periodicity(true, false, true))
                return static_cast<int64_t>(i);
        }
    }
    return -1;  // no loop detected
}
