#include <print>
#include <stdexcept>
#include <string>
#include <vector>

#include "bb_machine.hpp"

// 標準BB表記をパース: "1RB 1LB 1LA 1RH"
// 各トークンは <write><dir><next> の3文字
// 状態: A=0, B=1, C=2, ... H=halt(-1)
// 例: BB(2) -> "1RB 1LB 1LA 1RH"
BbMachine parse(const std::string& notation) {
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < notation.size()) {
        while (i < notation.size() && notation[i] == ' ')
            ++i;
        if (i + 3 <= notation.size()) {
            tokens.push_back(notation.substr(i, 3));
            i += 3;
        } else
            break;
    }

    // トークン数からnum_statesを決定（各状態に2トークン: read=0, read=1）
    if (tokens.size() % 2 != 0)
        throw std::runtime_error("invalid notation");
    const int num_states = static_cast<int>(tokens.size() / 2);

    BbMachine m(num_states, 1'000'000);  // max_stepsは適当に大きく設定

    for (int state = 0; state < num_states; ++state) {
        for (int read = 0; read < 2; ++read) {
            const auto& tok = tokens[state * 2 + read];
            const int write = tok[0] - '0';
            const auto dir = (tok[1] == 'R') ? BbMachine::Dir::R : BbMachine::Dir::L;
            const int next = (tok[2] == 'H') ? -1 : (tok[2] - 'A');
            m.set_instruction(state, read, write, dir, next);
        }
    }
    return m;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::println("usage: bb <notation>");
        std::println(R"(example: bb "1RB 1LB 1LA 1RH")");
        return 1;
    }

    BbMachine m = parse(argv[1]);
    const auto result = m.run();

    if (result == BbMachine::Result::HALT) {
        std::println("halted: {} steps", m.count());
    } else {
        std::println("max steps exceeded: {} steps", m.count());
    }
}
