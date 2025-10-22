#include "wasm_generator.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/Support/raw_ostream.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace asmtowasm
{

  WasmGenerator::WasmGenerator()
  {
    wasmModule_ = WasmModule();
    functionMap_.clear();
    localMap_.clear();
    errorMessage_.clear();
  }

  bool WasmGenerator::generateWasm(llvm::Module *module)
  {
    if (!module)
    {
      errorMessage_ = "無効なLLVMモジュールです";
      return false;
    }

    // 関数マップを初期化
    functionMap_.clear();
    localMap_.clear();

    uint32_t funcIndex = 0;
    for (auto &func : *module)
    {
      if (!func.isDeclaration())
      {
        functionMap_[&func] = funcIndex++;
      }
    }

    // 各関数を変換
    for (auto &func : *module)
    {
      if (!func.isDeclaration())
      {
        if (!convertFunction(&func))
        {
          return false;
        }
      }
    }

    return true;
  }

  bool WasmGenerator::writeWasmToFile(const std::string &filename)
  {
    std::vector<uint8_t> binary = generateBinary();

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
      errorMessage_ = "ファイルを開けませんでした: " + filename;
      return false;
    }

    file.write(reinterpret_cast<const char *>(binary.data()), binary.size());
    file.close();

    return true;
  }

  bool WasmGenerator::writeWastToFile(const std::string &filename)
  {
    std::string wast = generateWast();

    std::ofstream file(filename);
    if (!file.is_open())
    {
      errorMessage_ = "ファイルを開けませんでした: " + filename;
      return false;
    }

    file << wast;
    file.close();

    return true;
  }

  std::string WasmGenerator::getWastString() const
  {
    return generateWast();
  }

  WasmType WasmGenerator::convertLLVMType(llvm::Type *type)
  {
    if (type->isIntegerTy(32))
    {
      return WasmType::I32;
    }
    else if (type->isIntegerTy(64))
    {
      return WasmType::I64;
    }
    else if (type->isFloatTy())
    {
      return WasmType::F32;
    }
    else if (type->isDoubleTy())
    {
      return WasmType::F64;
    }
    else if (type->isVoidTy())
    {
      return WasmType::VOID;
    }

    // デフォルトはI32
    return WasmType::I32;
  }

  bool WasmGenerator::convertFunction(llvm::Function *func)
  {
    localMap_.clear();

    WasmFunction wasmFunc(func->getName().str());

    // パラメータを変換
    for (auto &arg : func->args())
    {
      wasmFunc.params.push_back(convertLLVMType(arg.getType()));
    }

    // 戻り値の型を設定
    wasmFunc.returnType = convertLLVMType(func->getReturnType());

    // ローカル変数を収集
    for (auto &block : *func)
    {
      for (auto &inst : block)
      {
        if (llvm::isa<llvm::AllocaInst>(inst))
        {
          assignLocalIndex(&inst, WasmType::I32, wasmFunc);
        }
      }
    }

    // SSA値に対応するローカルを事前確保
    for (auto &block : *func)
    {
      for (auto &inst : block)
      {
        if (llvm::isa<llvm::BinaryOperator>(inst) ||
            llvm::isa<llvm::CmpInst>(inst) ||
            llvm::isa<llvm::ZExtInst>(inst) ||
            llvm::isa<llvm::IntToPtrInst>(inst) ||
            llvm::isa<llvm::PtrToIntInst>(inst) ||
            llvm::isa<llvm::BitCastInst>(inst))
        {
          WasmType resultType = convertLLVMType(inst.getType());
          assignLocalIndex(&inst, resultType, wasmFunc);
        }
      }
    }

    // 基本ブロックを変換
    for (auto &block : *func)
    {
      if (!convertBasicBlock(&block, wasmFunc))
      {
        return false;
      }
    }

    wasmModule_.functions.push_back(wasmFunc);
    wasmModule_.functionIndices[func->getName().str()] = wasmModule_.functions.size() - 1;

    return true;
  }

  bool WasmGenerator::convertBasicBlock(llvm::BasicBlock *block, WasmFunction &wasmFunc)
  {
    for (auto &inst : *block)
    {
      if (!convertInstruction(&inst, wasmFunc))
      {
        return false;
      }
    }
    return true;
  }

  bool WasmGenerator::convertInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc)
  {
    auto &instructions = wasmFunc.instructions;

    if (llvm::isa<llvm::BinaryOperator>(inst))
    {
      return convertArithmeticInstruction(inst, wasmFunc);
    }
    else if (llvm::isa<llvm::CmpInst>(inst))
    {
      return convertCompareInstruction(inst, wasmFunc);
    }
    else if (llvm::isa<llvm::BranchInst>(inst))
    {
      return convertBranchInstruction(inst, wasmFunc);
    }
    else if (llvm::isa<llvm::CallInst>(inst))
    {
      return convertCallInstruction(inst, wasmFunc);
    }
    else if (llvm::isa<llvm::ReturnInst>(inst))
    {
      return convertReturnInstruction(inst, wasmFunc);
    }
    else if (llvm::isa<llvm::LoadInst>(inst) || llvm::isa<llvm::StoreInst>(inst))
    {
      return convertMemoryInstruction(inst, wasmFunc);
    }
    else if (llvm::isa<llvm::AllocaInst>(inst))
    {
      // Allocaは既にローカル変数として処理済み
      return true;
    }
    else if (llvm::isa<llvm::IntToPtrInst>(inst))
    {
      return convertIntToPtrInstruction(inst, wasmFunc);
    }
    else if (llvm::isa<llvm::PtrToIntInst>(inst))
    {
      return convertPtrToIntInstruction(inst, wasmFunc);
    }
    else if (llvm::isa<llvm::BitCastInst>(inst))
    {
      return convertBitCastInstruction(inst, wasmFunc);
    }
    else if (llvm::isa<llvm::ZExtInst>(inst))
    {
      // ゼロ拡張（例: i1 -> i32）を変換
      llvm::ZExtInst *zext = llvm::cast<llvm::ZExtInst>(inst);
      llvm::Value *op = zext->getOperand(0);

      // 代表的ケース: 比較結果(i1)のゼロ拡張
      if (llvm::isa<llvm::CmpInst>(op))
      {
        // 比較をWasmに変換（結果がスタックにi32で積まれる）
        if (!convertCompareInstruction(llvm::cast<llvm::Instruction>(op), wasmFunc))
        {
          return false;
        }
      }
      else if (llvm::isa<llvm::ConstantInt>(op))
      {
        auto *constInt = llvm::cast<llvm::ConstantInt>(op);
        instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
      }
      else if (llvm::isa<llvm::LoadInst>(op))
      {
        auto *load = llvm::cast<llvm::LoadInst>(op);
        uint32_t localIdx = getLocalIndex(load->getPointerOperand());
        instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
      }
      else
      {
        // 最低限の対応: その他は未対応として扱う
        errorMessage_ = "未対応のZExtオペランド";
        return false;
      }

      // zextの結果をローカルに保存
      uint32_t resultIdx = assignLocalIndex(inst, convertLLVMType(inst->getType()), wasmFunc);
      instructions.push_back(WasmInstruction(WasmOpcode::SET_LOCAL, resultIdx));
      return true;
    }

    // 未対応の命令
    errorMessage_ = "未対応のLLVM命令: " + std::string(inst->getOpcodeName());
    return false;
  }

  bool WasmGenerator::convertArithmeticInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc)
  {
    auto &instructions = wasmFunc.instructions;

    llvm::BinaryOperator *binOp = llvm::cast<llvm::BinaryOperator>(inst);

    // オペランドを取得
    llvm::Value *lhs = binOp->getOperand(0);
    llvm::Value *rhs = binOp->getOperand(1);

    // 左オペランドをスタックにプッシュ
    if (llvm::isa<llvm::ConstantInt>(lhs))
    {
      llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(lhs);
      instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
    }
    else if (llvm::isa<llvm::LoadInst>(lhs))
    {
      llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(lhs);
      uint32_t localIdx = getLocalIndex(load->getPointerOperand());
      instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
    }

    // 右オペランドをスタックにプッシュ
    if (llvm::isa<llvm::ConstantInt>(rhs))
    {
      llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(rhs);
      instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
    }
    else if (llvm::isa<llvm::LoadInst>(rhs))
    {
      llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(rhs);
      uint32_t localIdx = getLocalIndex(load->getPointerOperand());
      instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
    }

    // 演算命令を追加
    switch (binOp->getOpcode())
    {
    case llvm::Instruction::Add:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_ADD));
      break;
    case llvm::Instruction::Sub:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_SUB));
      break;
    case llvm::Instruction::Mul:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_MUL));
      break;
    case llvm::Instruction::SDiv:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_DIV_S));
      break;
    case llvm::Instruction::UDiv:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_DIV_U));
      break;
    default:
      errorMessage_ = "未対応の算術演算: " + std::string(binOp->getOpcodeName());
      return false;
    }

    // 結果をローカル変数に格納
    uint32_t resultIdx = assignLocalIndex(inst, convertLLVMType(inst->getType()), wasmFunc);
    instructions.push_back(WasmInstruction(WasmOpcode::SET_LOCAL, resultIdx));

    return true;
  }

  bool WasmGenerator::convertCompareInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc)
  {
    auto &instructions = wasmFunc.instructions;

    llvm::CmpInst *cmpInst = llvm::cast<llvm::CmpInst>(inst);

    // オペランドを取得
    llvm::Value *lhs = cmpInst->getOperand(0);
    llvm::Value *rhs = cmpInst->getOperand(1);

    // 左オペランドをスタックにプッシュ
    if (llvm::isa<llvm::ConstantInt>(lhs))
    {
      llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(lhs);
      instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
    }
    else if (llvm::isa<llvm::LoadInst>(lhs))
    {
      llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(lhs);
      uint32_t localIdx = getLocalIndex(load->getPointerOperand());
      instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
    }

    // 右オペランドをスタックにプッシュ
    if (llvm::isa<llvm::ConstantInt>(rhs))
    {
      llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(rhs);
      instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
    }
    else if (llvm::isa<llvm::LoadInst>(rhs))
    {
      llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(rhs);
      uint32_t localIdx = getLocalIndex(load->getPointerOperand());
      instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
    }

    // 比較命令を追加
    switch (cmpInst->getPredicate())
    {
    case llvm::CmpInst::ICMP_EQ:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_EQ));
      break;
    case llvm::CmpInst::ICMP_NE:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_NE));
      break;
    case llvm::CmpInst::ICMP_SLT:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_LT_S));
      break;
    case llvm::CmpInst::ICMP_ULT:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_LT_U));
      break;
    case llvm::CmpInst::ICMP_SGT:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_GT_S));
      break;
    case llvm::CmpInst::ICMP_UGT:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_GT_U));
      break;
    case llvm::CmpInst::ICMP_SLE:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_LE_S));
      break;
    case llvm::CmpInst::ICMP_ULE:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_LE_U));
      break;
    case llvm::CmpInst::ICMP_SGE:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_GE_S));
      break;
    case llvm::CmpInst::ICMP_UGE:
      instructions.push_back(WasmInstruction(WasmOpcode::I32_GE_U));
      break;
    default:
      errorMessage_ = "未対応の比較演算";
      return false;
    }

    return true;
  }

  bool WasmGenerator::convertBranchInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc)
  {
    auto &instructions = wasmFunc.instructions;

    llvm::BranchInst *branchInst = llvm::cast<llvm::BranchInst>(inst);

    if (branchInst->isUnconditional())
    {
      // 無条件ブランチの場合、WebAssemblyでは単純にスキップ
      // 実際のWebAssemblyでは、ブロック構造を使用する必要がある
      std::cout << "        無条件ブランチをスキップ（WebAssemblyではブロック構造を使用）" << std::endl;
      return true;
    }
    else
    {
      // 条件分岐
      llvm::Value *condition = branchInst->getCondition();
      llvm::BasicBlock *trueTarget = branchInst->getSuccessor(0);
      llvm::BasicBlock *falseTarget = branchInst->getSuccessor(1);

      // 条件をスタックにプッシュ
      if (llvm::isa<llvm::LoadInst>(condition))
      {
        llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(condition);
        uint32_t localIdx = getLocalIndex(load->getPointerOperand());
        instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
      }

      instructions.push_back(WasmInstruction(WasmOpcode::BR_IF, 0));
    }

    return true;
  }

  bool WasmGenerator::convertCallInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc)
  {
    auto &instructions = wasmFunc.instructions;

    llvm::CallInst *callInst = llvm::cast<llvm::CallInst>(inst);

    // 引数をスタックにプッシュ
    for (auto &arg : callInst->args())
    {
      if (llvm::isa<llvm::ConstantInt>(arg))
      {
        llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(arg);
        instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
      }
    }

    // 関数呼び出し
    llvm::Function *calledFunc = callInst->getCalledFunction();
    if (calledFunc)
    {
      auto it = functionMap_.find(calledFunc);
      if (it != functionMap_.end())
      {
        instructions.push_back(WasmInstruction(WasmOpcode::CALL, it->second));
      }
    }

    return true;
  }

  bool WasmGenerator::convertReturnInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc)
  {
    auto &instructions = wasmFunc.instructions;

    llvm::ReturnInst *retInst = llvm::cast<llvm::ReturnInst>(inst);

    if (retInst->getNumOperands() > 0)
    {
      llvm::Value *retValue = retInst->getOperand(0);
      if (llvm::isa<llvm::ConstantInt>(retValue))
      {
        llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(retValue);
        instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
      }
    }

    instructions.push_back(WasmInstruction(WasmOpcode::RETURN));
    return true;
  }

  bool WasmGenerator::convertMemoryInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc)
  {
    auto &instructions = wasmFunc.instructions;

    if (llvm::isa<llvm::LoadInst>(inst))
    {
      llvm::LoadInst *loadInst = llvm::cast<llvm::LoadInst>(inst);

      // ポインタオペランドを処理
      llvm::Value *ptrOperand = loadInst->getPointerOperand();

      if (llvm::isa<llvm::IntToPtrInst>(ptrOperand))
      {
        // inttoptr命令の場合は、そのオペランドを処理
        llvm::IntToPtrInst *intToPtr = llvm::cast<llvm::IntToPtrInst>(ptrOperand);
        llvm::Value *addrOperand = intToPtr->getOperand(0);

        if (llvm::isa<llvm::BinaryOperator>(addrOperand))
        {
          // 算術演算の結果を処理（配列アクセス）
          llvm::BinaryOperator *binOp = llvm::cast<llvm::BinaryOperator>(addrOperand);
          convertArithmeticInstruction(binOp, wasmFunc);
        }
        else if (llvm::isa<llvm::ConstantInt>(addrOperand))
        {
          llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(addrOperand);
          instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
        }
        else if (llvm::isa<llvm::LoadInst>(addrOperand))
        {
          llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(addrOperand);
          uint32_t localIdx = getLocalIndex(load->getPointerOperand());
          instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
        }
      }
      else
      {
        // 通常のポインタアクセス
        uint32_t localIdx = getLocalIndex(ptrOperand);
        instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
      }

      // WebAssemblyメモリロード命令（アライメント考慮）
      instructions.push_back(WasmInstruction(WasmOpcode::I32_LOAD));
    }
    else if (llvm::isa<llvm::StoreInst>(inst))
    {
      llvm::StoreInst *storeInst = llvm::cast<llvm::StoreInst>(inst);

      // ポインタオペランドを先にスタックへ（Wasm storeは「アドレス→値」の順）
      llvm::Value *ptrOperand = storeInst->getPointerOperand();

      if (llvm::isa<llvm::IntToPtrInst>(ptrOperand))
      {
        // inttoptr命令の場合は、そのオペランドを処理
        llvm::IntToPtrInst *intToPtr = llvm::cast<llvm::IntToPtrInst>(ptrOperand);
        llvm::Value *addrOperand = intToPtr->getOperand(0);

        if (llvm::isa<llvm::BinaryOperator>(addrOperand))
        {
          // 算術演算の結果を処理（配列アクセス）
          llvm::BinaryOperator *binOp = llvm::cast<llvm::BinaryOperator>(addrOperand);
          convertArithmeticInstruction(binOp, wasmFunc);
        }
        else if (llvm::isa<llvm::ConstantInt>(addrOperand))
        {
          llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(addrOperand);
          instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
        }
        else if (llvm::isa<llvm::LoadInst>(addrOperand))
        {
          llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(addrOperand);
          uint32_t localIdx = getLocalIndex(load->getPointerOperand());
          instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
        }
      }
      else
      {
        // 通常のポインタアクセス
        uint32_t localIdx = getLocalIndex(ptrOperand);
        instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
      }

      // 値を後からスタックにプッシュ
      llvm::Value *value = storeInst->getValueOperand();
      if (llvm::isa<llvm::ConstantInt>(value))
      {
        llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(value);
        instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
      }
      else if (llvm::isa<llvm::LoadInst>(value))
      {
        llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(value);
        uint32_t valLocalIdx = getLocalIndex(load->getPointerOperand());
        instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, valLocalIdx));
      }
      else if (llvm::isa<llvm::ZExtInst>(value))
      {
        // 直前にzextをローカルに保存している前提で取り出す
        uint32_t valLocalIdx = getLocalIndex(value);
        instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, valLocalIdx));
      }

      // WebAssemblyメモリストア命令（アライメント考慮）
      instructions.push_back(WasmInstruction(WasmOpcode::I32_STORE));
    }

    return true;
  }

  uint32_t WasmGenerator::assignLocalIndex(llvm::Value *value, WasmType type, WasmFunction &wasmFunc)
  {
    if (!value)
    {
      return 0;
    }

    auto it = localMap_.find(value);
    if (it != localMap_.end())
    {
      return it->second;
    }

    uint32_t index = static_cast<uint32_t>(wasmFunc.params.size() + wasmFunc.locals.size());
    wasmFunc.locals.push_back(type);
    localMap_[value] = index;

    return index;
  }

  uint32_t WasmGenerator::getLocalIndex(llvm::Value *value)
  {
    auto it = localMap_.find(value);
    if (it != localMap_.end())
    {
      return it->second;
    }

    // 定義されていないローカル変数を参照しようとした場合
    std::cout << "        警告: 定義されていないローカル変数を参照: " << value->getName().str() << std::endl;

    // デフォルトのインデックスを返す（0）
    return 0;
  }

  std::vector<uint8_t> WasmGenerator::generateBinary() const
  {
    // 簡単なWebAssemblyバイナリ形式を生成
    std::vector<uint8_t> binary;

    // WebAssemblyマジックナンバー
    binary.push_back(0x00);
    binary.push_back(0x61);
    binary.push_back(0x73);
    binary.push_back(0x6D);

    // バージョン
    binary.push_back(0x01);
    binary.push_back(0x00);
    binary.push_back(0x00);
    binary.push_back(0x00);

    // 関数セクション
    binary.push_back(0x03); // 関数セクションID
    binary.push_back(0x01); // セクションサイズ
    binary.push_back(static_cast<uint8_t>(wasmModule_.functions.size()));

    // コードセクション
    binary.push_back(0x0A); // コードセクションID
    binary.push_back(0x01); // セクションサイズ
    binary.push_back(static_cast<uint8_t>(wasmModule_.functions.size()));

    for (const auto &func : wasmModule_.functions)
    {
      // 関数のコードサイズ
      binary.push_back(0x01);
      binary.push_back(0x00); // 空の関数
    }

    return binary;
  }

  std::string WasmGenerator::generateWast() const
  {
    std::ostringstream wast;

    wast << "(module\n";

    // メモリセクション
    wast << "  (memory " << wasmModule_.memorySize;
    if (wasmModule_.memoryMaxSize > 0)
    {
      wast << " " << wasmModule_.memoryMaxSize;
    }
    wast << ")\n";

    // 関数を出力
    for (const auto &func : wasmModule_.functions)
    {
      wast << generateFunctionWast(func) << "\n";
    }

    wast << ")\n";

    return wast.str();
  }

  std::string WasmGenerator::generateFunctionWast(const WasmFunction &func) const
  {
    std::ostringstream wast;

    wast << "  (func $" << func.name;

    // パラメータ
    for (size_t i = 0; i < func.params.size(); ++i)
    {
      wast << " (param $" << i << " " << getWasmTypeString(func.params[i]) << ")";
    }

    // 戻り値
    if (func.returnType != WasmType::VOID)
    {
      wast << " (result " << getWasmTypeString(func.returnType) << ")";
    }

    // ローカル変数
    for (size_t i = 0; i < func.locals.size(); ++i)
    {
      wast << " (local $" << (func.params.size() + i) << " " << getWasmTypeString(func.locals[i]) << ")";
    }

    wast << "\n";

    // 命令
    for (const auto &inst : func.instructions)
    {
      wast << "    " << generateInstructionWast(inst) << "\n";
    }

    wast << "  )";

    return wast.str();
  }

  std::string WasmGenerator::generateInstructionWast(const WasmInstruction &inst) const
  {
    std::ostringstream wast;

    wast << getWasmOpcodeString(inst.opcode);

    for (uint64_t operand : inst.operands)
    {
      wast << " " << operand;
    }

    return wast.str();
  }

  std::string WasmGenerator::getWasmTypeString(WasmType type) const
  {
    switch (type)
    {
    case WasmType::I32:
      return "i32";
    case WasmType::I64:
      return "i64";
    case WasmType::F32:
      return "f32";
    case WasmType::F64:
      return "f64";
    case WasmType::VOID:
      return "void";
    default:
      return "i32";
    }
  }

  std::string WasmGenerator::getWasmOpcodeString(WasmOpcode opcode) const
  {
    switch (opcode)
    {
    case WasmOpcode::I32_CONST:
      return "i32.const";
    case WasmOpcode::I32_ADD:
      return "i32.add";
    case WasmOpcode::I32_SUB:
      return "i32.sub";
    case WasmOpcode::I32_MUL:
      return "i32.mul";
    case WasmOpcode::I32_DIV_S:
      return "i32.div_s";
    case WasmOpcode::I32_DIV_U:
      return "i32.div_u";
    case WasmOpcode::I32_EQ:
      return "i32.eq";
    case WasmOpcode::I32_NE:
      return "i32.ne";
    case WasmOpcode::I32_LT_S:
      return "i32.lt_s";
    case WasmOpcode::I32_LT_U:
      return "i32.lt_u";
    case WasmOpcode::I32_GT_S:
      return "i32.gt_s";
    case WasmOpcode::I32_GT_U:
      return "i32.gt_u";
    case WasmOpcode::I32_LE_S:
      return "i32.le_s";
    case WasmOpcode::I32_LE_U:
      return "i32.le_u";
    case WasmOpcode::I32_GE_S:
      return "i32.ge_s";
    case WasmOpcode::I32_GE_U:
      return "i32.ge_u";
    case WasmOpcode::GET_LOCAL:
      return "local.get";
    case WasmOpcode::SET_LOCAL:
      return "local.set";
    case WasmOpcode::CALL:
      return "call";
    case WasmOpcode::RETURN:
      return "return";
    case WasmOpcode::BR:
      return "br";
    case WasmOpcode::BR_IF:
      return "br_if";
    case WasmOpcode::I32_LOAD:
      return "i32.load";
    case WasmOpcode::I32_STORE:
      return "i32.store";
    default:
      return "unknown";
    }
  }

  bool WasmGenerator::convertIntToPtrInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc)
  {
    auto &instructions = wasmFunc.instructions;

    llvm::IntToPtrInst *intToPtr = llvm::cast<llvm::IntToPtrInst>(inst);

    // WebAssemblyでは、inttoptrは単純に値をそのまま使用
    // メモリアドレスとして扱う
    llvm::Value *operand = intToPtr->getOperand(0);

    if (llvm::isa<llvm::ConstantInt>(operand))
    {
      llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(operand);
      instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
    }
    else if (llvm::isa<llvm::LoadInst>(operand))
    {
      llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(operand);
      uint32_t localIdx = getLocalIndex(load->getPointerOperand());
      instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
    }
    else if (llvm::isa<llvm::BinaryOperator>(operand))
    {
      // 算術演算の結果を処理
      llvm::BinaryOperator *binOp = llvm::cast<llvm::BinaryOperator>(operand);
      convertArithmeticInstruction(binOp, wasmFunc);
    }

    return true;
  }

  bool WasmGenerator::convertPtrToIntInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc)
  {
    auto &instructions = wasmFunc.instructions;

    llvm::PtrToIntInst *ptrToInt = llvm::cast<llvm::PtrToIntInst>(inst);

    // WebAssemblyでは、ptrtointは単純に値をそのまま使用
    llvm::Value *operand = ptrToInt->getOperand(0);

    if (llvm::isa<llvm::ConstantInt>(operand))
    {
      llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(operand);
      instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
    }
    else if (llvm::isa<llvm::LoadInst>(operand))
    {
      llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(operand);
      uint32_t localIdx = getLocalIndex(load->getPointerOperand());
      instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
    }

    return true;
  }

  bool WasmGenerator::convertBitCastInstruction(llvm::Instruction *inst, WasmFunction &wasmFunc)
  {
    auto &instructions = wasmFunc.instructions;

    llvm::BitCastInst *bitCast = llvm::cast<llvm::BitCastInst>(inst);

    // WebAssemblyでは、bitcastは単純に値をそのまま使用
    llvm::Value *operand = bitCast->getOperand(0);

    if (llvm::isa<llvm::ConstantInt>(operand))
    {
      llvm::ConstantInt *constInt = llvm::cast<llvm::ConstantInt>(operand);
      instructions.push_back(WasmInstruction(WasmOpcode::I32_CONST, constInt->getZExtValue()));
    }
    else if (llvm::isa<llvm::LoadInst>(operand))
    {
      llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(operand);
      uint32_t localIdx = getLocalIndex(load->getPointerOperand());
      instructions.push_back(WasmInstruction(WasmOpcode::GET_LOCAL, localIdx));
    }

    return true;
  }

} // namespace asmtowasm
