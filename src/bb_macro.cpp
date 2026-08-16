#include "bb_macro.hpp"

#include <algorithm>
#include <print>

BbMachineUnstoppableChecker::BbMachineUnstoppableChecker(const BbMachine& m)
    : m_(BbMachine(m.num_states(), m.max_steps())) {
    for (int state = 0; state < m.num_states(); ++state) {
        for (int symbol = 0; symbol < 2; ++symbol) {
            const Instruction& instr = m.get_instructions()[state * 2 + symbol];
            m_.set_instruction(state, static_cast<Symbol>(symbol), instr);
        }
    }
}

bool BbMachineUnstoppableChecker::check() {
    assert(m_.count() == 0);

    int64_t max_pos = m_.head_pos();
    int64_t min_pos = m_.head_pos();
    for (size_t i = 0; i < m_.max_steps(); ++i) {
        const auto& instruction = m_.move_one_step();
        if (instruction.is_halt()) {
            return false;
        }

        history_.instructions.push_back(instruction);
        const int64_t pos = m_.head_pos();
        max_pos = std::max(max_pos, pos);
        min_pos = std::min(min_pos, pos);
        history_.positions.push_back(pos);

        history_.states.push_back(create_machine_state(m_, max_pos, min_pos));

        if (check_same_loop()) {
            return true;
        }
    }

    return false;
}

bool BbMachineUnstoppableChecker::check_same_loop() {
    assert(m_.count() == history_.states.size());

    // 2のべき乗の状態の回数では状態を保存
    if ((m_.count() & (m_.count() - 1)) == 0) {
        m_same_loop_ = m_.clone();
        return false;
    }
    assert(m_same_loop_);
    assert(((m_same_loop_->count()) & (m_same_loop_->count() - 1)) == 0);

    // 保存した場外が同じ状態かどうかを確認する
    if (!is_same_state(history_.states[m_same_loop_->count() - 1], history_.states.back())) {
        return false;
    }

    // 同じであれば、そこからの移動範囲のテープが同じかどうかを確認する
    int64_t max_pos = *std::ranges::max_element(
        history_.positions.begin() + m_same_loop_->count() - 1, history_.positions.end());
    int64_t min_pos = *std::ranges::min_element(
        history_.positions.begin() + m_same_loop_->count() - 1, history_.positions.end());

    for (int64_t pos = min_pos; pos <= max_pos; ++pos) {
        if (m_same_loop_->read(pos) != m_.read(pos)) {
            return false;
        }
    }

    return true;
}
