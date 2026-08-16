#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "bb_types.hpp"

class Tape {
    friend class BbMachine;

    std::vector<char> data_;
    size_t offset_;
    size_t size_;
    size_t head_;

public:
    Tape() : data_(), size_(0), head_(0), offset_(0) {}
    Tape(size_t max_pos) : offset_(max_pos), head_(0), size_(max_pos * 2 + 1) {
        data_.resize(size_, static_cast<char>(Symbol::ZERO));
    }
    void write_and_move(Symbol value, Dir dir) {
        data_[head_ + offset_] = static_cast<char>(value);
        head_ += static_cast<int>(dir);
    }
    [[nodiscard]] Symbol read() const { return static_cast<Symbol>(data_[head_ + offset_]); }
    [[nodiscard]] size_t head_pos() const { return head_; }
    [[nodiscard]] Symbol read_at(int offset) const {
        const size_t idx = head_ + static_cast<size_t>(offset) + offset_;
        if (idx >= size_)
            return Symbol::ZERO;
        return static_cast<Symbol>(data_[idx]);
    }

    [[nodiscard]] std::unique_ptr<Tape> clone() const {
        return std::unique_ptr<Tape>(new Tape(*this));
    }

private:
    Tape(const Tape&) = default;
    Tape& operator=(const Tape&) = delete;
};

class BbMachine {
public:
    BbMachine() = default;
    explicit BbMachine(int num_states, size_t max_steps)
        : num_states_(num_states), max_steps_(max_steps), table_(num_states), tape_(max_steps) {}

    void set_instruction(State state, Symbol read, const Instruction& instr) {
        table_.set_instruction(state, read, instr);
    }

    [[nodiscard]] int num_states() const { return num_states_; }
    [[nodiscard]] const std::vector<Instruction>& get_instructions() const {
        return table_.get_instructions();
    }
    [[nodiscard]] uint64_t count() const { return count_; }
    [[nodiscard]] size_t max_steps() const { return max_steps_; }
    [[nodiscard]] State current_state() const { return state_; }
    [[nodiscard]] Symbol current_symbol() const { return tape_.read(); }
    [[nodiscard]] int64_t head_pos() const { return static_cast<int64_t>(tape_.head_pos()); }
    [[nodiscard]] Symbol read(int64_t pos) const { return tape_.read_at(pos); }
    [[nodiscard]] std::string format_tape(int from, int to) const {
        std::string s = "...";
        for (int r = from; r <= to; ++r) {
            const char cell = '0' + static_cast<int>(tape_.read_at(r));
            if (r == 0) {
                s += '[';
                s += cell;
                s += ']';
            } else
                s += cell;
        }
        return s + "...";
    }
    [[nodiscard]] std::string format_tape(int window = 10) const {
        return format_tape(-window, window);
    }

    Transition run();
    const Instruction& move_one_step();
    int64_t check_loop();  // returns step where loop detected, -1 if not

    [[nodiscard]] std::unique_ptr<BbMachine> clone() const {
        return std::unique_ptr<BbMachine>(new BbMachine(*this));
    }

private:
    int num_states_ = 0;
    size_t max_steps_ = 0;
    State state_ = 0;
    Tape tape_;
    TransitionTable table_{0};
    uint64_t count_ = 0;

    BbMachine(const BbMachine&) = default;
    BbMachine& operator=(const BbMachine&) = delete;
};
