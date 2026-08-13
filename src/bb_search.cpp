#include <print>
#include <string>
#include <vector>

#include "bb_machine.hpp"

struct Transition {
    int write;
    int dir;   // -1=L, 1=R
    int next;  // -1=halt
};

// halt を1箇所に固定した全パターンを列挙
void search(int n_states, size_t max_steps) {
    const int total_slots = n_states * 2;

    // 非halt遷移の選択肢
    std::vector<Transition> choices;
    for (int w : {0, 1})
        for (int d : {-1, 1})
            for (int s = 0; s < n_states; ++s)
                choices.push_back({.write = w, .dir = d, .next = s});

    const int n_choices = static_cast<int>(choices.size());

    uint64_t best_steps = 0;
    std::vector<Transition> best_table;

    const int free_slots = total_slots - 1;
    size_t total_patterns = 1;
    for (int i = 0; i < free_slots; ++i)
        total_patterns *= n_choices;

    for (int halt_pos = 0; halt_pos < total_slots; ++halt_pos) {
        for (size_t p = 0; p < total_patterns; ++p) {
            // p を n_choices 進数でデコードして遷移表を構築
            std::vector<Transition> table(total_slots);
            size_t tmp = p;
            int fi = free_slots - 1;
            for (int i = total_slots - 1; i >= 0; --i) {
                if (i == halt_pos) {
                    table[i] = {.write = 0, .dir = 1, .next = -1};
                } else {
                    table[i] = choices[tmp % n_choices];
                    tmp /= n_choices;
                    --fi;
                }
            }

            BbMachine m(n_states, max_steps);
            for (int s = 0; s < n_states; ++s)
                for (int r = 0; r < 2; ++r) {
                    const auto& t = table[s * 2 + r];
                    m.set_instruction(s, r, t.write,
                                      t.dir == 1 ? BbMachine::Dir::R : BbMachine::Dir::L, t.next);
                }

            if (m.run() == BbMachine::Result::HALT && m.count() > best_steps) {
                best_steps = m.count();
                best_table = table;
                std::println("new best: {} steps", best_steps);
            }
        }
    }

    std::print("最大: {} steps\n表記: ", best_steps);
    for (unsigned int i = 0; i < best_table.size(); ++i) {
        const auto& t = best_table[i];
        if (i > 0)
            std::print(" ");
        if (t.next == -1)
            std::print("{}{}H", t.write, t.dir == 1 ? 'R' : 'L');
        else
            std::print("{}{}{}", t.write, t.dir == 1 ? 'R' : 'L', static_cast<char>('A' + t.next));
    }
    std::println("");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::println("usage: bb_search <n_states> [max_steps]");
        return 1;
    }
    const int n = std::stoi(argv[1]);
    const size_t ms = argc >= 3 ? std::stoull(argv[2]) : 1000;
    std::println("n={}, max_steps={}", n, ms);
    search(n, ms);
}
