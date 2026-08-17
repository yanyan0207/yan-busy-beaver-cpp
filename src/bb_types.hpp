#pragma once

#include <cstddef>
#include <print>
#include <string>
#include <vector>

using State = int;

enum class Symbol : char { ZERO = 0, ONE = 1 };
enum class Dir { L = -1, R = 1 };

struct Instruction {
    Symbol write;
    Dir dir;
    State next;

    [[nodiscard]] bool is_halt() const { return next == -1; }
    [[nodiscard]] bool operator==(const Instruction&) const = default;
};

struct Transition {
    State state;
    Symbol value;
    Instruction instr;
};

class TransitionTable {
public:
    explicit TransitionTable(int num_states) {
        for (int i = 0; i < num_states; ++i) {
            for (int j = 0; j < 2; ++j) {
                table_.push_back({.write = Symbol::ZERO, .dir = Dir::R, .next = -1});
            }
        }
    }

    void set_instruction(State state, Symbol read, const Instruction& instr) {
        table_[key(state, read)] = instr;
    }

    [[nodiscard]] const Instruction& get_instruction(State state, Symbol read) const {
        return table_[key(state, read)];
    }

    [[nodiscard]] const std::vector<Instruction>& get_instructions() const { return table_; }

private:
    [[nodiscard]] static size_t key(State state, Symbol read) {
        return static_cast<size_t>(state) * 2 + static_cast<size_t>(read);
    }

    std::vector<Instruction> table_;
};

static std::string instruction_to_string(const Instruction& instr) {
    std::string s;
    s += std::to_string(static_cast<int>(instr.write));
    s += (instr.dir == Dir::R) ? 'R' : 'L';
    s += (instr.next == -1) ? 'H' : static_cast<char>('A' + instr.next);
    return s;
}

static std::string table_to_notation(const std::vector<Instruction>& table) {
    std::string result;
    for (size_t i = 0; i < table.size(); ++i) {
        if (i > 0)
            result += ' ';
        const auto& t = table[i];
        result += instruction_to_string(t);
    }
    return result;
}
