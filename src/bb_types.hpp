#pragma once

#include <cassert>
#include <cstddef>
#include <print>
#include <sstream>
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

class TransitionTable {
public:
    static TransitionTable create_trans_table(const std::string& notation, int n_states) {
        TransitionTable table(n_states);
        std::istringstream stream(notation);

        std::string instr_str;

        for (int i = 0; i < n_states * 2; ++i) {
            stream >> instr_str;
            assert(instr_str.size() == 3);
            assert(instr_str[0] == '0' || instr_str[0] == '1');
            assert(instr_str[1] == 'R' || instr_str[1] == 'L');
            assert(instr_str[2] == 'H' ||
                   (instr_str[2] >= 'A' && instr_str[2] <= 'A' + n_states - 1));
            const auto write = static_cast<Symbol>(instr_str[0] - '0');
            const Dir dir = instr_str[1] == 'R' ? Dir::R : Dir::L;
            const State next = instr_str[2] == 'H' ? -1 : static_cast<State>(instr_str[2] - 'A');

            table.set_instruction(i / 2, static_cast<Symbol>(i % 2),
                                  {.write = write, .dir = dir, .next = next});
        }

        assert(table_to_notation(table.get_instructions()) == notation);
        return table;
    }

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
