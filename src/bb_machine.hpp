#pragma once

#include <algorithm>
#include <vector>

using State = int;
using Symbol = int;

class Tape {
    char* data_;
    size_t offset_;
    size_t size_;
    size_t head_;

public:
    Tape() : data_(nullptr), size_(0), head_(0), offset_(0) {}
    Tape(size_t max_pos) : offset_(max_pos), head_(0), size_(max_pos * 2 + 1) {
        data_ = new char[size_];
        std::fill(data_, data_ + size_, 0);
    }
    ~Tape() { delete[] data_; }
    Tape(const Tape&) = delete;
    Tape& operator=(const Tape&) = delete;
    void write_and_move(char value, int dir) {
        data_[head_ + offset_] = value;
        head_ += dir;
    }
    [[nodiscard]] char read() const { return data_[head_ + offset_]; }
};

struct Instruction {
    Symbol write;
    int dir;
    State next;
};

class TransitionTable {
public:
    TransitionTable(int num_states) {
        for (int i = 0; i < num_states; ++i) {
            for (int j = 0; j < 2; ++j) {
                table_.push_back({.write = 0, .dir = 0, .next = -1});
            }
        }
    }

    void set_instruction(State state, Symbol read, Symbol write, int dir, State next) {
        table_[key(state, read)] = {.write = write, .dir = dir, .next = next};
    }

    [[nodiscard]] const Instruction& get_instruction(State state, Symbol read) const {
        return table_[key(state, read)];
    }

private:
    [[nodiscard]] size_t key(State state, Symbol read) const { return state * 2 + read; }
    std::vector<Instruction> table_;
};

class BbMachine {
public:
    enum class Dir { L = -1, R = 1 };
    enum class Result { HALT, MAX_STEPS_EXCEEDED };

    BbMachine() = default;
    BbMachine(const BbMachine&) = delete;
    BbMachine& operator=(const BbMachine&) = delete;
    explicit BbMachine(int num_states, size_t max_steps)
        : num_states_(num_states), max_steps_(max_steps), table_(num_states), tape_(max_steps) {}

    void set_instruction(State state, Symbol read, Symbol write, Dir dir, State next) {
        table_.set_instruction(state, read, write, static_cast<int>(dir), next);
    }

    Result run();
    [[nodiscard]] uint64_t count() const { return count_; }

private:
    int num_states_ = 0;
    size_t max_steps_ = 0;
    State state_ = 0;
    Tape tape_;
    TransitionTable table_{0};
    uint64_t count_ = 0;
};
