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

        history_.states.push_back(create_machine_state(m_, max_pos, min_pos));

        if (check_same_loop()) {
            return true;
        }
    }

    return false;
}

bool BbMachineUnstoppableChecker::check_same_loop() {
    assert(m_.count() == history_.states.size());
    if (m_.count() < 4) {
        return false;
    }

    // 最後か2のべき乗でなかったらスキップ
    if (m_.count() != m_.max_steps() && (m_.count() & (m_.count() - 1)) != 0) {
        return false;
    }

    const MachineState& machine_state = history_.states[m_.count() - 1];

    std::unique_ptr<BbMachine> trial = nullptr;
    int64_t max_pos = machine_state.max_pos;
    int64_t min_pos = machine_state.min_pos;
    for (size_t i = 1; i < m_.count() / 2; ++i) {
        // 一つ前の同じ状態を探す
        const auto& old_state = history_.states[m_.count() - 1 - i];
        if (!is_same_state(old_state, machine_state)) {
            continue;
        }
        // 同じ差分のもう一つ前の状態を探す
        const auto& old2_state = history_.states[m_.count() - 1 - i * 2];
        if (!is_same_state(old2_state, machine_state)) {
            continue;
        }

        // インストラクション履歴が同じかどうかを確認する
        std::span<const Instruction> current_instructions{
            history_.instructions.data() + m_.count() - i, i};
        std::span<const Instruction> old_instructions{
            history_.instructions.data() + m_.count() - i * 2, i};

        if (!std::ranges::equal(current_instructions, old_instructions)) {
            continue;
        }

        if (!trial) {
            // FIXME: max_stepsを1.5倍にすべき
            trial = m_.clone();
        }
        // 同じループを繰り返す
        while (trial->count() < m_.count() + i) {
            trial->move_one_step();
            if (trial->head_pos() > machine_state.max_pos ||
                trial->head_pos() < machine_state.min_pos) {
                // ポジションが範囲外に出た場合はこれ以上ループチェックしても意味はない
                return false;
            }
            max_pos = std::max(max_pos, trial->head_pos());
            min_pos = std::min(min_pos, trial->head_pos());
        }

        MachineState new_machine_state =
            create_machine_state(*trial, machine_state.max_pos, machine_state.min_pos);
        if (!is_same_state(new_machine_state, machine_state)) {
            continue;
        }

        bool is_same = true;
        for (int64_t pos = min_pos; pos <= max_pos; ++pos) {
            if (trial->read(pos) != m_.read(pos)) {
                is_same = false;
                break;
            }
        }

        // 状態が同じで、動く範囲のテープが同じなので、このマシンは無限ループする
        if (is_same) {
            return true;
        }
    }
    return false;
}
