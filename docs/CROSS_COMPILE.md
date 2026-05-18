# cross-compile

> mark wrote this section at 3 a.m. on a tuesday.
> he wanted to make sure nobody had to learn seven things
> just to share a program with a friend who used a different os.

there are three honest ways to ship a `.crc` program. one button, one
command, or one tag. pick the one that fits.

---

## 1. portable single-file build (default)

```
corec build hello.crc
```

this produces `hello.pyz` -- a zip archive that is also a python
program. the interpreter and your source are both inside it.
on any machine with `python3` installed, running it works:

```
python3 hello.pyz
# or, on unix:
./hello.pyz
```

if you pass `--target windows` you also get a `hello.bat` next to the
`.pyz`. ship them together; double-clicking the `.bat` runs the
program. `--target macos` does the same with a `.command` file.

**when to use it.** quick sharing, scripts, sending a colleague your
program over chat. requires the recipient to have python3 (almost
every linux and macos already does; windows users install it once).

---

## 2. true native binary (no python on the host)

```
corec build hello.crc --native
```

this calls [pyinstaller](https://pyinstaller.org) under the hood and
produces a single executable file with the python runtime statically
bundled. no install, no dependencies, no shebang, just a binary.

```
./dist/hello                # linux / macos
.\dist\hello.exe            # windows
```

**limitation.** pyinstaller cannot cross-compile. running this on
linux only produces a linux binary. for the three-os trick, see
option 3.

`corec doctor` reports whether pyinstaller is available on this
machine.

---

## 3. one tag, three binaries (the real cross-compile)

push a tag, walk away, come back to a github release with linux,
windows and macos binaries already attached.

```
git tag v0.1.0
git push origin v0.1.0
```

the workflow at [`.github/workflows/release.yml`](../.github/workflows/release.yml)
runs on `ubuntu-latest`, `windows-latest` and `macos-latest`
simultaneously. each runner builds with pyinstaller for its own os.
when all three finish, a release is created and the binaries are
uploaded.

you can also run it on demand (without tagging) from the
**actions → release → run workflow** page, and pass any `.crc` file
in the repo as the source.

`corec build hello.crc --actions` prints the same instructions.

---

## why not "real" cross-compile yet

a true cross-compiler would mean a backend that emits machine code
for a target triple directly -- llvm, qbe, cranelift, or a c
transpilation step followed by `zig cc`. that is the roadmap (see
[`SELF_HOSTING.md`](SELF_HOSTING.md)) and the language is small
enough that it is realistic. but until then, github actions doing
the real thing on real machines is honest and reproducible.

mark would rather we ship something that works than promise something
that doesn't.
