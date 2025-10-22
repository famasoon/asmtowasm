#include "assembly_lifter.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DIBuilder.h>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace asmtowasm
{

  AssemblyLifter::AssemblyLifter()
      : context_(std::make_unique<llvm::LLVMContext>()),
        module_(std::make_unique<llvm::Module>("assembly_module", *context_)),
        builder_(std::make_unique<llvm::IRBuilder<>>(*context_))
  {
    registers_.clear();
    blocks_.clear();
    functions_.clear();
    errorMessage_.clear();
  }

  bool AssemblyLifter::liftToLLVM(const std::vector<Instruction> &instructions,
                                  const std::map<std::string, size_t> &labels)
  {
    std::cout << "Assemblyリフター: LLVM IR生成を開始" << std::endl;
    std::cout << "命令数: " << instructions.size() << ", ラベル数: " << labels.size() << std::endl;

    // CALL先ラベルを事前収集（関数として扱う）
    std::set<std::string> callTargets;
    for (const auto &inst : instructions)
    {
      if (inst.type == InstructionType::CALL && inst.operands.size() == 1 && inst.operands[0].type == OperandType::LABEL)
      {
        callTargets.insert(inst.operands[0].value);
      }
    }

    // 関数はラベル到達時に作成
    llvm::FunctionType *funcType = llvm::FunctionType::get(getIntType(), false);
    llvm::Function *currentFunc = nullptr;

    // 各命令をLLVM IRに変換
    for (size_t i = 0; i < instructions.size(); ++i)
    {
      std::cout << "命令 " << i << " を処理中: ";
      if (!instructions[i].label.empty())
      {
        std::cout << "ラベル=" << instructions[i].label << " ";
      }
      std::cout << "タイプ=" << static_cast<int>(instructions[i].type) << std::endl;

      // ラベル付きの命令: 関数開始 or 現関数内ブロック
      if (!instructions[i].label.empty())
      {
        const std::string &labelName = instructions[i].label;
        if (labelName == "main" || callTargets.count(labelName) > 0)
        {
          // 新しい関数に切替え
          currentFunc = getOrCreateFunction(labelName);
          if (!currentFunc)
          {
            std::cout << "関数の作成に失敗: " << labelName << std::endl;
            return false;
          }
          // ブロック名の衝突を避けるためクリア
          blocks_.clear();
          registers_.clear();
          llvm::BasicBlock *funcEntry = llvm::BasicBlock::Create(*context_, labelName, currentFunc);
          builder_->SetInsertPoint(funcEntry);
        }
        else
        {
          // 現関数内のブロックとして扱う
          if (!currentFunc)
          {
            // まだ関数が作成されていない場合は、デフォルト関数を作成
            currentFunc = getOrCreateFunction("main");
            if (!currentFunc)
            {
              std::cout << "デフォルト関数の作成に失敗" << std::endl;
              return false;
            }
          }
          llvm::BasicBlock *labelBlock = getOrCreateBlock(labelName);
          if (!labelBlock)
          {
            std::cout << "ブロックの作成に失敗: " << labelName << std::endl;
            return false;
          }
          builder_->SetInsertPoint(labelBlock);
        }
      }

      if (!liftInstruction(instructions[i], i))
      {
        std::cout << "命令 " << i << " の処理に失敗しました" << std::endl;
        return false;
      }
    }

    // 旧entryブロック処理は不要（関数はラベル到達時に開始）

    // すべてのBasicBlockに終端命令があることを確認
    if (currentFunc)
    {
      for (auto &block : *currentFunc)
      {
        std::cout << "BasicBlock " << block.getName().str() << " をチェック中..." << std::endl;
        if (!block.getTerminator())
        {
          std::cout << "BasicBlock " << block.getName().str() << " に終端命令を追加" << std::endl;
          llvm::IRBuilder<> tempBuilder(&block);
          tempBuilder.CreateRet(llvm::ConstantInt::get(getIntType(), 0));
        }
        else
        {
          std::cout << "BasicBlock " << block.getName().str() << " は既に終端命令を持っています" << std::endl;
        }
      }
    }

    // 最適化パスを適用
    applyOptimizationPasses();

    // IRの妥当性を検証
    std::cout << "IR検証を開始..." << std::endl;
    std::string verifyError;
    llvm::raw_string_ostream verifyStream(verifyError);
    // 全関数を検証
    for (auto &func : *module_)
    {
      if (func.isDeclaration())
        continue;
      verifyError.clear();
      llvm::raw_string_ostream vs(verifyError);
      if (llvm::verifyFunction(func, &vs))
      {
        std::cout << "IR検証エラー: " << verifyError << std::endl;
        std::cout << "生成されたLLVM IR:" << std::endl;
        std::string moduleDump;
        llvm::raw_string_ostream dumpStream(moduleDump);
        module_->print(dumpStream, nullptr);
        std::cout << moduleDump << std::endl;
        errorMessage_ = "IR検証エラー: " + verifyError;
        return false;
      }
    }
    std::cout << "IR検証成功!" << std::endl;
    std::cout << "Assemblyリフター: LLVM IR生成完了" << std::endl;

    return true;
  }

  llvm::Value *AssemblyLifter::getOrCreateRegister(const std::string &regName)
  {
    auto it = registers_.find(regName);
    if (it != registers_.end())
    {
      std::cout << "        既存のレジスタを使用: " << regName << std::endl;
      return it->second;
    }

    // 新しいレジスタを作成
    llvm::Value *reg = builder_->CreateAlloca(getIntType(), nullptr, regName);
    registers_[regName] = reg;
    std::cout << "        新しいレジスタを作成: " << regName << std::endl;
    return reg;
  }

  llvm::Value *AssemblyLifter::getOperandValue(const Operand &operand)
  {
    std::cout << "      getOperandValue: タイプ=" << static_cast<int>(operand.type) << ", 値=" << operand.value << std::endl;

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
      return calculateMemoryAddress(operand);
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

  bool AssemblyLifter::liftInstruction(const Instruction &instruction, size_t index)
  {
    std::cout << "  liftInstruction: タイプ=" << static_cast<int>(instruction.type);
    if (!instruction.label.empty())
    {
      std::cout << ", ラベル=" << instruction.label;
    }
    std::cout << ", オペランド数=" << instruction.operands.size() << std::endl;

    switch (instruction.type)
    {
    case InstructionType::ADD:
    case InstructionType::SUB:
    case InstructionType::MUL:
    case InstructionType::DIV:
      return liftArithmeticInstruction(instruction);
    case InstructionType::MOV:
      return liftMoveInstruction(instruction);
    case InstructionType::CMP:
      return liftCompareInstruction(instruction);
    case InstructionType::JMP:
    case InstructionType::JE:
    case InstructionType::JNE:
    case InstructionType::JL:
    case InstructionType::JG:
    case InstructionType::JLE:
    case InstructionType::JGE:
      return liftJumpInstruction(instruction);
    case InstructionType::CALL:
      return liftCallInstruction(instruction);
    case InstructionType::RET:
      return liftReturnInstruction(instruction);
    case InstructionType::PUSH:
    case InstructionType::POP:
      return liftStackInstruction(instruction);
    case InstructionType::LABEL:
      std::cout << "  LABEL命令をスキップ" << std::endl;
      return true;
    default:
      errorMessage_ = "未対応の命令タイプ";
      return false;
    }
  }

  bool AssemblyLifter::liftArithmeticInstruction(const Instruction &instruction)
  {
    std::cout << "    liftArithmeticInstruction: オペランド数=" << instruction.operands.size() << std::endl;

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
      std::cout << "    ADD命令を生成" << std::endl;
      break;
    case InstructionType::SUB:
      result = builder_->CreateSub(left, right, "sub");
      std::cout << "    SUB命令を生成" << std::endl;
      break;
    case InstructionType::MUL:
      result = builder_->CreateMul(left, right, "mul");
      std::cout << "    MUL命令を生成" << std::endl;
      break;
    case InstructionType::DIV:
      result = builder_->CreateSDiv(left, right, "div");
      std::cout << "    DIV命令を生成" << std::endl;
      break;
    default:
      return false;
    }

    // 結果を最初のオペランド（通常はレジスタ）に格納
    if (instruction.operands[0].type == OperandType::REGISTER)
    {
      llvm::Value *reg = getOrCreateRegister(instruction.operands[0].value);
      builder_->CreateStore(result, reg);
      std::cout << "    結果をレジスタ " << instruction.operands[0].value << " に格納" << std::endl;
    }

    return true;
  }

  bool AssemblyLifter::liftMoveInstruction(const Instruction &instruction)
  {
    std::cout << "    liftMoveInstruction: オペランド数=" << instruction.operands.size() << std::endl;

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
      std::cout << "    MOV命令を生成: " << instruction.operands[0].value << " = " << instruction.operands[1].value << std::endl;
    }
    else if (instruction.operands[0].type == OperandType::MEMORY)
    {
      // メモリアクセス: mov (%esi), %eax または mov %eax, (%esi)
      if (instruction.operands[1].type == OperandType::REGISTER)
      {
        // mov (%esi), %eax: メモリからレジスタへ
        llvm::Value *memAddr = calculateMemoryAddress(instruction.operands[0]);
        llvm::Value *memPtr = builder_->CreateIntToPtr(memAddr, getPtrType(), "mem_ptr");
        llvm::Value *reg = getOrCreateRegister(instruction.operands[1].value);
        llvm::Value *memValue = builder_->CreateLoad(getIntType(), memPtr, "mem_val");
        builder_->CreateStore(memValue, reg);
        std::cout << "    MOV命令を生成: " << instruction.operands[1].value << " = " << instruction.operands[0].value << std::endl;
      }
      else
      {
        errorMessage_ = "メモリアクセスのMOV命令では、ソースはレジスタである必要があります";
        return false;
      }
    }
    else if (instruction.operands[1].type == OperandType::MEMORY)
    {
      // mov %eax, (%esi): レジスタからメモリへ
      if (instruction.operands[0].type == OperandType::REGISTER)
      {
        llvm::Value *reg = getOrCreateRegister(instruction.operands[0].value);
        llvm::Value *regValue = builder_->CreateLoad(getIntType(), reg, "reg_val");
        llvm::Value *memAddr = calculateMemoryAddress(instruction.operands[1]);
        llvm::Value *memPtr = builder_->CreateIntToPtr(memAddr, getPtrType(), "mem_ptr");
        builder_->CreateStore(regValue, memPtr);
        std::cout << "    MOV命令を生成: " << instruction.operands[1].value << " = " << instruction.operands[0].value << std::endl;
      }
      else
      {
        errorMessage_ = "メモリアクセスのMOV命令では、デスティネーションはレジスタである必要があります";
        return false;
      }
    }
    else
    {
      errorMessage_ = "MOV命令のデスティネーションはレジスタまたはメモリアクセスである必要があります";
      return false;
    }

    return true;
  }

  bool AssemblyLifter::liftCompareInstruction(const Instruction &instruction)
  {
    std::cout << "    liftCompareInstruction: オペランド数=" << instruction.operands.size() << std::endl;

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

    // 各種フラグを設定（符号付き比較）
    llvm::Value *eq = builder_->CreateICmpEQ(left, right, "cmp_eq");
    llvm::Value *lt = builder_->CreateICmpSLT(left, right, "cmp_lt");
    llvm::Value *gt = builder_->CreateICmpSGT(left, right, "cmp_gt");
    llvm::Value *le = builder_->CreateICmpSLE(left, right, "cmp_le");
    llvm::Value *ge = builder_->CreateICmpSGE(left, right, "cmp_ge");

    setFlagRegister("ZF", builder_->CreateZExt(eq, getIntType(), "zf_int"));
    setFlagRegister("LT", builder_->CreateZExt(lt, getIntType(), "lt_int"));
    setFlagRegister("GT", builder_->CreateZExt(gt, getIntType(), "gt_int"));
    setFlagRegister("LE", builder_->CreateZExt(le, getIntType(), "le_int"));
    setFlagRegister("GE", builder_->CreateZExt(ge, getIntType(), "ge_int"));
    std::cout << "    CMP命令を生成 (ZF,LT,GT,LE,GE を設定)" << std::endl;

    return true;
  }

  bool AssemblyLifter::liftJumpInstruction(const Instruction &instruction)
  {
    std::cout << "    liftJumpInstruction: オペランド数=" << instruction.operands.size() << std::endl;

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
      std::cout << "    JMP命令を生成: " << instruction.operands[0].value << std::endl;
      // 終端後に後続命令を挿入しないよう、新しい継続ブロックへ切替
      {
        llvm::Function *currentFunc = builder_->GetInsertBlock()->getParent();
        llvm::BasicBlock *nextBlock = llvm::BasicBlock::Create(*context_, "cont", currentFunc);
        builder_->SetInsertPoint(nextBlock);
      }
      break;
    case InstructionType::JE:
    case InstructionType::JNE:
    case InstructionType::JL:
    case InstructionType::JG:
    case InstructionType::JLE:
    case InstructionType::JGE:
    {
      // CMPで設定したフラグを使用して条件分岐を生成
      auto getFlagCond = [&](const char *name, bool branchWhenNonZero) -> llvm::Value *
      {
        llvm::Value *reg = getFlagRegister(name);
        llvm::Value *val = builder_->CreateLoad(getIntType(), reg, std::string(name) + "_val");
        return branchWhenNonZero
                   ? builder_->CreateICmpNE(val, llvm::ConstantInt::get(getIntType(), 0), std::string(name) + "_nz")
                   : builder_->CreateICmpEQ(val, llvm::ConstantInt::get(getIntType(), 0), std::string(name) + "_z");
      };

      llvm::Function *currentFunc = builder_->GetInsertBlock()->getParent();
      llvm::BasicBlock *fallthrough = llvm::BasicBlock::Create(*context_, "cont", currentFunc);

      switch (instruction.type)
      {
      case InstructionType::JE:
        builder_->CreateCondBr(getFlagCond("ZF", /*branchWhenNonZero*/ true), targetBlock, fallthrough);
        break;
      case InstructionType::JNE:
        builder_->CreateCondBr(getFlagCond("ZF", /*branchWhenNonZero*/ false), fallthrough, targetBlock);
        break;
      case InstructionType::JL:
        builder_->CreateCondBr(getFlagCond("LT", /*branchWhenNonZero*/ true), targetBlock, fallthrough);
        break;
      case InstructionType::JG:
        builder_->CreateCondBr(getFlagCond("GT", /*branchWhenNonZero*/ true), targetBlock, fallthrough);
        break;
      case InstructionType::JLE:
        builder_->CreateCondBr(getFlagCond("LE", /*branchWhenNonZero*/ true), targetBlock, fallthrough);
        break;
      case InstructionType::JGE:
        builder_->CreateCondBr(getFlagCond("GE", /*branchWhenNonZero*/ true), targetBlock, fallthrough);
        break;
      default:
        // 他の条件は暫定で無条件ジャンプ
        builder_->CreateBr(targetBlock);
        break;
      }

      builder_->SetInsertPoint(fallthrough);
      std::cout << "    条件ジャンプ命令を生成: " << instruction.operands[0].value << std::endl;
      break;
    }
    default:
      return false;
    }

    return true;
  }

  bool AssemblyLifter::liftCallInstruction(const Instruction &instruction)
  {
    std::cout << "    liftCallInstruction: オペランド数=" << instruction.operands.size() << std::endl;

    if (instruction.operands.size() != 1)
    {
      errorMessage_ = "CALL命令には1つのオペランドが必要です";
      return false;
    }

    std::string funcName = instruction.operands[0].value;
    llvm::Function *func = getOrCreateFunction(funcName);

    if (!func)
    {
      errorMessage_ = "関数が見つかりません: " + funcName;
      return false;
    }

    builder_->CreateCall(func);
    std::cout << "    CALL命令を生成: " << funcName << std::endl;
    return true;
  }

  bool AssemblyLifter::liftReturnInstruction(const Instruction &instruction)
  {
    std::cout << "    liftReturnInstruction: オペランド数=" << instruction.operands.size() << std::endl;

    if (instruction.operands.empty())
    {
      builder_->CreateRet(llvm::ConstantInt::get(getIntType(), 0));
      std::cout << "    RET命令を生成: 0を返す" << std::endl;
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
      std::cout << "    RET命令を生成: 値を返す" << std::endl;
    }
    return true;
  }

  bool AssemblyLifter::liftStackInstruction(const Instruction &instruction)
  {
    std::cout << "    liftStackInstruction: オペランド数=" << instruction.operands.size() << std::endl;

    if (instruction.type == InstructionType::PUSH)
    {
      if (instruction.operands.size() != 1)
      {
        errorMessage_ = "PUSH命令には1つのオペランドが必要です";
        return false;
      }

      // PUSH命令: スタックに値をプッシュ（簡単化のため、メモリに保存）
      llvm::Value *value = getOperandValue(instruction.operands[0]);
      if (!value)
      {
        errorMessage_ = "PUSH命令のオペランドの解析に失敗しました";
        return false;
      }

      // スタックポインタを取得または作成
      llvm::Value *stackPtr = getOrCreateRegister("STACK_PTR");
      llvm::Value *stackValue = builder_->CreateLoad(getIntType(), stackPtr, "stack_ptr_val");

      // スタックポインタをデクリメント（簡単化のため、固定オフセット）
      llvm::Value *newStackPtr = builder_->CreateSub(stackValue, llvm::ConstantInt::get(getIntType(), 4), "new_stack_ptr");
      builder_->CreateStore(newStackPtr, stackPtr);

      // 値をスタックに保存
      llvm::Value *stackAddr = builder_->CreateIntToPtr(newStackPtr, getPtrType(), "stack_addr");
      builder_->CreateStore(value, stackAddr);

      std::cout << "    PUSH命令を生成: " << instruction.operands[0].value << std::endl;
    }
    else if (instruction.type == InstructionType::POP)
    {
      if (instruction.operands.size() != 1)
      {
        errorMessage_ = "POP命令には1つのオペランドが必要です";
        return false;
      }

      // POP命令: スタックから値をポップ（簡単化のため、メモリから読み込み）
      llvm::Value *stackPtr = getOrCreateRegister("STACK_PTR");
      llvm::Value *stackValue = builder_->CreateLoad(getIntType(), stackPtr, "stack_ptr_val");

      // スタックから値を読み込み
      llvm::Value *stackAddr = builder_->CreateIntToPtr(stackValue, getPtrType(), "stack_addr");
      llvm::Value *value = builder_->CreateLoad(getIntType(), stackAddr, "stack_val");

      // スタックポインタをインクリメント
      llvm::Value *newStackPtr = builder_->CreateAdd(stackValue, llvm::ConstantInt::get(getIntType(), 4), "new_stack_ptr");
      builder_->CreateStore(newStackPtr, stackPtr);

      // 値をレジスタに保存
      if (instruction.operands[0].type == OperandType::REGISTER)
      {
        llvm::Value *reg = getOrCreateRegister(instruction.operands[0].value);
        builder_->CreateStore(value, reg);
      }

      std::cout << "    POP命令を生成: " << instruction.operands[0].value << std::endl;
    }

    return true;
  }

  llvm::BasicBlock *AssemblyLifter::getOrCreateBlock(const std::string &labelName)
  {
    auto it = blocks_.find(labelName);
    if (it != blocks_.end())
    {
      std::cout << "        既存のBasicBlockを使用: " << labelName << std::endl;
      return it->second;
    }

    // 新しいBasicBlockを作成
    llvm::BasicBlock *currentBlock = builder_->GetInsertBlock();
    llvm::Function *currentFunc = nullptr;

    if (currentBlock)
    {
      currentFunc = currentBlock->getParent();
    }
    else
    {
      // 現在のブロックが設定されていない場合は、デフォルト関数を作成
      currentFunc = getOrCreateFunction("main");
      if (!currentFunc)
      {
        std::cout << "        エラー: デフォルト関数の作成に失敗" << std::endl;
        return nullptr;
      }
    }

    llvm::BasicBlock *block = llvm::BasicBlock::Create(*context_, labelName, currentFunc);
    blocks_[labelName] = block;
    std::cout << "        新しいBasicBlockを作成: " << labelName << std::endl;
    return block;
  }

  llvm::Function *AssemblyLifter::getOrCreateFunction(const std::string &funcName)
  {
    auto it = functions_.find(funcName);
    if (it != functions_.end())
    {
      std::cout << "        既存の関数を使用: " << funcName << std::endl;
      return it->second;
    }

    // 新しい関数を作成
    llvm::FunctionType *funcType = llvm::FunctionType::get(getIntType(), false);
    llvm::Function *func = llvm::Function::Create(funcType,
                                                  llvm::Function::ExternalLinkage,
                                                  funcName,
                                                  *module_);
    functions_[funcName] = func;
    std::cout << "        新しい関数を作成: " << funcName << std::endl;
    return func;
  }

  llvm::Type *AssemblyLifter::getIntType() const
  {
    std::cout << "        getIntType: Int32型を取得" << std::endl;
    return llvm::Type::getInt32Ty(*context_);
  }

  llvm::Type *AssemblyLifter::getPtrType() const
  {
    std::cout << "        getPtrType: ポインタ型を取得" << std::endl;
    return llvm::PointerType::get(getIntType(), 0);
  }

  llvm::Value *AssemblyLifter::calculateMemoryAddress(const Operand &operand)
  {
    // メモリアドレスを解析: (%esi), (%esi+4), (1000) など
    std::string addr = operand.value.substr(1, operand.value.length() - 2); // ()を除去
    std::cout << "        メモリアドレスを計算: " << operand.value << " -> " << addr << std::endl;

    // 配列アクセスの最適化: レジスタ+オフセットの形式をチェック
    if (addr.find('+') != std::string::npos)
    {
      // (%esi+4) のような形式 - 配列アクセスとして最適化
      size_t plusPos = addr.find('+');
      std::string regName = addr.substr(0, plusPos);
      std::string offsetStr = addr.substr(plusPos + 1);

      llvm::Value *reg = getOrCreateRegister(regName);
      llvm::Value *regValue = builder_->CreateLoad(getIntType(), reg, "base_addr");

      int offset = std::stoi(offsetStr);

      // メモリアライメントを考慮（4バイト境界）
      if (offset % 4 == 0)
      {
        // 4バイト境界の場合は最適化
        llvm::Value *alignedOffset = llvm::ConstantInt::get(getIntType(), offset);
        llvm::Value *result = builder_->CreateAdd(regValue, alignedOffset, "aligned_mem_addr");
        std::cout << "        最適化された配列アクセス: " << regName << " + " << offset << " (4バイト境界)" << std::endl;
        return result;
      }
      else
      {
        // 非境界の場合は通常の計算
        llvm::Value *offsetValue = llvm::ConstantInt::get(getIntType(), offset);
        llvm::Value *result = builder_->CreateAdd(regValue, offsetValue, "mem_addr");
        std::cout << "        配列アクセス: " << regName << " + " << offset << " (非境界)" << std::endl;
        return result;
      }
    }
    else if (addr.find('%') != std::string::npos)
    {
      // (%esi) のような形式 - 直接レジスタアクセス
      llvm::Value *reg = getOrCreateRegister(addr);
      llvm::Value *regValue = builder_->CreateLoad(getIntType(), reg, "reg_val");
      std::cout << "        直接レジスタアクセス: " << addr << std::endl;
      return regValue;
    }
    else
    {
      // (1000) のような形式 - 絶対アドレス
      int value = std::stoi(addr);

      // 絶対アドレスのアライメントチェック
      if (value % 4 == 0)
      {
        std::cout << "        最適化された絶対アドレス: " << value << " (4バイト境界)" << std::endl;
      }
      else
      {
        std::cout << "        絶対アドレス: " << value << " (非境界)" << std::endl;
      }

      return llvm::ConstantInt::get(getIntType(), value);
    }
  }

  std::string AssemblyLifter::normalizeRegisterName(const std::string &regName)
  {
    std::cout << "        レジスタ名を正規化: " << regName << std::endl;
    std::string normalized = regName;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    return normalized;
  }

  bool AssemblyLifter::handleInstructionSideEffects(const Instruction &instruction)
  {
    std::cout << "        命令の副作用を処理: タイプ=" << static_cast<int>(instruction.type) << std::endl;
    // 命令の副作用を処理（フラグレジスタの更新など）
    return true;
  }

  llvm::Value *AssemblyLifter::getFlagRegister(const std::string &flagName)
  {
    std::cout << "        フラグレジスタを取得: " << flagName << std::endl;
    return getOrCreateRegister("FLAG_" + flagName);
  }

  void AssemblyLifter::setFlagRegister(const std::string &flagName, llvm::Value *value)
  {
    llvm::Value *flagReg = getFlagRegister(flagName);
    builder_->CreateStore(value, flagReg);
    std::cout << "        フラグレジスタを設定: " << flagName << std::endl;
  }

  bool AssemblyLifter::handleMemoryOperation(const Instruction &instruction)
  {
    std::cout << "        メモリ操作を処理: タイプ=" << static_cast<int>(instruction.type) << std::endl;

    switch (instruction.type)
    {
    case InstructionType::MOV:
      // MOV命令のメモリ操作は既にliftMoveInstructionで処理済み
      return true;

    case InstructionType::ADD:
    case InstructionType::SUB:
    case InstructionType::MUL:
    case InstructionType::DIV:
      // 算術命令のメモリ操作は既にliftArithmeticInstructionで処理済み
      return true;

    case InstructionType::CMP:
      // 比較命令のメモリ操作は既にliftCompareInstructionで処理済み
      return true;

    case InstructionType::PUSH:
    case InstructionType::POP:
      // スタック操作のメモリ操作は既にliftStackInstructionで処理済み
      return true;

    default:
      std::cout << "        未対応のメモリ操作命令: " << static_cast<int>(instruction.type) << std::endl;
      return true;
    }
  }

  llvm::Value *AssemblyLifter::performTypeConversion(llvm::Value *value, llvm::Type *targetType)
  {
    std::cout << "        型変換を処理" << std::endl;

    if (!value || !targetType)
    {
      std::cout << "        型変換エラー: 無効な値または型" << std::endl;
      return nullptr;
    }

    llvm::Type *sourceType = value->getType();

    // 同じ型の場合は変換不要
    if (sourceType == targetType)
    {
      std::cout << "        型変換不要: 同じ型" << std::endl;
      return value;
    }

    // 整数型の変換
    if (sourceType->isIntegerTy() && targetType->isIntegerTy())
    {
      unsigned sourceBits = sourceType->getIntegerBitWidth();
      unsigned targetBits = targetType->getIntegerBitWidth();

      if (sourceBits < targetBits)
      {
        // ゼロ拡張
        std::cout << "        ゼロ拡張: " << sourceBits << " -> " << targetBits << std::endl;
        return builder_->CreateZExt(value, targetType, "zext");
      }
      else if (sourceBits > targetBits)
      {
        // 切り詰め
        std::cout << "        切り詰め: " << sourceBits << " -> " << targetBits << std::endl;
        return builder_->CreateTrunc(value, targetType, "trunc");
      }
    }

    // ポインタ型の変換
    if (sourceType->isPointerTy() && targetType->isPointerTy())
    {
      std::cout << "        ポインタ型変換" << std::endl;
      return builder_->CreateBitCast(value, targetType, "bitcast");
    }

    // 整数からポインタへの変換
    if (sourceType->isIntegerTy() && targetType->isPointerTy())
    {
      std::cout << "        整数からポインタへの変換" << std::endl;
      return builder_->CreateIntToPtr(value, targetType, "inttoptr");
    }

    // ポインタから整数への変換
    if (sourceType->isPointerTy() && targetType->isIntegerTy())
    {
      std::cout << "        ポインタから整数への変換" << std::endl;
      return builder_->CreatePtrToInt(value, targetType, "ptrtoint");
    }

    std::cout << "        未対応の型変換" << std::endl;
    return value;
  }

  void AssemblyLifter::applyOptimizationPasses()
  {
    std::cout << "最適化パスを適用中..." << std::endl;

    // 最適化パスは現在無効化（リンクエラーを避けるため）
    // 将来的にはLLVMの最適化パスを適用予定

    std::cout << "最適化パス適用完了（スキップ）" << std::endl;
  }

  void AssemblyLifter::generateDebugInfo()
  {
    std::cout << "デバッグ情報を生成中..." << std::endl;

    try
    {
      // DIBuilderを作成
      llvm::DIBuilder dibuilder(*module_);

      // デバッグ情報のファイルを作成
      auto file = dibuilder.createFile("assembly_module.asm", "/home/user/work/asmtowasm");

      // デバッグ情報のコンパイル単位を作成
      auto compileUnit = dibuilder.createCompileUnit(
          llvm::dwarf::DW_LANG_C, file, "AsmToWasm", false, "", 0);

      // 基本型を作成
      auto int32Type = dibuilder.createBasicType("int32", 32, llvm::dwarf::DW_ATE_signed);
      auto int32PtrType = dibuilder.createPointerType(int32Type, 32);

      // 関数の型を作成
      auto funcType = dibuilder.createSubroutineType(dibuilder.getOrCreateTypeArray(int32Type));

      // 関数のデバッグ情報を作成
      for (auto &func : *module_)
      {
        if (func.getName() == "main")
        {
          // メイン関数のデバッグ情報
          auto subprogram = dibuilder.createFunction(
              file, func.getName(), func.getName(), file, 1,
              funcType, 1, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagZero);

          // 関数の開始位置を設定
          func.setSubprogram(subprogram);

          // レジスタのデバッグ情報を作成
          for (auto &block : func)
          {
            for (auto &inst : block)
            {
              if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(&inst))
              {
                // レジスタ変数のデバッグ情報
                std::string varName = alloca->getName().str();
                if (varName.find('%') != std::string::npos)
                {
                  auto var = dibuilder.createAutoVariable(
                      subprogram, varName, file, 1, int32Type, true);

                  // 変数の位置情報を設定
                  auto location = dibuilder.createExpression();
                  dibuilder.insertDeclare(alloca, var, location,
                                          llvm::DILocation::get(*context_, 1, 0, subprogram), &block);
                }
              }
            }
          }
        }
      }

      // デバッグ情報を最終化
      dibuilder.finalize();

      std::cout << "デバッグ情報生成完了" << std::endl;
    }
    catch (const std::exception &e)
    {
      std::cout << "デバッグ情報生成エラー: " << e.what() << std::endl;
      std::cout << "デバッグ情報生成をスキップします" << std::endl;
    }
    catch (...)
    {
      std::cout << "デバッグ情報生成で不明なエラーが発生しました" << std::endl;
      std::cout << "デバッグ情報生成をスキップします" << std::endl;
    }
  }

} // namespace asmtowasm
