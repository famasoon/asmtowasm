# AsmToWasm - Assembly to LLVM IR/Wasm Converter

This is a C++ tool that converts simple Assembly code to LLVM IR and WebAssembly (WAT/WASM).

## Overview

This project compiles a simplified Assembly language into LLVM IR (Intermediate Representation), and optionally emits WebAssembly (WAT/WASM). It is designed for education and prototyping.

## Features

- Parse Assembly input
- Generate LLVM IR (via `LLVMGenerator` and the more advanced `AssemblyLifter`)
- Generate WebAssembly text/binary (`--wast`/`--wasm`)
- Basic arithmetic (ADD, SUB, MUL, DIV)
- Data movement (MOV)
- Comparison (CMP) and conditional branches (JMP, JE/JZ, JNE/JNZ, JL, JG, JLE, JGE)
- Function calls (CALL, RET)
- Stack operations (PUSH, POP)
- Registers and simple memory addressing support

## Requirements

- CMake 3.15+
- LLVM 10.0+
- C++17 compiler (GCC 7+ or Clang 5+)

## Build

```bash
# Go to project directory
cd asmtowasm

# Create build directory
mkdir build
cd build

# Configure with CMake (C++ only)
cmake ..

# Build
make
```

## Usage

```bash
# Basic: print LLVM IR to stdout (or to file with -o)
./asmtowasm examples/simple_add.asm
./asmtowasm -o build/out.ll examples/arithmetic.asm

# Use advanced lifter (recommended) for detailed IR logs
./asmtowasm --lifter examples/function_calls.asm

# Emit WebAssembly text/binary (input file is the last positional argument)
./asmtowasm --wast build/out.wat examples/conditional_jump.asm
./asmtowasm --wasm build/out.wasm examples/loop_example.asm

# Help
./asmtowasm --help
```

## Supported Assembly syntax

### Instruction form
```
[label:] MNEMONIC [operand1] [, operand2] [; comment]
```

### Operand kinds
- Registers: `%eax`, `%ebx`, `%ecx`, `%edx`, `%esi`, `%edi`
- Immediates: `10`, `-5`, `0x1A`
- Memory addresses: `(%eax)`, `(%ebx+4)`
- Labels: `start`, `loop`, `end`

### Supported instructions

#### Arithmetic
- `ADD dst, src` - add
- `SUB dst, src` - subtract
- `MUL dst, src` - multiply
- `DIV dst, src` - divide

#### Data movement
- `MOV dst, src` - move

#### Comparison and branching
- `CMP op1, op2` - signed compare; sets internal flags `ZF, LT, GT, LE, GE`
- `JMP label` - unconditional branch
- `JE/JZ label` - equal (`ZF != 0`)
- `JNE/JNZ label` - not equal (`ZF == 0`)
- `JL label` - less than (`LT != 0`)
- `JG label` - greater than (`GT != 0`)
- `JLE label` - less-or-equal (`LE != 0`)
- `JGE label` - greater-or-equal (`GE != 0`)

#### Functions
- `CALL function` - call function
- `RET [value]` - return

#### Stack
- `PUSH src` - push
- `POP dst` - pop

## Examples

### Simple add
```assembly
start:
    mov %eax, 10      # %eax = 10
    mov %ebx, 20      # %ebx = 20
    add %eax, %ebx    # %eax = %eax + %ebx
    ret               # return
```

### Branching
```assembly
compare:
    mov %eax, 10
    mov %ebx, 15
    cmp %eax, %ebx
    jl less_than
    jg greater_than
    je equal

less_than:
    mov %ecx, 1
    jmp end

greater_than:
    mov %ecx, 2
    jmp end

equal:
    mov %ecx, 0

end:
    ret
```

## Output example (WAT, modern syntax)

```wat
(module
  (func $main (result i32) (local $0 i32) (local $1 i32)
    i32.const 10
    local.get 0
    i32.store
    i32.const 20
    local.get 1
    i32.store
    ;; ... omitted ...
    i32.const 0
    return)
)
```

## Project layout

```
asmtowasm/
├── CMakeLists.txt          # CMake build
├── README.md               # This file
├── include/                # Headers
│   ├── assembly_parser.h   # Assembly parser
│   ├── llvm_generator.h    # Simple LLVM IR generator
│   ├── assembly_lifter.h   # Advanced LLVM IR generator
│   └── wasm_generator.h    # Wasm generator
├── src/                    # Sources
│   ├── main.cpp            # CLI
│   ├── assembly_parser.cpp # Parser
│   ├── assembly_lifter.cpp # Advanced IR generator
│   ├── llvm_generator.cpp  # Simple IR generator
│   └── wasm_generator.cpp  # Wasm generator
└── examples/               # Sample assemblies
    ├── simple_add.asm      # simple add
    ├── arithmetic.asm      # arithmetic
    └── conditional_jump.asm# branching
```

## Limitations

- Educational, simplified
- Branching relies on internal flags (ZF/LT/GT/LE/GE); CF/SF/OF etc. are not implemented
- Memory/stack are simplified models (do not follow a real ABI)
- Wasm emission is minimal (no structured control lowering yet)
- No optimization passes

## Roadmap

- More instructions/flags (AND/OR/XOR/SHL/SHR, CF/SF/OF, ...)
- Better error handling
- Optimization passes
- Debug info
- Richer addressing modes

## Troubleshooting

- If CMake detects `clang-cl` and fails, ensure it's configured for C++ only, clear cache, and reconfigure.
  - `CMakeLists.txt` uses `project(AsmToWasm)`
  - Example: `rm -rf build/CMakeCache.txt build/CMakeFiles && CXX=/usr/bin/clang++ cmake ..`

## License

This project is released under the MIT License.

## Contributing

Bug reports and feature requests are welcome via GitHub Issues.
