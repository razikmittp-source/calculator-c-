# CoreC ⚡

**A blazingly fast programming language that transpiles to C.**

CoreC gives you the speed of C with a clean, modern syntax. No semicolons, no `#include`, no pain. Write code fast, compile to native binaries via gcc.

## 🚀 Quick Start

```bash
# Clone
git clone https://github.com/mineroce/corec.git
cd corec

# Run a program
python3 compiler/corec.py run examples/hello.crc

# Or build a binary
python3 compiler/corec.py build examples/fibonacci.crc -o fib
./fib
```

## ✨ Syntax Overview

```
fn main() {
    let name = "World"
    print("Hello, {name}!")

    let nums = [1, 2, 3, 4, 5]
    let mut sum = 0
    for n in nums {
        sum += n
    }
    print("Sum: {sum}")
}

fn factorial(n: i32) -> i32 {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
```

## 🔥 Features

| Feature | CoreC | C | C++ |
|---------|-------|---|-----|
| Semicolons | ❌ Not needed | ✅ Required | ✅ Required |
| Type inference | ✅ `let x = 42` | ❌ `int x = 42;` | ⚠️ `auto x = 42;` |
| String interpolation | ✅ `"Hello {name}"` | ❌ `printf(...)` | ❌ `std::format(...)` |
| For-in loops | ✅ `for x in arr` | ❌ Manual indexing | ⚠️ Range-based |
| Range loops | ✅ `for i in [0..10]` | ❌ `for(int i=0;i<10;i++)` | ❌ Same |
| Compile speed | ⚡ Instant transpile + gcc | ⚡ gcc | 🐌 Can be slow |
| Runtime speed | 🔥 Same as C (IS C) | 🔥 Native | 🔥 Native |
| Header files | ❌ Auto-import | ✅ `#include` hell | ✅ `#include` hell |

## 📦 Installation

**Requirements:** Python 3.8+ and gcc

```bash
# Install (symlink to PATH)
sudo ln -sf $(pwd)/compiler/corec.py /usr/local/bin/corec

# Now use anywhere
corec run myprogram.crc
corec build myprogram.crc -o myprogram
```

## 🛠 CLI Commands

```bash
corec build <file.crc>            # Compile to native binary
corec run <file.crc>              # Compile and run immediately
corec emit <file.crc>             # Show generated C code
corec build <file.crc> -o <name>  # Custom output name
corec build <file.crc> --keep-c   # Keep the .c file
```

## 🖥 Web IDE

CoreC comes with a built-in web IDE with syntax highlighting and live compilation:

```bash
cd ide
python3 server.py
# Open http://localhost:8080
```

## 📖 Language Reference

### Variables
```
let x = 42            // immutable, type inferred
let mut counter = 0   // mutable
let name = "Alice"    // string
let pi: f64 = 3.14    // explicit type
```

### Types
- `i32` — 32-bit integer
- `i64` — 64-bit integer  
- `f32` — 32-bit float
- `f64` — 64-bit float
- `bool` — boolean
- `str` — string (const char*)
- `void` — no return value

### Functions
```
fn add(a: i32, b: i32) -> i32 {
    return a + b
}

fn greet(name: str) {
    print("Hello, {name}!")
}
```

### Control Flow
```
// If-else
if x > 10 {
    print("big")
} else {
    print("small")
}

// While loop
while x < 100 {
    x += 1
}

// Range for loop
for i in [0..10] {
    print("{i}")
}

// Array for-in loop
for item in array {
    print("{item}")
}
```

### Arrays
```
let nums = [1, 2, 3, 4, 5]
let first = nums[0]
```

### String Interpolation
```
let name = "World"
let age = 25
print("Hello {name}, you are {age}!")
```

## ⚡ Benchmarks

```
CoreC prime sieve (100k):  7ms   ← compiled to C with -O2
Python prime sieve (100k): 3200ms
```

CoreC is **~450x faster than Python** because it compiles to optimized native C code.

## 📁 Project Structure

```
corec/
├── compiler/
│   └── corec.py         # Compiler (lexer → parser → codegen → C)
├── ide/
│   ├── index.html       # Web IDE frontend
│   └── server.py        # IDE backend (compile & run API)
├── examples/
│   ├── hello.crc        # Hello World
│   ├── fibonacci.crc    # Fibonacci sequence
│   ├── factorial.crc    # Factorial
│   ├── arrays.crc       # Arrays and loops
│   └── benchmark.crc    # Prime number benchmark
└── README.md
```

## 🗺 Roadmap

- [x] Lexer, Parser, Code Generator
- [x] Variables (let, let mut, type inference)
- [x] Functions with typed parameters
- [x] If/else, while, for-in, for-range
- [x] Arrays and indexing
- [x] String interpolation
- [x] CLI (build, run, emit)
- [x] Web IDE with syntax highlighting
- [ ] Structs
- [ ] Pattern matching (match)
- [ ] Modules / imports
- [ ] Package manager (`corec get`)
- [ ] Self-hosted compiler (CoreC written in CoreC)
- [ ] LSP server for VS Code extension

## 📄 License

MIT License

## 👤 Author

**mineroce** — Creator of CoreC
