#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <span>
#include <unordered_map>
#include <variant>
#include <vector>

#include "bb_machine.hpp"
#include "bb_types.hpp"

class Block {
public:
    [[nodiscard]] virtual int size() const = 0;
    virtual ~Block() = default;
};

class SymbolBlock : public Block {
private:
    Symbol symbol_;

public:
    SymbolBlock(Symbol symbol) : symbol_(symbol) {}
    [[nodiscard]] int size() const override { return 1; }

    [[nodiscard]] Symbol symbol() const { return symbol_; }
};
class LiteralBlock : public Block {
private:
    std::vector<Symbol> symbols_;

public:
    LiteralBlock(const std::vector<Symbol>& symbols) : symbols_(symbols) {}
    [[nodiscard]] int size() const override { return static_cast<int>(symbols_.size()); };

    [[nodiscard]] const std::vector<Symbol>& symbols() const { return symbols_; }
};

class InfinityZeroBlock : public Block {
public:
    [[nodiscard]] int size() const override { return -1; }
};
class RepeatBlock : public Block {
private:
    std::unique_ptr<Block> block_;
    size_t repeat_count_;

public:
    RepeatBlock(std::unique_ptr<Block> block, size_t repeat_count)
        : block_(std::move(block)), repeat_count_(repeat_count) {}
    [[nodiscard]] int size() const override {
        return static_cast<int>(block_->size() * repeat_count_);
    }

    [[nodiscard]] const Block& block() const { return *block_; }
    [[nodiscard]] size_t repeat_count() const { return repeat_count_; }
};

struct MacroTape {
private:
    std::vector<std::unique_ptr<Block>> tape_;
    int64_t head_pos_ = 0;

public:
    MacroTape() {
        tape_.emplace_back(std::make_unique<InfinityZeroBlock>());
        tape_.emplace_back(std::make_unique<SymbolBlock>(Symbol::ZERO));
        tape_.emplace_back(std::make_unique<InfinityZeroBlock>());
    }
};

struct MachineState {
    State state;
    Symbol symbol;
    int64_t pos;
    int64_t max_pos;
    int64_t min_pos;
    int64_t step;
};

inline MachineState create_machine_state(const BbMachine& m, int64_t max_pos, int64_t min_pos) {
    return MachineState{.state = m.current_state(),
                        .symbol = m.current_symbol(),
                        .pos = m.head_pos(),
                        .max_pos = max_pos,
                        .min_pos = min_pos,
                        .step = static_cast<int64_t>(m.count())};
}

inline bool is_same_state(const MachineState& s1, const MachineState& s2) {
    return s1.state == s2.state && s1.symbol == s2.symbol && s1.pos == s2.pos &&
           s1.max_pos == s2.max_pos && s1.min_pos == s2.min_pos;
}
class BbMachineUnstoppableChecker {
    struct MachineHistory {
        std::vector<MachineState> states;
        std::vector<Instruction> instructions;
    };

    bool check_same_loop();

public:
    BbMachineUnstoppableChecker(const BbMachine& m);
    bool check();

private:
    BbMachine m_;
    MachineHistory history_;
};
