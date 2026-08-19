#include <argparse/argparse.hpp>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>

#include "../bb_types.hpp"
#include "bb_simple_machine.hpp"

struct Config {
    int n_states;
    int64_t max_steps;
    int64_t max_candidates;
};

struct OutputFiles {
    std::ofstream success;
    std::ofstream failure;
};

class BbCheckLoopMachine : public BbSimpleMachine {
    struct HistoryEntry {
        int64_t step;
        int64_t read_pos;
    };
    std::vector<HistoryEntry> history;
    int64_t max_pos = 0;
    int64_t min_pos = 0;

protected:
    [[nodiscard]] std::pair<int64_t, int64_t> get_read_pos_range(int64_t start_step) const {
        assert(start_step >= 0 && start_step < history.size());

        int64_t min_pos = history[start_step].read_pos;
        int64_t max_pos = history[start_step].read_pos;
        for (int64_t i = start_step; i < history.size(); ++i) {
            const HistoryEntry& entry = history[i];
            if (entry.read_pos < min_pos) {
                min_pos = entry.read_pos;
            }
            if (entry.read_pos > max_pos) {
                max_pos = entry.read_pos;
            }
        }
        return {min_pos, max_pos};
    }

public:
    const Instruction& run_one_step() override {
        int64_t read_pos = head_pos();
        const Instruction& instr = BbSimpleMachine::run_one_step();
        int64_t step = count();
        history.push_back({
            .step = step,
            .read_pos = read_pos,
        });
        int64_t pos = head_pos();
        if (pos > max_pos) {
            max_pos = pos;
        }
        if (pos < min_pos) {
            min_pos = pos;
        }
        return instr;
    }

    struct Result {
        bool is_loop;
        int64_t pos_diff;
        int64_t count;
    };
    [[nodiscard]] static Result check_loop(const std::string& notation, int n_states,
                                           int64_t max_steps) {
        BbCheckLoopMachine reference_machine;
        TransitionTable table = TransitionTable::create_trans_table(notation, n_states);
        reference_machine.init(max_steps * 2 + 1, &table);
        const Transition& tran = reference_machine.run(max_steps);
        assert(!tran.instr.is_halt());
        assert(reference_machine.count() == max_steps);

        BbCheckLoopMachine machine;
        machine.init(max_steps * 2 + 1, &table);
        while (machine.count() < reference_machine.count() - 1) {
            const Instruction& instr = machine.run_one_step();
            assert(!instr.is_halt());

            if (machine.current_state() == reference_machine.current_state() &&
                machine.current_symbol() == reference_machine.current_symbol()) {
                const int64_t pos_diff = reference_machine.head_pos() - machine.head_pos();
                if (pos_diff == 0) {
                    // 恒等ループをチェック
                    const auto [min_pos, max_pos] =
                        reference_machine.get_read_pos_range(machine.count());
                    const auto reference_tape_range =
                        reference_machine.get_tape_range(min_pos, max_pos - min_pos + 1);
                    const auto machine_tape_range =
                        machine.get_tape_range(min_pos, max_pos - min_pos + 1);

                    // テープの内容が一致するかを確認
                    if (std::ranges::equal(reference_tape_range, machine_tape_range)) {
                        // 中身が同じ
                        return Result{
                            .is_loop = true, .pos_diff = pos_diff, .count = machine.count()};
                    }

                } else if (pos_diff > 0) {
                    // 右にずれるループをチェック
                    if (machine.max_pos + pos_diff > reference_machine.max_pos) {
                        continue;
                    }

                    auto [min_pos, max_pos] = reference_machine.get_read_pos_range(machine.count());

                    int64_t size = reference_machine.max_pos - (min_pos + pos_diff) + 1;
                    const auto reference_tape_range =
                        reference_machine.get_tape_range(min_pos + pos_diff, size);
                    const auto machine_tape_range = machine.get_tape_range(min_pos, size);

                    // テープの内容が一致するかを確認
                    if (std::ranges::equal(reference_tape_range, machine_tape_range)) {
                        // 中身が同じ
                        return Result{
                            .is_loop = true, .pos_diff = pos_diff, .count = machine.count()};
                    }
                } else {
                    // 左にずれるループをチェック
                    if (machine.min_pos + pos_diff < reference_machine.min_pos) {
                        continue;
                    }

                    auto [min_pos, max_pos] = reference_machine.get_read_pos_range(machine.count());

                    int64_t size = (max_pos + pos_diff) - reference_machine.min_pos + 1;
                    const auto reference_tape_range =
                        reference_machine.get_tape_range(min_pos, size);
                    const auto machine_tape_range =
                        machine.get_tape_range(min_pos - pos_diff, size);

                    // テープの内容が一致するかを確認
                    if (std::ranges::equal(reference_tape_range, machine_tape_range)) {
                        // 中身が同じ
                        return Result{
                            .is_loop = true, .pos_diff = pos_diff, .count = machine.count()};
                    }
                }
            }
        }
        return Result{.is_loop = false};
    }
};

int main(int argc, char* argv[]) {
    argparse::ArgumentParser app("bb_quest");

    app.add_argument("n_states").help("number of states").scan<'i', int>();
    app.add_argument("max_steps")
        .help("max steps")
        .default_value(static_cast<int64_t>(1000))
        .scan<'i', int64_t>();
    app.add_argument("--max-candidates")
        .help("max candidates to try (0 = unlimited)")
        .default_value(static_cast<int64_t>(0))
        .scan<'i', int64_t>();

    try {
        app.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::println(stderr, "{}", e.what());
        std::println(stderr, "{}", app.help().str());
        return 1;
    }

    Config config;
    config.n_states = app.get<int>("n_states");
    config.max_steps = app.get<int64_t>("max_steps");
    config.max_candidates = app.get<int64_t>("--max-candidates");

    std::println("n={}, max_steps={} max_candidates={}", config.n_states, config.max_steps,
                 config.max_candidates);

    std::string ifilename =
        std::format("results/bb_quest_{}/01_simple_search/unresolved.csv", config.n_states);
    auto ifile = std::ifstream(ifilename);

    std::string output_dirname = std::format("results/bb_quest_{}/02_check_loop", config.n_states);
    const auto output_dir = std::filesystem::path{output_dirname};
    std::filesystem::create_directories(output_dir);
    auto decided = std::ofstream(std::format("{}/decided.csv", output_dirname));
    auto unresoleved = std::ofstream(std::format("{}/unresolved.csv", output_dirname));

    if (!ifile) {
        std::println(stderr, "Failed to open file: {}", ifilename);
        return 1;
    }

    if (!decided) {
        std::println(stderr, "Failed to open file: {}/decided.csv", output_dirname);
        return 1;
    }

    if (!unresoleved) {
        std::println(stderr, "Failed to open file: {}/unresolved.csv", output_dirname);
        return 1;
    }
    decided << "pattern,result,steps" << '\n';
    unresoleved << "pattern,result,steps" << '\n';

    std::string line;

    // ヘッダを飛ばす
    std::getline(ifile, line);

    int64_t count = 0;
    while (std::getline(ifile, line)) {
        if (line.empty())
            continue;

        const auto comma_pos = line.find(',');
        if (comma_pos == std::string::npos) {
            std::println(stderr, "Invalid line format: {}", line);
            continue;
        }
        // std::println("pattern={}", line.substr(0, comma_pos));
        std::string pattern = line.substr(0, comma_pos);
        auto result = BbCheckLoopMachine::check_loop(pattern, config.n_states, config.max_steps);
        // std::println("is_loop={}, pos_diff={}, count={}", result.is_loop, result.pos_diff,
        //              result.count);

        if (result.is_loop) {
            decided << pattern << ",loop," << result.count << '\n';
        } else {
            unresoleved << pattern << ",unresolved," << result.count << '\n';
        }

        count++;
        if (config.max_candidates > 0 && count >= config.max_candidates) {
            break;
        }
    }
}
