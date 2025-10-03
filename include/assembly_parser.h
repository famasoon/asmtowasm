#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace asmtowasm
{

  // Assembly命令の種類
  enum class InstructionType
  {
    ADD,    // 加算
    SUB,    // 減算
    MUL,    // 乗算
    DIV,    // 除算
    MOV,    // 移動
    CMP,    // 比較
    JMP,    // 無条件ジャンプ
    JE,     // 等しい場合のジャンプ
    JNE,    // 等しくない場合のジャンプ
    JL,     // 小さい場合のジャンプ
    JG,     // 大きい場合のジャンプ
    JLE,    // 小さいか等しい場合のジャンプ
    JGE,    // 大きいか等しい場合のジャンプ
    CALL,   // 関数呼び出し
    RET,    // 関数から戻る
    PUSH,   // スタックにプッシュ
    POP,    // スタックからポップ
    LABEL,  // ラベル
    UNKNOWN // 不明な命令
  };

  // オペランドの種類
  enum class OperandType
  {
    REGISTER,  // レジスタ
    IMMEDIATE, // 即値
    MEMORY,    // メモリアドレス
    LABEL      // ラベル
  };

  // オペランド
  struct Operand
  {
    OperandType type;
    std::string value;

    Operand(OperandType t, const std::string &v) : type(t), value(v) {}
  };

  // Assembly命令
  struct Instruction
  {
    InstructionType type;
    std::vector<Operand> operands;
    std::string label; // ラベルがある場合

    Instruction(InstructionType t) : type(t) {}
  };

  // Assemblyパーサークラス
  class AssemblyParser
  {
  public:
    AssemblyParser();
    ~AssemblyParser() = default;

    // Assemblyファイルをパース
    bool parseFile(const std::string &filename);

    // Assembly文字列をパース
    bool parseString(const std::string &assemblyCode);

    // パースされた命令のリストを取得
    const std::vector<Instruction> &getInstructions() const { return instructions_; }

    // ラベルのマップを取得
    const std::map<std::string, size_t> &getLabels() const { return labels_; }

    // エラーメッセージを取得
    const std::string &getErrorMessage() const { return errorMessage_; }

  private:
    std::vector<Instruction> instructions_;
    std::map<std::string, size_t> labels_; // ラベル名 -> 命令インデックス
    std::string errorMessage_;

    // 命令タイプを文字列から解析
    InstructionType parseInstructionType(const std::string &instruction);

    // オペランドを解析
    Operand parseOperand(const std::string &operand);

    // 行を解析
    bool parseLine(const std::string &line);

    // 文字列のトリム
    std::string trim(const std::string &str);

    // コメントを除去
    std::string removeComments(const std::string &line);
  };

} // namespace asmtowasm
