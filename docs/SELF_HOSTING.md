# self-hosting roadmap

> the language is not finished until it can describe itself.
> mark wrote that on the front page of a notebook in 2019.
> the notebook is still on his desk.

right now `core-c` is interpreted by a python program
([`compiler/corec.py`](../compiler/corec.py)). that is the bootstrap.
the goal is to remove python from the picture entirely.

## phase 0 -- where we are

- interpreter is written in python (~1400 lines).
- there is a working language: `voice`, `keep`, `whisper`, `drown`,
  `perhaps`, `bury`, `fade`, `memory`, `pain`, `echo`, `silence`.
- there is an ide (`abyss`) and a build pipeline (portable `.pyz`,
  native via pyinstaller, github actions for three-os release).

## phase 1 -- transpile to c

write a back end inside `corec.py` that emits c instead of evaluating
the ast. then compile the c with `zig cc` (a single binary that can
cross-compile to every target triple linux/windows/macos x86_64 and
arm64). this gives us:

- real native binaries
- real cross-compile (linux host → windows binary, etc.)
- a path off of the python runtime for end users

the language is small. the runtime needed is small (`whisper`,
`listen`, growable arrays, a tomb of error strings, a fade/return
unwind). a few thousand lines of generated c should be enough.

deliverables in this phase:

1. `corec transpile hello.crc -o hello.c`
2. `corec build hello.crc --backend c` (transpile + zig cc)
3. examples still pass; ci is extended to also build via c on every
   push.

## phase 2 -- rewrite the compiler in core-c

once the c backend is stable, port the python compiler to core-c
itself. the steps in order:

1. add the missing language features the compiler needs and that the
   current spec is silent about: structs, enums, pattern matches,
   sized integer types, byte strings, file io, process spawning.
   keep the names in the language family (`shape` for struct,
   `whisper_to(file, x)` for write, `summon(cmd)` for spawn).
2. translate `corec.py` to `corec.crc` module by module. start with
   the lexer (smallest), then the parser, then the interpreter,
   then the c emitter. each phase keeps a python reference so we
   can diff outputs.
3. when the core-c compiler can build itself from itself ("stage 1
   compiles stage 2 compiles stage 3, stage 2 == stage 3"), we drop
   the python bootstrap and ship the language as a single native
   binary.

## phase 3 -- a real backend

after self-hosting, the c transpiler can be replaced with an llvm or
qbe backend if we want sharper codegen and faster compiles. this is
optional. zig cc is good enough for a long time.

## why this order

self-hosting before the llvm backend means the language proves it can
do real work first. a language that compiles itself is by definition
not a toy. then we can choose backends without being trapped by
python.

mark thinks the whole plan takes about a year of weekends. that is
fine. the only deadline he cares about is the next commit.
