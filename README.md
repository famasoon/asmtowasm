# AsmToWasm - Assembly to LLVM IR Converter

AssemblyコードをLLVM IRに変換するC++ツールです。

## 概要

このプロジェクトは、簡易的なAssembly言語のコードをLLVM IR（Intermediate Representation）に変換するコンパイラです。教育目的やプロトタイプ開発に適しています。

## 機能

- Assemblyコードのパース
- 基本的な算術演算（ADD, SUB, MUL, DIV）
- 移動命令（MOV）
- 比較命令（CMP）
- 条件分岐（JMP, JE, JNE, JL, JG）
- 関数呼び出し（CALL, RET）
- スタック操作（PUSH, POP）
- レジスタとメモリアドレスのサポート

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

# CMakeでビルド設定
cmake ..

# ビルド実行
make
```

## 使用方法

```bash
# 基本的な使用方法
./asmtowasm examples/simple_add.asm

# 出力ファイルを指定
./asmtowasm -o output.ll examples/arithmetic.asm

# ヘルプを表示
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
- `CMP op1, op2` - 比較
- `JMP label` - 無条件ジャンプ
- `JE label` - 等しい場合のジャンプ
- `JNE label` - 等しくない場合のジャンプ
- `JL label` - 小さい場合のジャンプ
- `JG label` - 大きい場合のジャンプ

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

## 出力例

入力Assemblyコードが以下のLLVM IRに変換されます：

```llvm
; ModuleID = 'assembly_module'
source_filename = "assembly_module"

define i32 @main() {
entry:
  %eax = alloca i32
  %ebx = alloca i32
  store i32 10, i32* %eax
  store i32 20, i32* %ebx
  %eax_val = load i32, i32* %eax
  %ebx_val = load i32, i32* %ebx
  %add = add i32 %eax_val, %ebx_val
  store i32 %add, i32* %eax
  ret i32 0
}
```

## プロジェクト構造

```
asmtowasm/
├── CMakeLists.txt          # CMakeビルド設定
├── README.md              # このファイル
├── include/               # ヘッダーファイル
│   ├── assembly_parser.h  # Assemblyパーサー
│   └── llvm_generator.h   # LLVM IR生成器
├── src/                   # ソースファイル
│   ├── main.cpp           # メインアプリケーション
│   ├── assembly_parser.cpp # Assemblyパーサー実装
│   └── llvm_generator.cpp  # LLVM IR生成器実装
└── examples/              # サンプルAssemblyファイル
    ├── simple_add.asm     # 簡単な加算
    ├── arithmetic.asm     # 算術演算
    └── conditional_jump.asm # 条件分岐
```

## 制限事項

- 現在の実装は教育目的の簡易版です
- 一部の命令は完全に実装されていません
- エラーハンドリングは基本的なもののみ
- 最適化は行われません

## 今後の拡張予定

- より多くの命令のサポート
- エラーハンドリングの改善
- 最適化パスの追加
- デバッグ情報の生成
- より複雑なメモリアドレッシングモードのサポート

## ライセンス

このプロジェクトはMITライセンスの下で公開されています。

## 貢献

バグ報告や機能追加の提案は、GitHubのIssuesページでお願いします。
