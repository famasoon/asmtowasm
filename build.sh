#!/bin/bash

# AsmToWasm ビルドスクリプト

echo "AsmToWasm プロジェクトをビルド中..."

# ビルドディレクトリを作成
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# CMakeでビルド設定
echo "CMakeでビルド設定中..."
cmake ..

# ビルド実行
echo "ビルド実行中..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo "ビルド成功!"
    echo ""
    echo "使用方法:"
    echo "  ./asmtowasm examples/simple_add.asm"
    echo "  ./asmtowasm -o output.ll examples/arithmetic.asm"
    echo "  ./asmtowasm --help"
else
    echo "ビルドエラーが発生しました。"
    exit 1
fi
