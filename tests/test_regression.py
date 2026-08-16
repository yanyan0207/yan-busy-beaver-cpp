"""C++ bb_search の出力を Python v0.1 の結果と突き合わせる回帰テスト。"""

import ast
import csv
import importlib.util
import io
import subprocess
import sys
from pathlib import Path
from unittest.mock import patch

import pytest

_TEST_DIR = Path(__file__).parent
_PROJECT_DIR = _TEST_DIR.parent
_BB_SEARCH = _PROJECT_DIR / "build" / "bb_search.exe"
_DATA_DIR = _TEST_DIR / "data"
_MAX_STEPS = 1000


def _v01_csv_path(n: int) -> Path:
    _DATA_DIR.mkdir(exist_ok=True)
    return _DATA_DIR / f"v01_{n}states_{_MAX_STEPS}steps.csv"


def _generate_v01_csv(n: int, path: Path) -> None:
    """tests/ にコピーした v0.1 ソースを使って正解 CSV を生成する。"""
    v01_machine_path = _TEST_DIR / "v01_bb_machine.py"
    v01_search_path = _TEST_DIR / "v01_bb_search.py"

    machine_spec = importlib.util.spec_from_file_location(
        "v01_bb_machine", v01_machine_path
    )
    assert machine_spec and machine_spec.loader
    v01_machine = importlib.util.module_from_spec(machine_spec)
    sys.modules["v01_bb_machine"] = v01_machine
    machine_spec.loader.exec_module(v01_machine)  # type: ignore[union-attr]

    search_spec = importlib.util.spec_from_file_location(
        "v01_bb_search", v01_search_path
    )
    assert search_spec and search_spec.loader
    v01_search = importlib.util.module_from_spec(search_spec)
    sys.modules["bb_machine"] = v01_machine
    search_spec.loader.exec_module(v01_search)  # type: ignore[union-attr]
    del sys.modules["bb_machine"]

    buf = io.StringIO()
    with patch("sys.stdout", new=buf), patch("sys.stderr", new=io.StringIO()):
        v01_search.run_search(n, max_steps_limit=_MAX_STEPS)  # type: ignore[attr-defined]

    path.write_text(buf.getvalue(), encoding="utf-8")


def _load_v01_halting(n: int) -> dict[str, int]:
    """Python v0.1 CSV からハルトパターン（steps > 0）だけ C++ 表記で返す。"""
    path = _ensure_v01_csv(n)

    result: dict[str, int] = {}
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            steps = int(row["steps"])
            if steps <= 0:
                continue
            # Python 形式 [['0RA', None], ...] → C++ 表記 "0RA 0RH ..."
            pattern = ast.literal_eval(row["pattern"])
            tokens = [
                instr if instr is not None else "0RH"
                for row_ in pattern
                for instr in row_
            ]
            result[" ".join(tokens)] = steps
    return result


def _run_bb_search_csv(n: int) -> Path:
    """bb_search を --output-csv で実行し、CSV ファイルパスを返す。"""
    if not _BB_SEARCH.exists():
        pytest.skip(f"bb_search.exe が見つかりません: {_BB_SEARCH}")
    subprocess.run(
        [str(_BB_SEARCH), str(n), str(_MAX_STEPS), "--output-csv"],
        check=True,
        capture_output=True,
    )
    return _PROJECT_DIR / "results" / f"bb_{n}_patterns.csv"


def _run_bb_search(n: int) -> dict[str, int]:
    """bb_search を --output-csv で実行し、ハルトパターン辞書を返す。"""
    out_csv = _run_bb_search_csv(n)
    result: dict[str, int] = {}
    with open(out_csv, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            result[row["pattern"]] = int(row["steps"])
    return result


def _run_bb_search_full(n: int) -> dict[str, dict[str, str]]:
    """bb_search を --output-csv で実行し、全パターンの行辞書を返す。"""
    out_csv = _run_bb_search_csv(n)
    result: dict[str, dict[str, str]] = {}
    with open(out_csv, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            result[row["pattern"]] = dict(row)
    return result


_V01_CSV_COLUMNS = {"pattern", "steps", "elapsed_ms", "growth_pattern", "loop_shift"}


def _ensure_v01_csv(n: int) -> Path:
    """v0.1 CSV が最新フォーマットで存在することを確認し、パスを返す。"""
    path = _v01_csv_path(n)
    if path.exists():
        with open(path, newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            if reader.fieldnames and _V01_CSV_COLUMNS <= set(reader.fieldnames):
                return path
        path.unlink()
    print(f"\nv0.1 CSV を生成中: {path}", flush=True)
    _generate_v01_csv(n, path)
    return path


def _load_v01_loop_shifts(n: int) -> dict[str, int]:
    """Python v0.1 CSV からループパターン（steps < 0）の loop_shift 辞書を返す。"""
    path = _ensure_v01_csv(n)
    result: dict[str, int] = {}
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if int(row["steps"]) >= 0:
                continue
            pattern = ast.literal_eval(row["pattern"])
            tokens = [
                instr if instr is not None else "0RH"
                for row_ in pattern
                for instr in row_
            ]
            result[" ".join(tokens)] = int(row["loop_shift"])
    return result


def _assert_csv_match(v01: dict[str, int], cpp: dict[str, int]) -> None:
    """v0.1 の全ハルトパターンが C++ にも存在し、ステップ数が一致することを確認する。

    注: Python v0.1 は未到達 HALT 命令を枝刈りするため C++ より少ないパターンしか
    列挙しない。C++ が追加で見つけるパターン（未到達スロットが暗黙的に HALT の機械）
    は正当だがここでは検証対象外とする。
    """
    missing = {p for p in v01 if p not in cpp}
    assert not missing, f"v0.1 にあって C++ に無いパターン: {missing}"
    mismatches = {p: (cpp[p], v01[p]) for p in v01 if cpp[p] != v01[p]}
    assert not mismatches, f"ステップ数不一致 (cpp, v01): {mismatches}"


class TestCheckSameLoop:
    """C++ check_same_loop (BbMachineUnstoppableChecker) の正当性検証。

    unstoppable_checker_detected=true のパターンは Python でも定位置ループ
    (loop_shift=0) として検出されることを確認する。
    """

    def _assert_no_false_positives(self, n: int) -> None:
        v01_loops = _load_v01_loop_shifts(n)
        cpp_all = _run_bb_search_full(n)

        false_positives = []
        for pattern, row in cpp_all.items():
            if row.get("unstoppable_checker_detected") != "true":
                continue
            if pattern not in v01_loops:
                false_positives.append(
                    f"{pattern}: C++ unstoppable だが Python でループ未検出"
                )
            elif v01_loops[pattern] != 0:
                false_positives.append(
                    f"{pattern}: C++ unstoppable だが Python loop_shift={v01_loops[pattern]} (0 以外)"
                )

        assert not false_positives, "\n".join(false_positives)

    def test_2states(self) -> None:
        self._assert_no_false_positives(2)

    def test_3states(self) -> None:
        self._assert_no_false_positives(3)

    def test_4states(self) -> None:
        self._assert_no_false_positives(4)


class TestRegressionV01:
    def test_2states_same_as_v01(self) -> None:
        v01 = _load_v01_halting(2)
        cpp = _run_bb_search(2)
        _assert_csv_match(v01, cpp)

    def test_3states_same_as_v01(self) -> None:
        v01 = _load_v01_halting(3)
        cpp = _run_bb_search(3)
        _assert_csv_match(v01, cpp)

    def test_4states_same_as_v01(self) -> None:
        v01 = _load_v01_halting(4)
        cpp = _run_bb_search(4)
        _assert_csv_match(v01, cpp)
