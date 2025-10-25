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
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <map>
#include <string>
#include <vector>

namespace asmtowasm
{

  // Assemblyリフタークラス
  class AssemblyLifter
  {
  public:
    AssemblyLifter();
    ~AssemblyLifter() = default;

    // AssemblyコードからLLVM IRを生成
    bool liftToLLVM(const std::vector<Instruction> &instructions,
                    const std::map<std::string, size_t> &labels);

    // LLVMモジュールを取得
    llvm::Module *getModule() const { return module_.get(); }

    // エラーメッセージを取得
    const std::string &getErrorMessage() const { return errorMessage_; }

  private:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    std::map<std::string, llvm::Value *> registers_;    // レジスタ名 -> LLVM Value
    std::map<std::string, llvm::BasicBlock *> blocks_;  // ラベル名 -> BasicBlock
    std::map<std::string, llvm::Function *> functions_; // 関数名 -> LLVM Function
    std::string errorMessage_;

    // レジスタの値を取得または作成
    llvm::Value *getOrCreateRegister(const std::string &regName);

    // オペランドからLLVM Valueを取得
    llvm::Value *getOperandValue(const Operand &operand);

    // 命令をLLVM IRに変換
    bool liftInstruction(const Instruction &instruction, size_t index);

    // 算術命令をリフト
    bool liftArithmeticInstruction(const Instruction &instruction);

    // 移動命令をリフト
    bool liftMoveInstruction(const Instruction &instruction);

    // 比較命令をリフト
    bool liftCompareInstruction(const Instruction &instruction);

    // ジャンプ命令をリフト
    bool liftJumpInstruction(const Instruction &instruction);

    // 関数呼び出し命令をリフト
    bool liftCallInstruction(const Instruction &instruction);

    // 戻り命令をリフト
    bool liftReturnInstruction(const Instruction &instruction);

    // スタック操作命令をリフト
    bool liftStackInstruction(const Instruction &instruction);

    // ラベルからBasicBlockを取得または作成
    llvm::BasicBlock *getOrCreateBlock(const std::string &labelName);

    // 関数を取得または作成
    llvm::Function *getOrCreateFunction(const std::string &funcName);

    // 整数型を取得
    llvm::Type *getIntType() const;

    // ポインタ型を取得
    llvm::Type *getPtrType() const;

    // メモリアドレスを計算
    llvm::Value *calculateMemoryAddress(const Operand &operand);

    // フラグレジスタを管理
    llvm::Value *getFlagRegister(const std::string &flagName);
    void setFlagRegister(const std::string &flagName, llvm::Value *value);

    // 最適化パスを適用
    void applyOptimizationPasses();
  };

} // namespace asmtowasm
