from argparse import ArgumentParser
from copy import deepcopy
from itertools import permutations

import pandas as pd


def main(n_states: int):
    skipped_df = pd.read_csv(
        f"results/bb_quest_{n_states}/01_simple_search/skipped.csv",
    )
    decided_df = pd.read_csv(
        f"results/bb_quest_{n_states}/01_simple_search/decided.csv",
    )
    unresolved_df = pd.read_csv(
        f"results/bb_quest_{n_states}/01_simple_search/unresolved.csv"
    )

    print(f"n_states: {n_states}")
    print(f"skipped: {len(skipped_df)}")
    print(f"decided: {len(decided_df)}")
    print(f"unresolved: {len(unresolved_df)}")

    cnt = 0

    def permute_pattern(pattern: str, perm: tuple[str, str, str]) -> str:
        nonlocal cnt
        instructions = pattern.split()

        blocks = {
            "A": instructions[0:2],
            "B": instructions[2:4],
            "C": instructions[4:6],
            "D": instructions[6:8],
        }

        mapping = {
            "A": "A",
            "B": perm[0],
            "C": perm[1],
            "D": perm[2],
            "H": "H",
        }

        # まず各命令内の遷移先状態をrename
        renamed = {}
        for old_state, block in blocks.items():
            renamed[mapping[old_state]] = [
                inst[:2] + mapping.get(inst[2], inst[2]) for inst in block
            ]

        # 新しい A,B,C,D 順に並べ直す
        result = renamed["A"] + renamed["B"] + renamed["C"] + renamed["D"]

        ret = " ".join(result)
        # if cnt % 100000 == 0:
        #     print(perm, pattern, "->", ret)
        # cnt += 1
        return ret

    for perm in permutations("BCD"):
        work_df = deepcopy(unresolved_df)
        work_df["pattern"] = unresolved_df["pattern"].apply(
            lambda x: permute_pattern(x, perm)
        )

        # skipped_dfのpattern列とwork_dfのpattern列を比較して、マッチする行を抽出
        matched_skipped = work_df["pattern"].isin(skipped_df["pattern"])
        if matched_skipped.any():
            print(f"Found matching pattern in skipped: {perm}")
            print(work_df[matched_skipped])

        # decided_dfのpattern列とwork_dfのpattern列を比較して、マッチする行を抽出
        matched = work_df["pattern"].isin(decided_df["pattern"])
        if matched.any():
            print(f"Found matching pattern: {perm}")
            print(work_df[matched])

        if perm != ("B", "C", "D"):
            # 元のファイルとも比較
            work_df["pattern"] == unresolved_df["pattern"]
            matched = work_df["pattern"].isin(unresolved_df["pattern"])
            if matched.any():
                print(f"Found matching pattern: {perm}")
                print(work_df[matched])


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("n_states", type=int)
    args = parser.parse_args()
    main(args.n_states)
