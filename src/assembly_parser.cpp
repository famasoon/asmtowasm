#include "assembly_parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace asmtowasm
{

  AssemblyParser::AssemblyParser()
  {
    instructions_.clear();
    labels_.clear();
    errorMessage_.clear();
  }

  bool AssemblyParser::parseFile(const std::string &filename)
  {
    std::ifstream file(filename);
    if (!file.is_open())
    {
      errorMessage_ = "ファイルを開けませんでした: " + filename;
      return false;
    }

    std::string line;
    size_t lineNumber = 0;

    while (std::getline(file, line))
    {
      lineNumber++;
      if (!parseLine(line))
      {
        errorMessage_ = "行 " + std::to_string(lineNumber) + " でエラー: " + errorMessage_;
        return false;
      }
    }

    file.close();
    return true;
  }

  bool AssemblyParser::parseString(const std::string &assemblyCode)
  {
    std::istringstream stream(assemblyCode);
    std::string line;
    size_t lineNumber = 0;

    while (std::getline(stream, line))
    {
      lineNumber++;
      if (!parseLine(line))
      {
        errorMessage_ = "行 " + std::to_string(lineNumber) + " でエラー: " + errorMessage_;
        return false;
      }
    }

    return true;
  }

  bool AssemblyParser::parseLine(const std::string &line)
  {
    // コメントを除去
    std::string cleanLine = removeComments(line);

    // トリム
    cleanLine = trim(cleanLine);

    // 空行をスキップ
    if (cleanLine.empty())
    {
      return true;
    }

    std::istringstream iss(cleanLine);
    std::string token;
    std::vector<std::string> tokens;

    // トークンに分割
    while (iss >> token)
    {
      tokens.push_back(token);
    }

    if (tokens.empty())
    {
      return true;
    }

    // ラベルかどうかチェック
    std::string firstToken = tokens[0];
    if (firstToken.back() == ':')
    {
      // ラベル
      std::string labelName = firstToken.substr(0, firstToken.length() - 1);
      labels_[labelName] = instructions_.size();
      std::cout << "ラベル " << labelName << " を検出しました。トークン数: " << tokens.size() << std::endl;

      // ラベルの後に命令があるかチェック
      if (tokens.size() > 1)
      {
        InstructionType type = parseInstructionType(tokens[1]);
        if (type == InstructionType::UNKNOWN)
        {
          errorMessage_ = "不明な命令: " + tokens[1];
          return false;
        }

        Instruction inst(type);
        inst.label = labelName;

        // オペランドを解析
        for (size_t i = 2; i < tokens.size(); ++i)
        {
          inst.operands.push_back(parseOperand(tokens[i]));
        }

        instructions_.push_back(inst);
      }
      else
      {
        // 単独のラベルの場合、LABEL命令を作成
        Instruction labelInst(InstructionType::LABEL);
        labelInst.label = labelName;
        instructions_.push_back(labelInst);
        std::cout << "単独のラベル " << labelName << " を処理しました" << std::endl;
      }
    }
    else
    {
      // 通常の命令
      InstructionType type = parseInstructionType(firstToken);
      if (type == InstructionType::UNKNOWN)
      {
        errorMessage_ = "不明な命令: " + firstToken;
        return false;
      }

      Instruction inst(type);

      // オペランドを解析
      for (size_t i = 1; i < tokens.size(); ++i)
      {
        inst.operands.push_back(parseOperand(tokens[i]));
      }

      instructions_.push_back(inst);
    }

    return true;
  }

  InstructionType AssemblyParser::parseInstructionType(const std::string &instruction)
  {
    std::string upper = instruction;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "ADD")
      return InstructionType::ADD;
    if (upper == "SUB")
      return InstructionType::SUB;
    if (upper == "MUL")
      return InstructionType::MUL;
    if (upper == "DIV")
      return InstructionType::DIV;
    if (upper == "MOV")
      return InstructionType::MOV;
    if (upper == "CMP")
      return InstructionType::CMP;
    if (upper == "JMP")
      return InstructionType::JMP;
    if (upper == "JE")
      return InstructionType::JE;
    if (upper == "JZ") // alias of JE
      return InstructionType::JE;
    if (upper == "JNE")
      return InstructionType::JNE;
    if (upper == "JNZ") // alias of JNE
      return InstructionType::JNE;
    if (upper == "JL")
      return InstructionType::JL;
    if (upper == "JG")
      return InstructionType::JG;
    if (upper == "JLE")
      return InstructionType::JLE;
    if (upper == "JGE")
      return InstructionType::JGE;
    if (upper == "CALL")
      return InstructionType::CALL;
    if (upper == "RET")
      return InstructionType::RET;
    if (upper == "PUSH")
      return InstructionType::PUSH;
    if (upper == "POP")
      return InstructionType::POP;

    return InstructionType::UNKNOWN;
  }

  Operand AssemblyParser::parseOperand(const std::string &operand)
  {
    std::string trimmed = trim(operand);

    // カンマを除去
    if (!trimmed.empty() && trimmed.back() == ',')
    {
      trimmed = trimmed.substr(0, trimmed.length() - 1);
    }

    // レジスタかどうかチェック
    if (trimmed.length() >= 2 && trimmed[0] == '%')
    {
      return Operand(OperandType::REGISTER, trimmed);
    }

    // メモリアドレスかどうかチェック
    if (trimmed.length() >= 3 && trimmed[0] == '(' && trimmed.back() == ')')
    {
      return Operand(OperandType::MEMORY, trimmed);
    }

    // 数値かどうかチェック
    if (std::all_of(trimmed.begin(), trimmed.end(), [](char c)
                    { return std::isdigit(c) || c == '-' || c == '+'; }))
    {
      return Operand(OperandType::IMMEDIATE, trimmed);
    }

    // それ以外はラベルとして扱う
    return Operand(OperandType::LABEL, trimmed);
  }

  std::string AssemblyParser::trim(const std::string &str)
  {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos)
    {
      return "";
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
  }

  std::string AssemblyParser::removeComments(const std::string &line)
  {
    size_t commentPos = line.find('#');
    if (commentPos != std::string::npos)
    {
      return line.substr(0, commentPos);
    }
    return line;
  }

} // namespace asmtowasm
