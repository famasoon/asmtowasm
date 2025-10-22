#pragma once

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
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <memory>
#include <map>
#include <string>
#include <vector>

namespace asmtowasm
{

  // WebAssembly型
  enum class WasmType
  {
    I32, // 32ビット整数
    I64, // 64ビット整数
    F32, // 32ビット浮動小数点
    F64, // 64ビット浮動小数点
    VOID // 戻り値なし
  };

  // WebAssembly命令
  enum class WasmOpcode
  {
    // 制御フロー
    BLOCK,
    LOOP,
    IF,
    ELSE,
    END,
    BR,
    BR_IF,
    BR_TABLE,
    RETURN,
    CALL,
    CALL_INDIRECT,

    // パラメータとローカル
    DROP,
    SELECT,
    GET_LOCAL,
    SET_LOCAL,
    TEE_LOCAL,
    GET_GLOBAL,
    SET_GLOBAL,

    // メモリ
    I32_LOAD,
    I64_LOAD,
    F32_LOAD,
    F64_LOAD,
    I32_LOAD8_S,
    I32_LOAD8_U,
    I32_LOAD16_S,
    I32_LOAD16_U,
    I64_LOAD8_S,
    I64_LOAD8_U,
    I64_LOAD16_S,
    I64_LOAD16_U,
    I64_LOAD32_S,
    I64_LOAD32_U,
    I32_STORE,
    I64_STORE,
    F32_STORE,
    F64_STORE,
    I32_STORE8,
    I32_STORE16,
    I64_STORE8,
    I64_STORE16,
    I64_STORE32,
    MEMORY_SIZE,
    MEMORY_GROW,

    // 定数
    I32_CONST,
    I64_CONST,
    F32_CONST,
    F64_CONST,

    // 比較演算
    I32_EQZ,
    I32_EQ,
    I32_NE,
    I32_LT_S,
    I32_LT_U,
    I32_GT_S,
    I32_GT_U,
    I32_LE_S,
    I32_LE_U,
    I32_GE_S,
    I32_GE_U,
    I64_EQZ,
    I64_EQ,
    I64_NE,
    I64_LT_S,
    I64_LT_U,
    I64_GT_S,
    I64_GT_U,
    I64_LE_S,
    I64_LE_U,
    I64_GE_S,
    I64_GE_U,
    F32_EQ,
    F32_NE,
    F32_LT,
    F32_GT,
    F32_LE,
    F32_GE,
    F64_EQ,
    F64_NE,
    F64_LT,
    F64_GT,
    F64_LE,
    F64_GE,

    // 算術演算
    I32_CLZ,
    I32_CTZ,
    I32_POPCNT,
    I32_ADD,
    I32_SUB,
    I32_MUL,
    I32_DIV_S,
    I32_DIV_U,
    I32_REM_S,
    I32_REM_U,
    I32_AND,
    I32_OR,
    I32_XOR,
    I32_SHL,
    I32_SHR_S,
    I32_SHR_U,
    I32_ROTL,
    I32_ROTR,
    I64_CLZ,
    I64_CTZ,
    I64_POPCNT,
    I64_ADD,
    I64_SUB,
    I64_MUL,
    I64_DIV_S,
    I64_DIV_U,
    I64_REM_S,
    I64_REM_U,
    I64_AND,
    I64_OR,
    I64_XOR,
    I64_SHL,
    I64_SHR_S,
    I64_SHR_U,
    I64_ROTL,
    I64_ROTR,
    F32_ABS,
    F32_NEG,
    F32_CEIL,
    F32_FLOOR,
    F32_TRUNC,
    F32_NEAREST,
    F32_SQRT,
    F32_ADD,
    F32_SUB,
    F32_MUL,
    F32_DIV,
    F32_MIN,
    F32_MAX,
    F32_COPYSIGN,
    F64_ABS,
    F64_NEG,
    F64_CEIL,
    F64_FLOOR,
    F64_TRUNC,
    F64_NEAREST,
    F64_SQRT,
    F64_ADD,
    F64_SUB,
    F64_MUL,
    F64_DIV,
    F64_MIN,
    F64_MAX,
    F64_COPYSIGN,

    // 型変換
    I32_WRAP_I64,
    I32_TRUNC_F32_S,
    I32_TRUNC_F32_U,
    I32_TRUNC_F64_S,
    I32_TRUNC_F64_U,
    I64_EXTEND_I32_S,
    I64_EXTEND_I32_U,
    I64_TRUNC_F32_S,
    I64_TRUNC_F32_U,
    I64_TRUNC_F64_S,
    I64_TRUNC_F64_U,
    F32_CONVERT_I32_S,
    F32_CONVERT_I32_U,
    F32_CONVERT_I64_S,
    F32_CONVERT_I64_U,
    F32_DEMOTE_F64,
    F64_CONVERT_I32_S,
    F64_CONVERT_I32_U,
    F64_CONVERT_I64_S,
    F64_CONVERT_I64_U,
    F64_PROMOTE_F32,
    I32_REINTERPRET_F32,
    I64_REINTERPRET_F64,
    F32_REINTERPRET_I32,
    F64_REINTERPRET_I64,

    // その他
    UNREACHABLE,
    NOP
  };

  // WebAssembly命令
  struct WasmInstruction
  {
    WasmOpcode opcode;
    std::vector<uint64_t> operands;

    WasmInstruction(WasmOpcode op) : opcode(op) {}
    WasmInstruction(WasmOpcode op, uint64_t operand) : opcode(op)
    {
      operands.push_back(operand);
    }
    WasmInstruction(WasmOpcode op, const std::vector<uint64_t> &ops) : opcode(op), operands(ops) {}
  };

  // WebAssembly関数
  struct WasmFunction
  {
    std::string name;
    std::vector<WasmType> params;
    std::vector<WasmType> locals;
    WasmType returnType;
    std::vector<WasmInstruction> instructions;

    WasmFunction(const std::string &n) : name(n), returnType(WasmType::VOID) {}
  };

  // WebAssemblyモジュール
  struct WasmModule
  {
    std::vector<WasmFunction> functions;
    std::map<std::string, uint32_t> functionIndices;
    uint32_t memorySize;
    uint32_t memoryMaxSize;

    WasmModule() : memorySize(1), memoryMaxSize(65536) {}
  };

  // WebAssembly生成器クラス
  class WasmGenerator
  {
  public:
    WasmGenerator();
    ~WasmGenerator() = default;

    // LLVM IRからWebAssemblyを生成
    bool generateWasm(llvm::Module *module);

    // WebAssemblyバイナリをファイルに出力
    bool writeWasmToFile(const std::string &filename);

    // WebAssemblyテキスト形式をファイルに出力
    bool writeWastToFile(const std::string &filename);

    // WebAssemblyテキスト形式を文字列として取得
    std::string getWastString() const;

    // エラーメッセージを取得
    const std::string &getErrorMessage() const { return errorMessage_; }

  private:
    WasmModule wasmModule_;
    std::string errorMessage_;
    std::map<llvm::Function *, uint32_t> functionMap_;
    std::map<llvm::Value *, uint32_t> localMap_;

    // LLVM型をWebAssembly型に変換
    WasmType convertLLVMType(llvm::Type *type);

    // LLVM関数をWebAssembly関数に変換
    bool convertFunction(llvm::Function *func);

    // LLVM基本ブロックをWebAssembly命令に変換
    bool convertBasicBlock(llvm::BasicBlock *block, WasmFunction &wasmFunc);

    // LLVM命令をWebAssembly命令に変換
    bool convertInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc);

    // 算術演算命令を変換
    bool convertArithmeticInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc);

    // 比較命令を変換
    bool convertCompareInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc);

    // 分岐命令を変換
    bool convertBranchInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc);

    // 関数呼び出し命令を変換
    bool convertCallInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc);

    // 戻り命令を変換
    bool convertReturnInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc);

    // ロード/ストア命令を変換
    bool convertMemoryInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc);
    bool convertIntToPtrInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc);
    bool convertPtrToIntInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc);
    bool convertBitCastInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc);

    // LLVM値をWebAssemblyローカルインデックスに変換
    uint32_t assignLocalIndex(llvm::Value *value, WasmType type, WasmFunction &wasmFunc);
    uint32_t getLocalIndex(llvm::Value *value);

    // WebAssemblyバイナリを生成
    std::vector<uint8_t> generateBinary() const;

    // WebAssemblyテキスト形式を生成
    std::string generateWast() const;

    // 関数のテキスト形式を生成
    std::string generateFunctionWast(const WasmFunction &func) const;

    // 命令のテキスト形式を生成
    std::string generateInstructionWast(const WasmInstruction &inst) const;

    // WebAssembly型の文字列表現を取得
    std::string getWasmTypeString(WasmType type) const;

    // WebAssemblyオペコードの文字列表現を取得
    std::string getWasmOpcodeString(WasmOpcode opcode) const;
  };

} // namespace asmtowasm
