from argparse import ArgumentParser

import pandas as pd


def main(n_states: int):
    # skipped_df = pd.read_csv(
    #     f"results/bb_quest_{n_states}/01_simple_search/skipped.csv",
    # )
    decided_df = pd.read_csv(
        f"results/bb_quest_{n_states}/01_simple_search/decided.csv",
    )
    unresolved_df = pd.read_csv(
        f"results/bb_quest_{n_states}/01_simple_search/unresolved.csv"
    )

    for df in [decided_df, unresolved_df]:
        pattern_list = df["pattern"].tolist()
        for pattern in pattern_list:
            instructions = pattern.split()

            if len(instructions) != 8:
                raise ValueError(f"Invalid pattern: {pattern}")

            states = set(inst[2] for inst in instructions[:6])
            if "H" not in states and "D" not in states:
                print(f"Pattern {pattern} does not contain H or D in ABC blocks.")


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("n_states", type=int)
    args = parser.parse_args()
    main(args.n_states)
