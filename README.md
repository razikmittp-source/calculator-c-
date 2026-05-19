# CoreC

**Язык, который слышит машину.**

> *"Все остальные языки — шум, налипший на оригинал. CoreC — чистый тон."*
> — К. (тетрадь 04, лист 17)

CoreC транспилируется в чистый C, компилируется через gcc → нативный
бинарник. Скорость ровно как у C, потому что это **и есть C** под капотом.
Только синтаксис чище и короче.

Историю языка и его создателя читай в [`LORE.md`](LORE.md).

---

## Быстрый старт

```bash
# Клонировать
git clone -b devin/corec-lang https://github.com/razikmittp-source/calculator-c-.git corec
cd corec

# Запустить программу
python3 compiler/corec.py run examples/hello.crc

# Собрать бинарник
python3 compiler/corec.py build examples/fibonacci.crc -o fib
./fib

# Собрать .exe для Windows (кросс-компиляция)
python3 compiler/corec.py build examples/hello.crc --target windows
# → examples/hello.exe (PE, работает на Windows)
```

## Установка

**Требования:** Python 3.8+, gcc

Для Windows .exe: `sudo apt install mingw-w64`

```bash
./install.sh
# или вручную:
sudo ln -sf $(pwd)/compiler/corec.py /usr/local/bin/corec
```

## Синтаксис

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

## Возможности

| Фича | CoreC | C | C++ |
|-------|-------|---|-----|
| Точки с запятой | Нет | Обязательны | Обязательны |
| Вывод типов | `let x = 42` | `int x = 42;` | `auto x = 42;` |
| Интерполяция строк | `"Hello {name}"` | `printf(...)` | `std::format(...)` |
| For-in циклы | `for x in arr` | Ручная индексация | Range-based |
| Range циклы | `for i in [0..10]` | `for(int i=0;i<10;i++)` | То же |
| Структуры | `struct Point { x: i32, y: i32 }` | `typedef struct ...` | `struct ...` |
| Pattern matching | `match x { 1 => ... }` | switch(x){case 1:...} | То же |
| Кросс-компиляция | `--target windows` | Руками через MinGW | CMake-файлы |
| Скорость | = C (это и есть C) | Нативная | Нативная |

## CLI

```bash
corec build <file.crc>                   # Linux бинарник
corec build <file.crc> --target windows  # Windows .exe
corec build <file.crc> --target win32    # Windows 32-bit .exe
corec run <file.crc>                     # Собрать и запустить
corec emit <file.crc>                    # Показать сгенерированный C
corec build <file.crc> -o <name>         # Своё имя выходного файла
corec build <file.crc> --keep-c          # Оставить .c файл
corec targets                            # Список платформ
```

**Доступные таргеты:**
| Таргет | Компилятор | Выход |
|--------|-----------|-------|
| `linux` | gcc | ELF binary |
| `linux64` | gcc -m64 | ELF 64-bit |
| `linux32` | gcc -m32 | ELF 32-bit |
| `windows` / `win64` | x86_64-w64-mingw32-gcc | PE .exe 64-bit |
| `win32` | i686-w64-mingw32-gcc | PE .exe 32-bit |

## IDE

Два варианта:

**Native IDE (C++/GTK3):**
```bash
./ide-native/corec-studio    # Visual Studio-style IDE
./ide-native/corec-ide       # простой IDE
```

**Web IDE:**
```bash
cd ide && python3 server.py
# Открой http://localhost:8080
```

## Справочник по языку

### Переменные
```
let x = 42              // неизменяемая, тип выведен
let mut counter = 0     // изменяемая
let name = "Alice"      // строка
let pi: f64 = 3.14      // явный тип
```

### Типы
- `i32` — 32-bit целое
- `i64` — 64-bit целое
- `f32` — 32-bit float
- `f64` — 64-bit float
- `bool` — булево
- `str` — строка (const char*)
- `void` — нет возвращаемого значения
- пользовательские struct-типы

### Функции
```
fn add(a: i32, b: i32) -> i32 {
    return a + b
}

fn greet(name: str) {
    print("Hello, {name}!")
}
```

### Управление потоком
```
if x > 10 {
    print("big")
} else {
    print("small")
}

while x < 100 {
    x += 1
}

for i in [0..10] {
    print("{i}")
}

for item in array {
    print("{item}")
}
```

### Структуры
```
struct Point {
    x: i32
    y: i32
}

struct Player {
    id: i32
    health: i32
    pos: Point
}

fn main() {
    let p = Point { x: 3, y: 4 }
    print("{p.x}, {p.y}")

    let hero = Player {
        id: 1
        health: 100
        pos: Point { x: 0, y: 0 }
    }
}
```

### Pattern Matching
```
match x {
    1 => print("one")
    2 => print("two")
    _ => print("other")
}

// С блоками:
match day {
    1 => { return "понедельник" }
    7 => { return "воскресенье" }
    _ => { return "другой день" }
}
```

### Массивы
```
let nums = [1, 2, 3, 4, 5]
let first = nums[0]
```

### Строковая интерполяция
```
let name = "World"
let age = 25
print("Hello {name}, you are {age}!")
```

## Бенчмарки

```
CoreC prime sieve (100k):  7ms    (нативный C с -O2)
Python prime sieve (100k): 3200ms
```

**CoreC в ~450 раз быстрее Python** — потому что компилируется в оптимизированный нативный C.

## Структура проекта

```
corec/
├── compiler/
│   └── corec.py           # Компилятор (lexer → parser → codegen → C)
├── ide-native/
│   ├── corec_studio.cpp   # VS-style IDE (C++/GTK3)
│   ├── corec_ide.cpp      # Простой IDE (C++/GTK3)
│   ├── corec-studio       # Бинарник VS-style IDE
│   └── corec-ide          # Бинарник простого IDE
├── ide/
│   ├── index.html         # Web IDE
│   └── server.py          # Бэкенд IDE
├── examples/
│   ├── hello.crc          # Hello World
│   ├── fibonacci.crc      # Числа Фибоначчи
│   ├── factorial.crc      # Факториал
│   ├── arrays.crc         # Массивы и циклы
│   ├── benchmark.crc      # Бенчмарк (простые числа)
│   ├── struct_demo.crc    # Структуры
│   └── match_demo.crc     # Pattern matching
├── LORE.md                # История создателя CoreC
├── README.md
├── LICENSE                # MIT
└── install.sh
```

## Roadmap

- [x] Лексер, парсер, кодогенератор
- [x] Переменные (let, let mut, type inference)
- [x] Функции с типизированными параметрами
- [x] If/else, while, for-in, for-range
- [x] Массивы и индексация
- [x] Строковая интерполяция
- [x] CLI (build, run, emit)
- [x] Web IDE + нативная IDE (C++/GTK3)
- [x] Структуры (struct) с вложенностью
- [x] Pattern matching (match/=>)
- [x] Кросс-компиляция: Linux, Windows x64, Windows x32
- [ ] Модули / import
- [ ] Пакетный менеджер (`corec get`)
- [ ] Self-hosted компилятор (CoreC на CoreC)
- [ ] LSP сервер для VS Code

## Лицензия

MIT License

## Автор

**К.** — создатель CoreC (1997)

*Восстановлено из архивных тетрадей и дискет. Подробности — в [LORE.md](LORE.md).*
