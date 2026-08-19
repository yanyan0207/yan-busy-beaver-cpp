import subprocess
from pathlib import Path

import pandas as pd
import pytest

from tests.test_regression import _ensure_v01_csv

_PROJECT_DIR = Path(__file__).parent.parent
_BB_SEARCH = _PROJECT_DIR / "build" / "bb_search.exe"
_BB_QUEST_SEARCH = _PROJECT_DIR / "build" / "bb_simple_search.exe"
_BB_QUEST_CHECK_LOOP = _PROJECT_DIR / "build" / "bb_check_loop.exe"


def _require_executables() -> None:
    missing = [
        path
        for path in (
            _BB_SEARCH,
            _BB_QUEST_SEARCH,
            _BB_QUEST_CHECK_LOOP,
        )
        if not path.exists()
    ]
    assert missing == [], (
        f"missing executables: {', '.join(str(path) for path in missing)}"
    )


def _run_tools(tmp_path: Path, n_states: int, max_steps: int) -> None:
    (tmp_path / "results").mkdir()

    command = [str(_BB_SEARCH), str(n_states), str(max_steps), "--output-csv"]
    output = subprocess.run(
        command,
        cwd=tmp_path,
        check=True,
        capture_output=True,
    )
    print(command)
    print("".join(output.stdout.decode("utf-8").splitlines()[-10:]))

    command = [str(_BB_QUEST_SEARCH), str(n_states), str(max_steps)]
    output = subprocess.run(
        command,
        cwd=tmp_path,
        check=True,
        capture_output=True,
    )
    print(command)
    print("".join(output.stdout.decode("utf-8").splitlines()[-10:]))

    command = [str(_BB_QUEST_CHECK_LOOP), str(n_states), str(max_steps)]
    output = subprocess.run(
        command,
        cwd=tmp_path,
        check=False,
        capture_output=True,
    )
    print("returncode:", output.returncode)
    print("stdout:")
    print(output.stdout)
    print("stderr:")
    print(output.stderr)

    assert output.returncode == 0


@pytest.mark.parametrize("n_states", [2, 3, 4])
def test_bb_quest_results_simple_search(tmp_path: Path, n_states: int) -> None:
    max_steps = 1000
    _require_executables()

    _run_tools(tmp_path, n_states, max_steps)

    bb_search_df = pd.read_csv(
        tmp_path / "results" / f"bb_{n_states}_patterns.csv", index_col=0
    )
    bb_quest_skipped_df = pd.read_csv(
        tmp_path / "results" / f"bb_quest_{n_states}/01_simple_search/skipped.csv"
    )
    bb_quest_decided_df = pd.read_csv(
        tmp_path / "results" / f"bb_quest_{n_states}/01_simple_search/decided.csv"
    )
    bb_quest_unresolved_df = pd.read_csv(
        tmp_path / "results" / f"bb_quest_{n_states}/01_simple_search/unresolved.csv"
    )

    # 正解が一致しているか確認
    for row in bb_quest_decided_df.itertuples(index=False):
        bb_search_row = bb_search_df.loc[row.pattern]
        assert bb_search_row.result == "halt"
        assert row.steps == bb_search_row.steps

    # NGが一致しているか確認
    for row in bb_quest_unresolved_df.itertuples(index=False):
        bb_search_row = bb_search_df.loc[row.pattern]
        assert bb_search_row.result != "halt"
    for row in bb_quest_skipped_df.itertuples(index=False):
        if row.pattern not in bb_search_df.index:
            continue
        bb_search_row = bb_search_df.loc[row.pattern]
        if row.reason == "unreachable":
            assert bb_search_row.result != "halt"


@pytest.mark.parametrize("n_states", [2, 3, 4])
def test_bb_quest_results_check_loop(tmp_path: Path, n_states: int) -> None:
    max_steps = 1000
    _require_executables()

    _run_tools(tmp_path, n_states, max_steps)

    path = _ensure_v01_csv(n_states)
    print(f"bb_search csv path: {path}")
    bb_search_df = pd.read_csv(path, index_col=0)
    bb_quest_decided_df = pd.read_csv(
        tmp_path / "results" / f"bb_quest_{n_states}/02_check_loop/decided.csv"
    )
    bb_quest_unresolved_df = pd.read_csv(
        tmp_path / "results" / f"bb_quest_{n_states}/02_check_loop/unresolved.csv"
    )

    bb_search_df.index = bb_search_df.index.str.replace("[", "", regex=False)
    bb_search_df.index = bb_search_df.index.str.replace("]", "", regex=False)
    bb_search_df.index = bb_search_df.index.str.replace("'", "", regex=False)
    bb_search_df.index = bb_search_df.index.str.replace(" ", "", regex=False)
    bb_search_df.index = bb_search_df.index.str.replace(",", " ", regex=False)
    bb_search_df.index = bb_search_df.index.str.replace("None", "0RH", regex=False)

    # 正解が一致しているか確認
    for row in bb_quest_decided_df.itertuples(index=False):
        bb_search_row = bb_search_df.loc[row.pattern]
        assert bb_search_row.steps < 0, (
            row.pattern,
            bb_search_row.steps,
            row.steps,
        )

    # NGが一致しているか確認
    for row in bb_quest_unresolved_df.itertuples(index=False):
        bb_search_row = bb_search_df.loc[row.pattern]
        assert bb_search_row.steps == 0
