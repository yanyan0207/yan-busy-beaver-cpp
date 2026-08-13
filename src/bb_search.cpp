#include <argparse/argparse.hpp>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <unordered_map>
#include <vector>

#include "bb_machine.hpp"

using Config = std::unordered_map<std::string, std::string>;

struct Transition {
    int write;
    int dir;   // -1=L, 1=R
    int next;  // -1=halt
};

std::string git_version() {
    std::array<char, 128> buf{};
    std::string result;
    FILE* pipe = _popen("git describe --tags 2>nul", "r");
    if (!pipe)
        return "unknown";
    while (fgets(buf.data(), buf.size(), pipe))
        result += buf.data();
    _pclose(pipe);
    if (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result.empty() ? "unknown" : result;
}

void save_csv(int n_states, const Config& config, double elapsed_sec) {
    std::filesystem::create_directories("results");
    const std::string path = std::format("results/bb_{}.csv", n_states);

    std::vector<std::string> lines;
    bool updated = false;
    {
        std::ifstream f(path);
        std::string line;
        while (std::getline(f, line))
            lines.push_back(line);
    }

    const std::string version = config.at("git_version");
    const std::string max_steps = config.at("max_steps");
    const std::string key = std::format("{},{}", version, max_steps);
    const std::string new_row = std::format("{},{},{:.3f}", version, max_steps, elapsed_sec);

    if (lines.empty())
        lines.push_back("git_version,max_steps,elapsed_seconds");

    for (auto& l : lines) {
        if (l.starts_with(key)) {
            l = new_row;
            updated = true;
            break;
        }
    }
    if (!updated)
        lines.push_back(new_row);

    std::ofstream f(path);
    for (const auto& l : lines)
        f << l << '\n';

    std::println("saved: {}", path);
}

std::string table_to_notation(const std::vector<Transition>& table) {
    std::string result;
    for (size_t i = 0; i < table.size(); ++i) {
        if (i > 0)
            result += ' ';
        const auto& t = table[i];
        if (t.next == -1)
            result += std::format("{}{}H", t.write, t.dir == 1 ? 'R' : 'L');
        else
            result += std::format("{}{}{}", t.write, t.dir == 1 ? 'R' : 'L',
                                  static_cast<char>('A' + t.next));
    }
    return result;
}

void save_log(int n_states, size_t max_steps_arg, uint64_t best_steps, size_t candidates_tried,
              size_t timeout_count, const std::vector<std::string>& max_patterns,
              const std::string& version) {
    std::filesystem::create_directories("results");
    const std::string path = std::format("results/{}states_{}steps.log", n_states, max_steps_arg);

    std::ofstream f(path);
    f << "git_tag: " << version << '\n';
    f << "candidates_tried: " << candidates_tried << '\n';
    f << "max_steps: " << best_steps << '\n';
    f << "timeout_count: " << timeout_count << '\n';
    f << "max_patterns: [";
    for (size_t i = 0; i < max_patterns.size(); ++i) {
        if (i > 0)
            f << ", ";
        f << '"' << max_patterns[i] << '"';
    }
    f << "]\n";
    std::println("saved: {}", path);
}

void search(const Config& config) {
    const int n_states = std::stoi(config.at("n_states"));
    const size_t max_steps = std::stoull(config.at("max_steps"));
    const bool save_time = config.count("save_time") > 0;

    const int total_slots = n_states * 2;

    std::vector<Transition> choices;
    for (int w : {0, 1})
        for (int d : {-1, 1})
            for (int s = 0; s < n_states; ++s)
                choices.push_back({.write = w, .dir = d, .next = s});

    const int n_choices = static_cast<int>(choices.size());

    uint64_t best_steps = 0;
    std::vector<std::string> max_patterns;
    size_t candidates_tried = 0;
    size_t timeout_count = 0;

    const int free_slots = total_slots - 1;
    size_t total_patterns = 1;
    for (int i = 0; i < free_slots; ++i)
        total_patterns *= n_choices;

    const auto t_start = std::chrono::steady_clock::now();

    for (int halt_pos = 0; halt_pos < total_slots; ++halt_pos) {
        for (size_t p = 0; p < total_patterns; ++p) {
            std::vector<Transition> table(total_slots);
            size_t tmp = p;
            for (int i = total_slots - 1; i >= 0; --i) {
                if (i == halt_pos) {
                    table[i] = {.write = 0, .dir = 1, .next = -1};
                } else {
                    table[i] = choices[tmp % n_choices];
                    tmp /= n_choices;
                }
            }

            BbMachine m(n_states, max_steps);
            for (int s = 0; s < n_states; ++s)
                for (int r = 0; r < 2; ++r) {
                    const auto& t = table[s * 2 + r];
                    m.set_instruction(s, r, t.write,
                                      t.dir == 1 ? BbMachine::Dir::R : BbMachine::Dir::L, t.next);
                }

            ++candidates_tried;
            const auto result = m.run();
            if (result == BbMachine::Result::MAX_STEPS_EXCEEDED) {
                ++timeout_count;
            } else if (m.count() > best_steps) {
                best_steps = m.count();
                max_patterns.clear();
                max_patterns.push_back(table_to_notation(table));
            } else if (m.count() == best_steps) {
                max_patterns.push_back(table_to_notation(table));
            }
        }
    }

    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();

    std::println("最大: {} steps", best_steps);
    std::println("経過時間: {:.3f} 秒", elapsed);

    const std::string version = git_version();
    save_log(n_states, max_steps, best_steps, candidates_tried, timeout_count, max_patterns,
             version);

    if (save_time) {
        Config c = config;
        c["git_version"] = version;
        save_csv(n_states, c, elapsed);
    }
}

int main(int argc, char* argv[]) {
    argparse::ArgumentParser app("bb_search");

    app.add_argument("n_states").help("number of states").scan<'i', int>();
    app.add_argument("max_steps").help("max steps").default_value(size_t(1000)).scan<'u', size_t>();
    app.add_argument("--save-time")
        .help("save elapsed time to CSV")
        .default_value(false)
        .implicit_value(true);

    try {
        app.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::println(stderr, "{}", e.what());
        std::println(stderr, "{}", app.help().str());
        return 1;
    }

    Config config;
    config["n_states"] = std::to_string(app.get<int>("n_states"));
    config["max_steps"] = std::to_string(app.get<size_t>("max_steps"));
    if (app.get<bool>("--save-time"))
        config["save_time"] = "1";

    std::println("n={}, max_steps={}", config["n_states"], config["max_steps"]);
    search(config);
}
