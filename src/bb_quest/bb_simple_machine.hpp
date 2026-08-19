#pragma once
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

#include "../bb_types.hpp"

class BbSimpleMachine {
protected:
    std::vector<Symbol> tape_;
    int64_t head_pos_ = 0;

    int64_t step_count_ = 0;
    State current_state_ = 0;

    const TransitionTable* transition_table_;

public:
    BbSimpleMachine() = default;
    BbSimpleMachine(const TransitionTable* transition_table, int64_t tape_size) {
        init(tape_size, transition_table);
    }
    void init(int64_t tape_size, const TransitionTable* transition_table) {
        // tapeの初期化
        tape_.resize(tape_size, Symbol::ZERO);
        head_pos_ = 0;

        // 状態の初期化
        step_count_ = 0;
        current_state_ = 0;

        // 遷移表の初期化
        transition_table_ = transition_table;
    }

    virtual const Instruction& run_one_step() {
        assert(transition_table_ != nullptr);
        const Symbol current_symbol = read_symbol(head_pos_);
        const Instruction& instr =
            transition_table_->get_instruction(current_state_, current_symbol);

        step_count_++;
        if (instr.is_halt()) {
            return instr;
        }

        current_state_ = instr.next;
        write_symbol(instr.write);
        head_pos_ += static_cast<int>(instr.dir);
        return instr;
    }

    Transition run(int64_t max_steps) {
        assert(step_count_ < max_steps);
        while (step_count_ < max_steps) {
            const State prev_state = current_state_;
            const Symbol prev_symbol = current_symbol();
            const Instruction& instr = run_one_step();
            if (instr.is_halt())
                return {.state = prev_state, .value = prev_symbol, .instr = instr};
        }
        const Symbol symbol = current_symbol();
        const Instruction& instr = transition_table_->get_instruction(current_state_, symbol);
        return {.state = current_state_, .value = symbol, .instr = instr};
    }

    // 状態取得
    [[nodiscard]] int64_t head_pos() const { return head_pos_; }
    [[nodiscard]] int64_t count() const { return step_count_; }
    [[nodiscard]] State current_state() const { return current_state_; }

    // tape用メソッド
    [[nodiscard]] Symbol current_symbol() const { return read_symbol(head_pos_); }
    [[nodiscard]] Symbol read_symbol(int64_t pos) const {
        return static_cast<Symbol>(tape_[index(pos)]);
    }
    [[nodiscard]] std::span<const Symbol> get_tape_range(int64_t start_pos, int64_t size) const {
        int64_t index = this->index(start_pos);
        return {&tape_[index], static_cast<size_t>(size)};
    }

protected:
    void write_symbol(Symbol symbol) { tape_[index(head_pos_)] = symbol; }
    [[nodiscard]] int64_t index(int64_t pos) const {
        const int64_t idx = pos + static_cast<int64_t>(tape_.size() / 2);
        assert(idx >= 0);
        assert(static_cast<size_t>(idx) < tape_.size());
        return idx;
    }
};
