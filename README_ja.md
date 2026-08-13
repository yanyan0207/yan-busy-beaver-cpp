# Busy Beaver (ビジービーバー) C++ 実装

ビジービーバー問題を C++23 で実装するプロジェクトです。Python スクリプトも併用します。

## ビジービーバーとは

ビジービーバー関数 BB(n) は、n 状態のチューリングマシンが停止するまでに書き込む 1 の最大個数を返します。この関数は計算不可能であり、数学・計算理論における重要な概念です。

| n | BB(n) |
|---|-------|
| 1 | 1     |
| 2 | 4     |
| 3 | 6     |
| 4 | 13    |
| 5 | 4098 以上（未確定） |

## 必要環境

以下を事前にインストールしてください。

| ツール | インストール方法 | 用途 |
|--------|----------------|------|
| Clang/LLVM | `winget install LLVM.LLVM` | C++ コンパイラ・clang-tidy |
| CMake | `winget install Kitware.CMake` | C++ ビルド |
| Ninja | `winget install Ninja-build.Ninja` | ビルドシステム（compile_commands.json 生成に必要） |
| uv | [公式手順](https://docs.astral.sh/uv/) | Python 管理 |

インストール後は **ターミナルを再起動**して PATH を反映させてください。

## セットアップ

```bash
# Python 依存のインストール（pre-commit・ruff・mypy・pylint 含む）
uv sync

# pre-commit フックの登録
uv run pre-commit install

# ビルドディレクトリの生成（compile_commands.json も出力）
# 既存の build/ がある場合は先に rm -rf build を実行
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_COMPILER=clang++
```

## ビルド・実行

```bash
cmake --build build
./build/bb
```

## テスト実行

```bash
./build/bb_test

# または CTest 経由
cd build && ctest
```

## コード品質

コミット時に pre-commit が自動で以下を実行します。

| ツール | 対象 | 役割 |
|--------|------|------|
| clang-format | C++ | コードフォーマット |
| clang-tidy | C++ | 静的解析（要ローカルインストール） |
| ruff | Python | フォーマット＋リント |

手動で全ファイルに対して実行する場合：

```powershell
uv run pre-commit run --all-files
```

## プロジェクト構成

```
busy_beaver_c/
├── CMakeLists.txt           # C++ ビルド設定
├── pyproject.toml           # Python 依存管理（uv）
├── .clang-format            # C++ フォーマット設定
├── .clang-tidy              # C++ リント設定
├── .pre-commit-config.yaml  # pre-commit フック設定
├── src/
│   └── main.cpp             # C++ メインプログラム
├── scripts/                 # Python スクリプト
└── tests/
    └── test_main.cpp        # C++ テスト
```

## ライセンス

MIT
