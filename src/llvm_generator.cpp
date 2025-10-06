#include "llvm_generator.h"
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <fstream>
#include <sstream>

namespace asmtowasm
{

  LLVMGenerator::LLVMGenerator()
      : context_(std::make_unique<llvm::LLVMContext>()),
        module_(std::make_unique<llvm::Module>("assembly_module", *context_)),
        builder_(std::make_unique<llvm::IRBuilder<>>(*context_))
  {
    registers_.clear();
    blocks_.clear();
    errorMessage_.clear();
  }

  bool LLVMGenerator::generateIR(const std::vector<Instruction> &instructions,
                                 const std::map<std::string, size_t> &labels)
  {
    // メイン関数を作成
    llvm::FunctionType *funcType = llvm::FunctionType::get(getIntType(), false);
    llvm::Function *mainFunc = llvm::Function::Create(funcType,
                                                      llvm::Function::ExternalLinkage,
                                                      "main",
                                                      *module_);

    llvm::BasicBlock *entryBlock = llvm::BasicBlock::Create(*context_, "entry", mainFunc);
    builder_->SetInsertPoint(entryBlock);

    // 各命令をLLVM IRに変換（すべて単一のBasicBlockで処理）
    for (size_t i = 0; i < instructions.size(); ++i)
    {
      if (!generateInstruction(instructions[i], i))
      {
        return false;
      }
    }

    // すべてのBasicBlockに終端命令があることを確認
    for (auto &block : *mainFunc)
    {
      if (!block.getTerminator())
      {
        llvm::IRBuilder<> tempBuilder(&block);
        tempBuilder.CreateRet(llvm::ConstantInt::get(getIntType(), 0));
      }
    }

    // IRの妥当性を検証
    std::string verifyError;
    llvm::raw_string_ostream verifyStream(verifyError);
    if (llvm::verifyFunction(*mainFunc, &verifyStream))
    {
      errorMessage_ = "IR検証エラー: " + verifyError;
      return false;
    }

    return true;
  }

  std::string LLVMGenerator::getIRString() const
  {
    std::string irString;
    llvm::raw_string_ostream stream(irString);
    module_->print(stream, nullptr);
    return irString;
  }

  bool LLVMGenerator::writeIRToFile(const std::string &filename)
  {
    std::ofstream file(filename);
    if (!file.is_open())
    {
      errorMessage_ = "ファイルを開けませんでした: " + filename;
      return false;
    }

    file << getIRString();
    file.close();
    return true;
  }

  llvm::Value *LLVMGenerator::getOrCreateRegister(const std::string &regName)
  {
    auto it = registers_.find(regName);
    if (it != registers_.end())
    {
      return it->second;
    }

    // 新しいレジスタを作成
    llvm::Value *reg = builder_->CreateAlloca(getIntType(), nullptr, regName);
    registers_[regName] = reg;
    return reg;
  }

  llvm::Value *LLVMGenerator::getOperandValue(const Operand &operand)
  {
    switch (operand.type)
    {
    case OperandType::REGISTER:
    {
      llvm::Value *reg = getOrCreateRegister(operand.value);
      return builder_->CreateLoad(getIntType(), reg, operand.value + "_val");
    }
    case OperandType::IMMEDIATE:
    {
      int value = std::stoi(operand.value);
      return llvm::ConstantInt::get(getIntType(), value);
    }
    case OperandType::MEMORY:
    {
      // メモリアドレスは簡単化のため、即値として扱う
      std::string addr = operand.value.substr(1, operand.value.length() - 2); // ()を除去
      int value = std::stoi(addr);
      return llvm::ConstantInt::get(getIntType(), value);
    }
    case OperandType::LABEL:
    {
      // ラベルはBasicBlockへの参照として扱う
      return getOrCreateBlock(operand.value);
    }
    default:
      return nullptr;
    }
  }

  bool LLVMGenerator::generateInstruction(const Instruction &instruction, size_t index)
  {
    switch (instruction.type)
    {
    case InstructionType::ADD:
    case InstructionType::SUB:
    case InstructionType::MUL:
    case InstructionType::DIV:
      return generateArithmeticInstruction(instruction);
    case InstructionType::MOV:
      return generateMoveInstruction(instruction);
    case InstructionType::CMP:
      return generateCompareInstruction(instruction);
    case InstructionType::JMP:
    case InstructionType::JE:
    case InstructionType::JNE:
    case InstructionType::JL:
    case InstructionType::JG:
      return generateJumpInstruction(instruction);
    case InstructionType::CALL:
      return generateCallInstruction(instruction);
    case InstructionType::RET:
      return generateReturnInstruction(instruction);
    case InstructionType::PUSH:
    case InstructionType::POP:
      return generateStackInstruction(instruction);
    case InstructionType::LABEL:
      // ラベルは無視（すべて単一のBasicBlockで処理）
      return true;
    default:
      errorMessage_ = "未対応の命令タイプ";
      return false;
    }
  }

  bool LLVMGenerator::generateArithmeticInstruction(const Instruction &instruction)
  {
    if (instruction.operands.size() < 2)
    {
      errorMessage_ = "算術命令には少なくとも2つのオペランドが必要です";
      return false;
    }

    llvm::Value *left = getOperandValue(instruction.operands[0]);
    llvm::Value *right = getOperandValue(instruction.operands[1]);

    if (!left || !right)
    {
      errorMessage_ = "オペランドの解析に失敗しました";
      return false;
    }

    llvm::Value *result = nullptr;
    switch (instruction.type)
    {
    case InstructionType::ADD:
      result = builder_->CreateAdd(left, right, "add");
      break;
    case InstructionType::SUB:
      result = builder_->CreateSub(left, right, "sub");
      break;
    case InstructionType::MUL:
      result = builder_->CreateMul(left, right, "mul");
      break;
    case InstructionType::DIV:
      result = builder_->CreateSDiv(left, right, "div");
      break;
    default:
      return false;
    }

    // 結果を最初のオペランド（通常はレジスタ）に格納
    if (instruction.operands[0].type == OperandType::REGISTER)
    {
      llvm::Value *reg = getOrCreateRegister(instruction.operands[0].value);
      builder_->CreateStore(result, reg);
    }

    return true;
  }

  bool LLVMGenerator::generateMoveInstruction(const Instruction &instruction)
  {
    if (instruction.operands.size() != 2)
    {
      errorMessage_ = "MOV命令には2つのオペランドが必要です";
      return false;
    }

    llvm::Value *source = getOperandValue(instruction.operands[1]);
    if (!source)
    {
      errorMessage_ = "ソースオペランドの解析に失敗しました";
      return false;
    }

    if (instruction.operands[0].type == OperandType::REGISTER)
    {
      llvm::Value *dest = getOrCreateRegister(instruction.operands[0].value);
      builder_->CreateStore(source, dest);
    }
    else
    {
      errorMessage_ = "MOV命令のデスティネーションはレジスタである必要があります";
      return false;
    }

    return true;
  }

  bool LLVMGenerator::generateCompareInstruction(const Instruction &instruction)
  {
    if (instruction.operands.size() != 2)
    {
      errorMessage_ = "CMP命令には2つのオペランドが必要です";
      return false;
    }

    llvm::Value *left = getOperandValue(instruction.operands[0]);
    llvm::Value *right = getOperandValue(instruction.operands[1]);

    if (!left || !right)
    {
      errorMessage_ = "CMP命令のオペランドの解析に失敗しました";
      return false;
    }

    // 比較結果を一時的に保存（実際の実装では、フラグレジスタとして管理）
    builder_->CreateICmpEQ(left, right, "cmp_result");

    return true;
  }

  bool LLVMGenerator::generateJumpInstruction(const Instruction &instruction)
  {
    if (instruction.operands.size() != 1)
    {
      errorMessage_ = "ジャンプ命令には1つのオペランドが必要です";
      return false;
    }

    llvm::BasicBlock *targetBlock = getOrCreateBlock(instruction.operands[0].value);
    if (!targetBlock)
    {
      errorMessage_ = "ジャンプ先のラベルが見つかりません: " + instruction.operands[0].value;
      return false;
    }

    switch (instruction.type)
    {
    case InstructionType::JMP:
      builder_->CreateBr(targetBlock);
      break;
    case InstructionType::JE:
    case InstructionType::JNE:
    case InstructionType::JL:
    case InstructionType::JG:
      // 条件分岐は簡単化のため、無条件ジャンプとして実装
      builder_->CreateBr(targetBlock);
      break;
    default:
      return false;
    }

    return true;
  }

  bool LLVMGenerator::generateCallInstruction(const Instruction &instruction)
  {
    if (instruction.operands.size() != 1)
    {
      errorMessage_ = "CALL命令には1つのオペランドが必要です";
      return false;
    }

    const std::string funcName = instruction.operands[0].value;

    // 既存関数を取得、なければ i32() -> i32 の外部関数として作成
    llvm::Function *callee = module_->getFunction(funcName);
    if (!callee)
    {
      llvm::FunctionType *funcType = llvm::FunctionType::get(getIntType(), false);
      callee = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, funcName, *module_);
    }

    if (!callee)
    {
      errorMessage_ = "関数が見つかりません: " + funcName;
      return false;
    }

    builder_->CreateCall(callee);
    return true;
  }

  bool LLVMGenerator::generateReturnInstruction(const Instruction &instruction)
  {
    if (instruction.operands.empty())
    {
      builder_->CreateRet(llvm::ConstantInt::get(getIntType(), 0));
    }
    else
    {
      llvm::Value *retValue = getOperandValue(instruction.operands[0]);
      if (!retValue)
      {
        errorMessage_ = "RET命令のオペランドの解析に失敗しました";
        return false;
      }
      builder_->CreateRet(retValue);
    }
    return true;
  }

  bool LLVMGenerator::generateStackInstruction(const Instruction &instruction)
  {
    errorMessage_ = "スタック操作命令は未実装です";
    return false;
  }

  llvm::BasicBlock *LLVMGenerator::getOrCreateBlock(const std::string &labelName)
  {
    auto it = blocks_.find(labelName);
    if (it != blocks_.end())
    {
      return it->second;
    }

    // 新しいBasicBlockを作成
    llvm::Function *currentFunc = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock *block = llvm::BasicBlock::Create(*context_, labelName, currentFunc);
    blocks_[labelName] = block;
    return block;
  }

  llvm::Type *LLVMGenerator::getIntType() const
  {
    return llvm::Type::getInt32Ty(*context_);
  }

} // namespace asmtowasm
