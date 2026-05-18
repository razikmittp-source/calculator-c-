# core-c language reference

This document defines core-c as implemented by `compiler/corec.py` in
this repository. The language is small on purpose. There are no
modules, no classes, no exceptions, no closures-by-name, no generics.
Programs are interpreted; failures are silent.

---

## 1. lexical structure

### 1.1 whitespace and newlines

Spaces, tabs, and carriage returns are insignificant. A newline ends
a statement. Multiple newlines collapse into one.

### 1.2 comments

```
~~ this is a comment until end of line
```

There is only one comment form. Block comments do not exist.

### 1.3 identifiers

`[A-Za-z_][A-Za-z0-9_]*` — case-sensitive. Identifiers that match
keywords or type names cannot be used as variable names.

### 1.4 keywords

```
voice  keep  perhaps  otherwise  drown  bury  fade
still  gone   void
in  and  or  not
echo  memory  pain  shadow
```

`whisper` and `listen` are **not** keywords — they are built-in
functions and can be reassigned (though doing so is unkind).

### 1.5 operators and punctuation

```
+  -  *  /  %
==  !=  <  >  <=  >=
=
->     ~~ function return type
..     ~~ range
(  )   {  }   [  ]
,  :
```

### 1.6 literals

| literal       | example                       |
|---------------|-------------------------------|
| integer       | `0`, `42`, `1000000`          |
| float         | `0.0`, `3.14`, `2.5`          |
| string        | `"i am here"`                 |
| boolean       | `still`, `gone`               |
| nothing       | `void`                        |
| array         | `[1, 2, 3]`, `["a", "b"]`     |
| range         | `0..10` (the value is `[0..9]`) |

Strings support escape sequences `\n \t \r \" \\ \{ \}`, and
**interpolation** with `{name}`:

```
keep name = "anna"
whisper("hello, {name}.")
~~ hello, anna.
```

A missing name in interpolation expands to `void` and the failure is
logged silently. The program continues.

---

## 2. types

| type     | values                       | python equivalent       |
|----------|------------------------------|-------------------------|
| `echo`   | strings                      | `str`                   |
| `memory` | integers                     | `int`                   |
| `pain`   | floating-point numbers       | `float`                 |
| `shadow` | arrays of anything           | `list`                  |
| `void`   | the absence of a value       | `None`                  |

Type annotations are **optional** everywhere they can appear. They
are accepted by the parser but the interpreter does not enforce them.
mark left it that way deliberately.

```
keep n: memory = 0
keep label: echo = "drift"
keep things: shadow = [1, "two", 3.0]
```

`still` and `gone` are values of an implicit boolean type
(`memory`-compatible — they coerce to `1` and `0` for arithmetic).

---

## 3. declarations

### 3.1 variables

```
keep <name> [: <type>] [= <expr>]
```

```
keep age              ~~ defined as void
keep age: memory      ~~ defined as void, hinted as memory
keep age = 30
keep age: memory = 30
```

Re-declaring with `keep` defines a new variable in the current scope.
Assignment to an existing name walks the scope chain:

```
keep n = 0
drown n < 3 {
    n = n + 1      ~~ updates the outer `n`
}
```

### 3.2 functions

```
voice <name> ( [<param> [: <type>] [, ...]] ) [-> <return-type>] {
    <statements>
}
```

```
voice greet(name: echo) -> echo {
    fade "hello, " + name + "."
}
```

Functions are values. They can be passed by name to other positions
(but core-c has no anonymous functions; if you need one, define it).

Function bodies share lexical scope with the place they were defined
in (lexical closures over **the defining environment**, not over
local frames).

### 3.3 main

If a function named `main` is defined and no top-level statements
have already failed, the interpreter calls `main()` after loading
the file.

---

## 4. statements

### 4.1 assignment

```
<name> = <expr>
<name>[<index-expr>] = <expr>
```

Indexed assignment silently fails if the target is not a list, or
the index is out of bounds. The failure is added to the tomb.

### 4.2 expression statements

Any expression can stand alone. Calls are the most common case:

```
whisper("done.")
```

### 4.3 `perhaps` / `otherwise`

```
perhaps <cond> { ... }
perhaps <cond> { ... } otherwise { ... }
perhaps <cond> { ... } otherwise perhaps <cond2> { ... } otherwise { ... }
```

`<cond>` is converted to a truth value (see [§5](#5-truthiness)).

### 4.4 `drown`

```
drown <cond> { ... }
```

A while-loop. The interpreter aborts a `drown` after 10 million
iterations and adds *"drown ran too long"* to the tomb.

### 4.5 `bury … in …`

```
bury <name> in <iterable> { ... }
```

A for-loop. `<iterable>` may be:

- a `shadow` (array) — iterates its elements;
- an `echo` (string) — iterates its characters;
- a number `n` — iterates `0, 1, …, n-1`;
- a range `a..b` — iterates `a, a+1, …, b-1`.

`<name>` is bound in a new inner scope for each iteration.

### 4.6 `fade`

```
fade                ~~ return void
fade <expr>         ~~ return the value
```

`fade` only makes sense inside a function. At the top level of a file
`fade` is harmless — it stops the file's top-level execution before
`main` would have run.

---

## 5. truthiness

| value type                  | truthy iff           |
|-----------------------------|----------------------|
| `void`                      | never                |
| `still` / `gone`            | `still`              |
| `memory`, `pain` (numbers)  | not zero             |
| `echo` (string)             | non-empty            |
| `shadow` (array)            | non-empty            |
| anything else               | always               |

---

## 6. operators

### 6.1 arithmetic

```
+   -   *   /   %
```

- Integer/integer division (`/`) is **truncating**: `7 / 2 == 3`.
- Float operands give float results.
- `+` on strings concatenates.
- `+` on arrays concatenates.
- `*` on `(string, int)` repeats: `"." * 5 == "....."`.
- Anything that doesn't fit the table above evaluates to `void`
  and a line is added to the tomb.

### 6.2 comparison

```
==   !=   <   >   <=   >=
```

Equality is by value. Ordering comparisons only return `still` when
both operands are numbers (compared numerically) or both are strings
(compared lexically). Otherwise they return `gone`.

### 6.3 logical

```
and   or   not
```

`and` and `or` short-circuit and return one of their operands (not
necessarily a boolean): `0 or "hi"` → `"hi"`.

### 6.4 indexing

```
<value>[<index>]
```

Valid on `shadow` and `echo`. Out-of-range and non-integer indices
return `void` and add a line to the tomb.

### 6.5 calls

```
<callee>(<arg>, <arg>, ...)
```

Missing positional arguments become `void`. Extra arguments are
silently dropped. Calling a non-callable adds *"not a voice"* to the
tomb.

---

## 7. built-in functions

| name        | signature                              | what it does                                    |
|-------------|----------------------------------------|-------------------------------------------------|
| `whisper`   | `whisper(a, b, c, …)`                  | prints all args joined by single spaces, then `\n`. |
| `listen`    | `listen(prompt: echo) -> echo`         | reads a line of input; returns `""` on EOF.      |
| `length`    | `length(value) -> memory`              | length of a string or array; `void` otherwise.   |
| `count`     | `count(a: memory, b: memory) -> shadow`| `[a, a+1, …, b-1]`. equivalent to `a..b`.        |
| `as_memory` | `as_memory(x) -> memory`               | coerce to int; `void` on failure.                |
| `as_pain`   | `as_pain(x) -> pain`                   | coerce to float; `void` on failure.              |
| `as_echo`   | `as_echo(x) -> echo`                   | string representation.                           |

There are intentionally no functions for files, networking, threads,
times, or randomness. *"the language is meant to be small enough that
a person can hold all of it at once,"* mark wrote. *"and lonely
enough that it doesn't reach outside the room it's in."*

---

## 8. the tomb

When an operation fails — a missing name, a bad index, a non-numeric
operand on `<`, a `drown` that ran too long, a call to something that
isn't a function — the failure is appended to a list called the
**tomb**.

After the program ends, if the tomb is non-empty, the interpreter
writes it to a file `<source>.crc.tomb` next to the input.

```
$ corec run examples/heartbeat.crc
thump.
...
thump.
...
thump.
...
then nothing.
$ ls examples/heartbeat.crc.tomb 2>/dev/null || echo "no tomb."
no tomb.
```

The IDE displays the tomb inline in the console panel and briefly
darkens the window. Programs cannot read or clear their own tomb;
the tomb is for the person watching, not for the program that died.

---

## 9. command-line interface

```
corec run <file.crc>     ~~ run the file
corec emit <file.crc>    ~~ run the file, write a JSON report
                            with {output, tomb, elapsed_ms}
```

Exit codes are uninformative on purpose.
