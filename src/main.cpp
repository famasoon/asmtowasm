#include "assembly_parser.h"
#include "llvm_generator.h"
#include "wasm_generator.h"
#include "assembly_lifter.h"
#include <iostream>
#include <string>
#include <fstream>

void printUsage(const char *programName)
{
  std::cout << "使用方法: " << programName << " [オプション] <入力ファイル>\n";
  std::cout << "オプション:\n";
  std::cout << "  -o <出力ファイル>  LLVM IRの出力ファイルを指定\n";
  std::cout << "  --wasm <ファイル>   WebAssemblyバイナリファイルを出力\n";
  std::cout << "  --wast <ファイル>  WebAssemblyテキストファイルを出力\n";
  std::cout << "  --lifter           高度なAssemblyリフターを使用\n";
  std::cout << "  -h, --help         このヘルプを表示\n";
  std::cout << "  -v, --version      バージョン情報を表示\n";
}

void printVersion()
{
  std::cout << "AsmToWasm v1.0.0\n";
  std::cout << "Assembly to LLVM IR and WebAssembly Converter with Advanced Lifter\n";
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printUsage(argv[0]);
    return 1;
  }

  std::string inputFile;
  std::string outputFile;
  std::string wasmFile;
  std::string wastFile;
  bool useLifter = false;

  // コマンドライン引数を解析
  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help")
    {
      printUsage(argv[0]);
      return 0;
    }
    else if (arg == "-v" || arg == "--version")
    {
      printVersion();
      return 0;
    }
    else if (arg == "-o")
    {
      if (i + 1 < argc)
      {
        outputFile = argv[++i];
      }
      else
      {
        std::cerr << "エラー: -o オプションには出力ファイル名が必要です\n";
        return 1;
      }
    }
    else if (arg == "--wasm")
    {
      if (i + 1 < argc)
      {
        wasmFile = argv[++i];
      }
      else
      {
        std::cerr << "エラー: --wasm オプションには出力ファイル名が必要です\n";
        return 1;
      }
    }
    else if (arg == "--wast")
    {
      if (i + 1 < argc)
      {
        wastFile = argv[++i];
      }
      else
      {
        std::cerr << "エラー: --wast オプションには出力ファイル名が必要です\n";
        return 1;
      }
    }
    else if (arg == "--lifter")
    {
      useLifter = true;
    }
    else if (arg[0] != '-')
    {
      inputFile = arg;
    }
    else
    {
      std::cerr << "エラー: 不明なオプション: " << arg << "\n";
      return 1;
    }
  }

  if (inputFile.empty())
  {
    std::cerr << "エラー: 入力ファイルが指定されていません\n";
    printUsage(argv[0]);
    return 1;
  }

  // 出力ファイル名が指定されていない場合は、入力ファイル名から生成
  if (outputFile.empty())
  {
    size_t dotPos = inputFile.find_last_of('.');
    if (dotPos != std::string::npos)
    {
      outputFile = inputFile.substr(0, dotPos) + ".ll";
    }
    else
    {
      outputFile = inputFile + ".ll";
    }
  }

  std::cout << "Assemblyファイルを解析中: " << inputFile << "\n";

  // Assemblyパーサーを作成
  asmtowasm::AssemblyParser parser;

  // Assemblyファイルをパース
  if (!parser.parseFile(inputFile))
  {
    std::cerr << "パースエラー: " << parser.getErrorMessage() << "\n";
    return 1;
  }

  std::cout << "パース完了: " << parser.getInstructions().size() << " 個の命令を検出\n";

  // デバッグ: 解析された命令の詳細を表示
  for (size_t i = 0; i < parser.getInstructions().size(); ++i)
  {
    const auto &inst = parser.getInstructions()[i];
    std::cout << "命令 " << i << ": ";
    if (!inst.label.empty())
    {
      std::cout << "ラベル=" << inst.label << " ";
    }
    std::cout << "タイプ=" << static_cast<int>(inst.type) << " ";
    std::cout << "オペランド数=" << inst.operands.size() << std::endl;
  }

  // デバッグ: 解析されたラベルの詳細を表示
  std::cout << "ラベル一覧:" << std::endl;
  for (const auto &label : parser.getLabels())
  {
    std::cout << "  " << label.first << " -> " << label.second << std::endl;
  }

  // LLVM IR生成器またはリフターを作成
  if (useLifter)
  {
    std::cout << "高度なAssemblyリフターを使用中...\n";
    asmtowasm::AssemblyLifter lifter;

    // LLVM IRを生成
    std::cout << "リフターに渡す命令数: " << parser.getInstructions().size() << std::endl;
    std::cout << "リフターに渡すラベル数: " << parser.getLabels().size() << std::endl;

    std::cout << "liftToLLVMを呼び出し中..." << std::endl;
    try
    {
      if (!lifter.liftToLLVM(parser.getInstructions(), parser.getLabels()))
      {
        std::cerr << "Assemblyリフターエラー: " << lifter.getErrorMessage() << "\n";
        return 1;
      }
      std::cout << "liftToLLVM完了" << std::endl;
    }
    catch (const std::exception &e)
    {
      std::cerr << "例外が発生しました: " << e.what() << std::endl;
      return 1;
    }
    catch (...)
    {
      std::cerr << "不明な例外が発生しました" << std::endl;
      return 1;
    }

    // LLVM IRをファイルに出力
    std::cout << "LLVM IRを出力中: " << outputFile << "\n";
    if (!lifter.writeIRToFile(outputFile))
    {
      std::cerr << "ファイル出力エラー: " << lifter.getErrorMessage() << "\n";
      return 1;
    }

    // WebAssembly出力が要求されている場合
    if (!wasmFile.empty() || !wastFile.empty())
    {
      std::cout << "WebAssemblyを生成中...\n";

      // LLVMモジュールを取得
      llvm::Module *module = lifter.getModule();

      if (!module)
      {
        std::cerr << "エラー: LLVMモジュールを取得できませんでした\n";
        return 1;
      }

      // WebAssembly生成器を作成
      asmtowasm::WasmGenerator wasmGenerator;

      // WebAssemblyを生成
      if (!wasmGenerator.generateWasm(module))
      {
        std::cerr << "WebAssembly生成エラー: " << wasmGenerator.getErrorMessage() << "\n";
        return 1;
      }

      // WebAssemblyバイナリファイルを出力
      if (!wasmFile.empty())
      {
        std::cout << "WebAssemblyバイナリを出力中: " << wasmFile << "\n";
        if (!wasmGenerator.writeWasmToFile(wasmFile))
        {
          std::cerr << "WebAssemblyバイナリ出力エラー: " << wasmGenerator.getErrorMessage() << "\n";
          return 1;
        }
      }

      // WebAssemblyテキストファイルを出力
      if (!wastFile.empty())
      {
        std::cout << "WebAssemblyテキストを出力中: " << wastFile << "\n";
        if (!wasmGenerator.writeWastToFile(wastFile))
        {
          std::cerr << "WebAssemblyテキスト出力エラー: " << wasmGenerator.getErrorMessage() << "\n";
          return 1;
        }
      }

      std::cout << "生成されたWebAssemblyテキスト:\n";
      std::cout << "----------------------------------------\n";
      std::cout << wasmGenerator.getWastString();
      std::cout << "----------------------------------------\n";
    }

    std::cout << "変換完了!\n";
    std::cout << "生成されたLLVM IR:\n";
    std::cout << "----------------------------------------\n";
    std::cout << lifter.getIRString();
    std::cout << "----------------------------------------\n";

    return 0;
  }
  else
  {
    // 従来のLLVM IR生成器を使用
    asmtowasm::LLVMGenerator generator;

    // LLVM IRを生成
    std::cout << "LLVM IRを生成中...\n";
    if (!generator.generateIR(parser.getInstructions(), parser.getLabels()))
    {
      std::cerr << "LLVM IR生成エラー: " << generator.getErrorMessage() << "\n";
      return 1;
    }

    // LLVM IRをファイルに出力
    std::cout << "LLVM IRを出力中: " << outputFile << "\n";
    if (!generator.writeIRToFile(outputFile))
    {
      std::cerr << "ファイル出力エラー: " << generator.getErrorMessage() << "\n";
      return 1;
    }

    // WebAssembly出力が要求されている場合
    if (!wasmFile.empty() || !wastFile.empty())
    {
      std::cout << "WebAssemblyを生成中...\n";

      // LLVMモジュールを取得
      llvm::Module *module = generator.getModule();

      if (!module)
      {
        std::cerr << "エラー: LLVMモジュールを取得できませんでした\n";
        return 1;
      }

      // WebAssembly生成器を作成
      asmtowasm::WasmGenerator wasmGenerator;

      // WebAssemblyを生成
      if (!wasmGenerator.generateWasm(module))
      {
        std::cerr << "WebAssembly生成エラー: " << wasmGenerator.getErrorMessage() << "\n";
        return 1;
      }

      // WebAssemblyバイナリファイルを出力
      if (!wasmFile.empty())
      {
        std::cout << "WebAssemblyバイナリを出力中: " << wasmFile << "\n";
        if (!wasmGenerator.writeWasmToFile(wasmFile))
        {
          std::cerr << "WebAssemblyバイナリ出力エラー: " << wasmGenerator.getErrorMessage() << "\n";
          return 1;
        }
      }

      // WebAssemblyテキストファイルを出力
      if (!wastFile.empty())
      {
        std::cout << "WebAssemblyテキストを出力中: " << wastFile << "\n";
        if (!wasmGenerator.writeWastToFile(wastFile))
        {
          std::cerr << "WebAssemblyテキスト出力エラー: " << wasmGenerator.getErrorMessage() << "\n";
          return 1;
        }
      }

      std::cout << "生成されたWebAssemblyテキスト:\n";
      std::cout << "----------------------------------------\n";
      std::cout << wasmGenerator.getWastString();
      std::cout << "----------------------------------------\n";
    }

    std::cout << "変換完了!\n";
    std::cout << "生成されたLLVM IR:\n";
    std::cout << "----------------------------------------\n";
    std::cout << generator.getIRString();
    std::cout << "----------------------------------------\n";
  }

  return 0;
}
