# C-LOX

This repo is an implementation of an interpreter for the Lox programming language as specified in Robert Nystrom's [Crafting Interpreters](https://craftinginterpreters.com/)

The C, stack-based bytecode virtual machine interpreter is part two of the book. I've also implemented the tree-walk interpreter in Rust, which can be viewed [here](https://github.com/yskim308/rust-walk) (though tree-walk interpreters are _painfully_ slow)

## Getting Started

The build system used is Xmake, because CMake is awful, clunky, and smelly.

To run or build, [install xmake](https://xmake.io/guide/quick-start.html) and run the following

```bash
# to build (note that the run directory root is the project root, not build)
xmake build

# to run in REPL
xmake run

# to run a specific file
xmake run clox ./path/to/file
```

---

## The Lox Programming Language

The full specs for the lox programming language are specified by Nystrom [here](https://craftinginterpreters.com/the-lox-language.html)

A quick TLDR rundown of the language features

- Lox is a dynamically typed, garbage collected language
- Supported Data types:
  - Boolean (true, false)
  - Numbers (encoded as double precision floats)
  - Strings ("String")
  - Nil
- Supported Expressions:
  - any C-style arithmetic (+, -, \* , /)
  - comparison and equaliity (<, <=, >, >=, == )
  - logical operators (and, or, !)
- Other Features:
  - print statements as `print {expression}`
  - variables declared with `var {variable} = {assignment expression}`
  - C-style for and while loops
  - functions declared with `fun {function} {}` and called with `fun()`
  - support for closures

The original implementation supports OOP as well, but I chose not to implement it because

1. I'm an OOP hater
2. Objects are a [poor man's closures](https://gist.github.com/jackrusher/5653669) anyway

---

## Brief Technical Overview

This implementation focuses on memory efficiency and execution speed through a stack-based architecture.

### 1. The Compiler Pipeline

- **Scanner:** A high-speed lexer that produces tokens on-demand to minimize memory overhead.
- **Single-Pass Compilation:** Uses a Pratt Parser to convert source code directly into bytecode, skipping the overhead of an explicit Abstract Syntax Tree (AST).

### 2. Virtual Machine (VM) Architecture

- **Bytecode Instructions:** A custom instruction set designed for dense storage and fast dispatch.
- **Stack-Based Execution:** Uses a value stack for expression evaluation and a call stack of "frames" for function management.
- **Advanced Closures:** Implements **Upvalues** to handle lexical scoping. Includes a "Close Upvalue" mechanism that hoists captured variables from the stack to the heap when a function returns, ensuring memory safety for escaping closures.

### 3. Memory Management

- **Interned Strings:** All strings are de-duplicated in a global hash table using **Weak References**. This allows the VM to perform instant string equality checks (pointer comparison) while still allowing unused strings to be garbage collected.
- **Tricolor Mark-and-Sweep GC:** A sophisticated garbage collector that traces reachability from roots (stack, globals). It features a "Stress Mode" for debugging and handles circular references that would break traditional reference-counting systems.

---

## Some Reflections

This project is the most difficult thing I've ever done. Nystrom is an excellent educator, and I've learned an incredible amount.

I feel much more confident with lower level concepts than I was before, though we did not touch machine code directly (although who does anyway, just emit LLVM IR amiright)

Also got a much bigger appreciation for C, I can see why it has stuck around for so long. It's tiny yet powerful. At the same time, I can see why people flocked to C++, and why Rust is picking up steam.

I'd like to go further and try implementing my own language some day, though I think I'd rather use Rust.

Also, I've become more curious about how the languages I use actually work under the hood, and I'd like to think that this has made me a much better programmer.

lastly, I'd recommend Crafting Interpreters to everyone, it's genuinely one of the best technical books I've ever gone through. Robert Nystrom is the GOAT.
