#pragma once

#include "assembly_parser.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <memory>
#include <map>
#include <string>

namespace asmtowasm
{

  // LLVM IR生成器クラス
  class LLVMGenerator
  {
  public:
    LLVMGenerator();
    ~LLVMGenerator() = default;

    // Assembly命令からLLVM IRを生成
    bool generateIR(const std::vector<Instruction> &instructions,
                    const std::map<std::string, size_t> &labels);

    // LLVM IRを文字列として出力
    std::string getIRString() const;

    // LLVM IRをファイルに出力
    bool writeIRToFile(const std::string &filename);

    // エラーメッセージを取得
    const std::string &getErrorMessage() const { return errorMessage_; }

    // LLVMモジュールを取得
    llvm::Module *getModule() const { return module_.get(); }

  private:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    std::map<std::string, llvm::Value *> registers_;   // レジスタ名 -> LLVM Value
    std::map<std::string, llvm::BasicBlock *> blocks_; // ラベル名 -> BasicBlock
    std::string errorMessage_;

    // レジスタの値を取得または作成
    llvm::Value *getOrCreateRegister(const std::string &regName);

    // オペランドからLLVM Valueを取得
    llvm::Value *getOperandValue(const Operand &operand);

    // 命令をLLVM IRに変換
    bool generateInstruction(const Instruction &instruction, size_t index);

    // 算術命令を生成
    bool generateArithmeticInstruction(const Instruction &instruction);

    // 移動命令を生成
    bool generateMoveInstruction(const Instruction &instruction);

    // 比較命令を生成
    bool generateCompareInstruction(const Instruction &instruction);

    // ジャンプ命令を生成
    bool generateJumpInstruction(const Instruction &instruction);

    // 関数呼び出し命令を生成
    bool generateCallInstruction(const Instruction &instruction);

    // 戻り命令を生成
    bool generateReturnInstruction(const Instruction &instruction);

    // スタック操作命令を生成
    bool generateStackInstruction(const Instruction &instruction);

    // ラベルからBasicBlockを取得または作成
    llvm::BasicBlock *getOrCreateBlock(const std::string &labelName);

    // 整数型を取得
    llvm::Type *getIntType() const;
  };

} // namespace asmtowasm
