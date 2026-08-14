#include "bb_machine.hpp"

#include <stdexcept>

Transition BbMachine::run() {
    for (size_t i = count_; i < max_steps_; ++i) {
        count_++;
        const auto& instruction = table_.get_instruction(state_, tape_.read());

        if (instruction.is_halt()) {
            return Transition{
                .state = state_, .value = tape_.read(), .instr = instruction};  // halt
        }

        tape_.write_and_move(instruction.write, instruction.dir);
        state_ = instruction.next;
    }
    const auto& instruction = table_.get_instruction(state_, tape_.read());
    return Transition{
        .state = state_, .value = tape_.read(), .instr = instruction};  // max steps exceeded
}
