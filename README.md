# AsmToWasm - Assembly to LLVM IR/Wasm Converter

AssemblyコードをLLVM IR/Wasmに変換するC++ツールです。

## 概要

このプロジェクトは、簡易的なAssembly言語のコードをLLVM IR（Intermediate Representation）に変換し、オプションでWebAssembly(WAT/WASM)にも出力するコンパイラです。教育目的やプロトタイプ開発に適しています。

## 機能

- Assemblyコードのパース
- LLVM IR生成（`LLVMGenerator`/`AssemblyLifter`）
- WebAssemblyテキスト/バイナリ生成（`--wast`/`--wasm`）
- 基本的な算術演算（ADD, SUB, MUL, DIV）
- データ移動（MOV）
- 比較（CMP）と条件分岐（JMP, JE/JZ, JNE/JNZ, JL, JG, JLE, JGE）
- 関数呼び出し（CALL, RET）
- スタック操作（PUSH, POP）
- レジスタとメモリアドレスのサポート（簡易）

## 必要な環境

- CMake 3.15以上
- LLVM 10.0以上
- C++17対応コンパイラ（GCC 7+ または Clang 5+）

## ビルド方法

```bash
# プロジェクトディレクトリに移動
cd /home/user/work/asmtowasm

# ビルドディレクトリを作成
mkdir build
cd build

# CMakeでビルド設定（C++のみ使用）
cmake ..

# ビルド実行
make
```

## 使用方法

```bash
# 基本: LLVM IRを標準出力（または -o でファイル）
./asmtowasm /home/user/work/asmtowasm/examples/simple_add.asm
./asmtowasm -o /home/user/work/asmtowasm/build/out.ll /home/user/work/asmtowasm/examples/arithmetic.asm

# 高度なリフター（推奨）でIR生成ログを詳細表示
./asmtowasm --lifter /home/user/work/asmtowasm/examples/function_calls.asm

# WebAssemblyテキスト/バイナリ出力（入力は最後の位置引数）
./asmtowasm --wast /home/user/work/asmtowasm/build/out.wat /home/user/work/asmtowasm/examples/conditional_jump.asm
./asmtowasm --wasm /home/user/work/asmtowasm/build/out.wasm /home/user/work/asmtowasm/examples/loop_example.asm

# ヘルプ
./asmtowasm --help
```

## サポートされているAssembly構文

### 命令形式
```
[ラベル:] 命令 [オペランド1] [, オペランド2] [; コメント]
```

### オペランドの種類
- **レジスタ**: `%eax`, `%ebx`, `%ecx`, `%edx`, `%esi`, `%edi`
- **即値**: `10`, `-5`, `0x1A`
- **メモリアドレス**: `(%eax)`, `(%ebx+4)`
- **ラベル**: `start`, `loop`, `end`

### サポートされている命令

#### 算術演算
- `ADD dst, src` - 加算
- `SUB dst, src` - 減算
- `MUL dst, src` - 乗算
- `DIV dst, src` - 除算

#### データ移動
- `MOV dst, src` - データ移動

#### 比較と分岐
- `CMP op1, op2` - 比較（符号付きで`ZF, LT, GT, LE, GE`を内部フラグに保存）
- `JMP label` - 無条件ジャンプ
- `JE/JZ label` - 等しい（`ZF!=0`）
- `JNE/JNZ label` - 等しくない（`ZF==0`）
- `JL label` - 小さい（`LT!=0`）
- `JG label` - 大きい（`GT!=0`）
- `JLE label` - 以下（`LE!=0`）
- `JGE label` - 以上（`GE!=0`）

#### 関数制御
- `CALL function` - 関数呼び出し
- `RET [value]` - 関数から戻る

#### スタック操作
- `PUSH src` - スタックにプッシュ
- `POP dst` - スタックからポップ

## サンプルコード

### 簡単な加算
```assembly
start:
    mov %eax, 10      # %eax = 10
    mov %ebx, 20      # %ebx = 20
    add %eax, %ebx    # %eax = %eax + %ebx
    ret               # 関数から戻る
```

### 条件分岐
```assembly
compare:
    mov %eax, 10
    mov %ebx, 15
    cmp %eax, %ebx
    jl less_than
    jg greater_than
    je equal

less_than:
    mov %ecx, 1
    jmp end

greater_than:
    mov %ecx, 2
    jmp end

equal:
    mov %ecx, 0

end:
    ret
```

## 出力例（WAT・新記法）

```wat
(module
  (func $main (result i32) (local $0 i32) (local $1 i32)
    i32.const 10
    local.get 0
    i32.store
    i32.const 20
    local.get 1
    i32.store
    ;; ... 省略 ...
    i32.const 0
    return)
)
```

## プロジェクト構造

```
asmtowasm/
├── CMakeLists.txt          # CMakeビルド設定
├── README.md              # このファイル
├── include/               # ヘッダーファイル
│   ├── assembly_parser.h  # Assemblyパーサー
│   ├── llvm_generator.h   # LLVM IR生成（シンプル）
│   ├── assembly_lifter.h  # LLVM IR生成（高度）
│   └── wasm_generator.h   # Wasm生成
├── src/                   # ソースファイル
│   ├── main.cpp           # メインアプリケーション
│   ├── assembly_parser.cpp # Assemblyパーサー
│   ├── assembly_lifter.cpp # 高度なLLVM IR生成
│   ├── llvm_generator.cpp  # シンプルなLLVM IR生成
│   └── wasm_generator.cpp  # Wasm生成
└── examples/              # サンプルAssemblyファイル
    ├── simple_add.asm     # 簡単な加算
    ├── arithmetic.asm     # 算術演算
    └── conditional_jump.asm # 条件分岐
```

## 制限事項

- 教育目的の簡易版です
- 条件分岐は内部フラグ（ZF/LT/GT/LE/GE）ベースの簡易実装（キャリーフラグ等は未対応）
- メモリ/スタックは簡略モデル（実機ABIとは異なる）
- Wasm生成は最小限（ブロック構造の最適化は未実装）
- 最適化は行っていません

## 今後の拡張予定

- より多くの命令/フラグのサポート（AND/OR/XOR/SHL/SHR、CF/SF/OFなど）
- エラーハンドリングの改善
- 最適化パスの追加
- デバッグ情報の生成
- より複雑なメモリアドレッシングモードのサポート

## トラブルシューティング

- CMakeが`clang-cl`を検出して失敗する場合は、C++のみを使う設定になっているか確認し、キャッシュ削除後に再設定してください。
  - `CMakeLists.txt`は`project(AsmToWasm LANGUAGES CXX)`を使用
  - 例: `rm -rf build/CMakeCache.txt build/CMakeFiles && CXX=/usr/bin/clang++ cmake ..`

## ライセンス

このプロジェクトはMITライセンスの下で公開されています。

## 貢献

バグ報告や機能追加の提案は、GitHubのIssuesページでお願いします。
