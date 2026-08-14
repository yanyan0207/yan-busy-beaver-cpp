#include <cassert>
#include <print>

#include "../src/bb_machine.hpp"

static BbMachine* make(int n, std::initializer_list<Instruction> instrs) {
    auto* m = new BbMachine(n, 10000);
    int slot = 0;
    for (const auto& instr : instrs) {
        m->set_instruction(slot / 2, static_cast<Symbol>(slot % 2), instr);
        ++slot;
    }
    return m;
}

static void test_halt_1step() {
    // A0=0RH: 1ステップでハルト
    const Instruction halt = {.write = Symbol::ZERO, .dir = Dir::R, .next = -1};
    auto* m = make(2, {halt, halt, halt, halt});
    const auto r = m->run();
    assert(r.instr.is_halt());
    assert(m->count() == 1);
    delete m;
    std::println("test_halt_1step: ok");
}

static void test_bb2_champion_6steps() {
    // "1RB 1LB 1LA 0RH" → 6ステップでハルト
    auto* m = make(2, {
                          {.write = Symbol::ONE, .dir = Dir::R, .next = 1},    // A0: 1RB
                          {.write = Symbol::ONE, .dir = Dir::L, .next = 1},    // A1: 1LB
                          {.write = Symbol::ONE, .dir = Dir::L, .next = 0},    // B0: 1LA
                          {.write = Symbol::ZERO, .dir = Dir::R, .next = -1},  // B1: 0RH
                      });
    const auto r = m->run();
    assert(r.instr.is_halt());
    assert(m->count() == 6);
    delete m;
    std::println("test_bb2_champion_6steps: ok");
}

static void test_bb3_champion_21steps() {
    // "1LB 0RH 1RB 0LC 1RC 1RA" → 21ステップでハルト
    auto* m = make(3, {
                          {.write = Symbol::ONE, .dir = Dir::L, .next = 1},    // A0: 1LB
                          {.write = Symbol::ZERO, .dir = Dir::R, .next = -1},  // A1: 0RH
                          {.write = Symbol::ONE, .dir = Dir::R, .next = 1},    // B0: 1RB
                          {.write = Symbol::ZERO, .dir = Dir::L, .next = 2},   // B1: 0LC
                          {.write = Symbol::ONE, .dir = Dir::R, .next = 2},    // C0: 1RC
                          {.write = Symbol::ONE, .dir = Dir::R, .next = 0},    // C1: 1RA
                      });
    const auto r = m->run();
    assert(r.instr.is_halt());
    assert(m->count() == 21);
    delete m;
    std::println("test_bb3_champion_21steps: ok");
}

static void test_bb4_champion_107steps() {
    // "1LB 1RB 1RA 0RC 0RH 1RD 1LD 0LA" → 107ステップでハルト
    auto* m = make(4, {
                          {.write = Symbol::ONE, .dir = Dir::L, .next = 1},    // A0: 1LB
                          {.write = Symbol::ONE, .dir = Dir::R, .next = 1},    // A1: 1RB
                          {.write = Symbol::ONE, .dir = Dir::R, .next = 0},    // B0: 1RA
                          {.write = Symbol::ZERO, .dir = Dir::R, .next = 2},   // B1: 0RC
                          {.write = Symbol::ZERO, .dir = Dir::R, .next = -1},  // C0: 0RH
                          {.write = Symbol::ONE, .dir = Dir::R, .next = 3},    // C1: 1RD
                          {.write = Symbol::ONE, .dir = Dir::L, .next = 3},    // D0: 1LD
                          {.write = Symbol::ZERO, .dir = Dir::L, .next = 0},   // D1: 0LA
                      });
    const auto r = m->run();
    assert(r.instr.is_halt());
    assert(m->count() == 107);
    delete m;
    std::println("test_bb4_champion_107steps: ok");
}

static void test_timeout() {
    // A0=0LA: 左に移動し続けてタイムアウト
    auto* m = new BbMachine(2, 10);
    m->set_instruction(0, Symbol::ZERO, {.write = Symbol::ZERO, .dir = Dir::L, .next = 0});
    m->set_instruction(0, Symbol::ONE, {.write = Symbol::ZERO, .dir = Dir::R, .next = -1});
    m->set_instruction(1, Symbol::ZERO, {.write = Symbol::ZERO, .dir = Dir::R, .next = -1});
    m->set_instruction(1, Symbol::ONE, {.write = Symbol::ZERO, .dir = Dir::R, .next = -1});
    const auto r = m->run();
    assert(!r.instr.is_halt());
    assert(m->count() == 10);
    delete m;
    std::println("test_timeout: ok");
}

int main() {
    test_halt_1step();
    test_bb2_champion_6steps();
    test_bb3_champion_21steps();
    test_bb4_champion_107steps();
    test_timeout();
    std::println("All tests passed.");
}
