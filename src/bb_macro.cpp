#include "bb_macro.hpp"

#include <algorithm>
#include <print>

BbMachineUnstoppableChecker::BbMachineUnstoppableChecker(const BbMachine& m)
    : m_(BbMachine(m.num_states(), m.max_steps())), original_machine_(m) {
    for (int state = 0; state < m.num_states(); ++state) {
        for (int symbol = 0; symbol < 2; ++symbol) {
            const Instruction& instr = m.get_instructions()[state * 2 + symbol];
            m_.set_instruction(state, static_cast<Symbol>(symbol), instr);
        }
    }
}

int64_t BbMachineUnstoppableChecker::check() {
    return check_same_loop();
}

int64_t BbMachineUnstoppableChecker::check_same_loop() {
    // max_pos,min_posが同じになるまで飛ばす
    auto [min_pos, max_pos] = original_machine_.get_min_max_pos();
    int64_t first_min_pos_at = original_machine_.first_read_at(min_pos);
    int64_t first_max_pos_at_ = original_machine_.first_read_at(max_pos);
    int64_t first_min_max_pos_at = std::max(first_min_pos_at, first_max_pos_at_);
    if (first_min_max_pos_at == original_machine_.count()) {
        return -1;  // ループ検出できず
    }

    // まずはmin_pos,max_posが同じになるまで進める
    int64_t step = 1;
    for (; step <= first_min_max_pos_at; ++step) {
        Instruction instr = m_.move_one_step();
        assert(!instr.is_halt());
    }

    for (; step < original_machine_.count(); ++step) {
        Instruction instr = m_.move_one_step();
        assert(!instr.is_halt());

        // 状態が同じになるまですすめる
        if (original_machine_.current_state() != m_.current_state() ||
            original_machine_.current_symbol() != m_.current_symbol() ||
            original_machine_.head_pos() != m_.head_pos()) {
            continue;
        }

        // テープを比較する
        int64_t pos = min_pos;

        // 今後読み取るところまで飛ばす
        for (; pos <= max_pos; ++pos) {
            if (original_machine_.last_read_at(pos) < m_.count()) {
                continue;  // もう読み取らない位置なので飛ばす
            }
            break;  // 今後読み取る位置に達したらループ確定
        }
        // テープの内容を比較する
        bool is_same = true;
        for (; pos <= max_pos; ++pos) {
            if (original_machine_.last_read_at(pos) < m_.count()) {
                return m_.count();  // もう読み取らない位置に達したらループ確定
            }
            if (original_machine_.read(pos) != m_.read(pos)) {
                is_same = false;
                break;  // テープの内容が異なる場合はループではない
            }
        }
        if (is_same) {
            return step;  // ループ検出
        }
    }
    return -1;  // no loop detected
}
