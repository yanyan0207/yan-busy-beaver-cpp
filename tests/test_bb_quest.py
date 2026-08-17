import csv
import subprocess
from pathlib import Path

import pytest

_PROJECT_DIR = Path(__file__).parent.parent
_BB_SEARCH = _PROJECT_DIR / "build" / "bb_search.exe"
_BB_QUEST = _PROJECT_DIR / "build" / "bb_quest.exe"
_BB_SEARCH_BOOTSTRAP_ONLY_PATTERNS = {"0LA 0RH 0RH 0RH"}


def _require_executables() -> None:
    missing = [path for path in (_BB_SEARCH, _BB_QUEST) if not path.exists()]
    if missing:
        pytest.skip(f"missing executables: {', '.join(str(path) for path in missing)}")


def _run_tools(tmp_path: Path, n_states: int, max_steps: int) -> None:
    (tmp_path / "results").mkdir()
    subprocess.run(
        [str(_BB_SEARCH), str(n_states), str(max_steps), "--output-csv"],
        cwd=tmp_path,
        check=True,
        capture_output=True,
    )
    subprocess.run(
        [str(_BB_QUEST), str(n_states), str(max_steps)],
        cwd=tmp_path,
        check=True,
        capture_output=True,
    )


def _load_bb_search_results(path: Path) -> dict[str, tuple[str, int]]:
    result: dict[str, tuple[str, int]] = {}
    with path.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            success = "true" if row["result"] == "halt" else "false"
            result[row["pattern"]] = (success, int(row["steps"]))
    return result


def _load_bb_quest_results(
    success_path: Path,
    failure_path: Path,
) -> dict[str, tuple[str, int]]:
    result: dict[str, tuple[str, int]] = {}
    for path, success in ((success_path, "true"), (failure_path, "false")):
        with path.open(newline="", encoding="utf-8") as f:
            for row in csv.reader(f):
                if not row:
                    continue
                result[row[0]] = (success, int(row[2]))
    return result


@pytest.mark.parametrize("max_steps", [1, 2, 3])
def test_bb_quest_results_match_bb_search(tmp_path: Path, max_steps: int) -> None:
    _require_executables()

    n_states = 2
    _run_tools(tmp_path, n_states, max_steps)

    bb_search = _load_bb_search_results(
        tmp_path / "results" / f"bb_{n_states}_patterns.csv"
    )
    bb_quest = _load_bb_quest_results(
        tmp_path / "results" / f"bb_quest_{n_states}_0.csv",
        tmp_path / "results" / f"bb_quest_{n_states}_1.csv",
    )

    extra_in_quest = set(bb_quest) - set(bb_search)
    assert not extra_in_quest

    search_only = set(bb_search) - set(bb_quest)
    assert search_only == _BB_SEARCH_BOOTSTRAP_ONLY_PATTERNS

    mismatches = {
        pattern: (bb_search[pattern], bb_quest[pattern])
        for pattern in bb_quest
        if bb_search[pattern] != bb_quest[pattern]
    }
    assert not mismatches
