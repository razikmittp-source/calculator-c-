# abyss — design notes

`abyss` is a native GTK3 application written in C++17. It is around
1000 lines of C++ and one CSS string, with no runtime dependencies
beyond `libgtk-3-0`. It compiles to a ~91 KB binary.

This file documents the design choices, not the build (see the repo
README for that).

---

## window layout

```
+-------------------------- header: "abyss" ---------------+ [light theme]
|                                                                          |
| documents  |  editor (monospace, slow caret)              | thoughts     |
|            |                                              |              |
| here.crc   |  voice main() {                              |  free text   |
| rain.crc   |      keep name: echo = "i am here"           |  area, saved |
| letters.   |      whisper(name)                           |  to disk     |
|     crc    |  }                                           |              |
| fade.crc   |                                              | -----------  |
| heart-     |----------------------------------------------| him          |
|     beat   |  console            [152 ms]   [   run   ]   |  (quote      |
|     .crc   |                                              |   feed)      |
|            |  i am here                                   |              |
|            |                                              | [say…] [send]|
| [new page] |                                              |              |
+----------------------------------------------------------+ rain: on -----+
   filename  ·  5 lines  ·  saved 3s ago                                ↑
                                                            statusbar
```

---

## the dark theme

The CSS is one string inside the binary, loaded with
`GtkCssProvider.load_from_data`. Colors are:

| role                  | hex       |
|-----------------------|-----------|
| background (deepest)  | `#07090c` |
| background (panel)    | `#0b1015` |
| background (console)  | `#0f161d` |
| separator             | `#161c24` |
| body text             | `#8993a0` |
| bright text           | `#c7cdd6` |
| dim text              | `#4b5360` |
| accent (caret, tomb)  | `#6b3a3a` |

There is no light theme. The "light theme" button in the header opens
a dialog:

> *"there is no light here. mark removed the switch. it was the first thing he committed."*

The dialog appears every time. The button never changes its label.

---

## features that the README mentions

### slow caret

A CSS rule sets a `steps(1)` animation on the caret so that it blinks
roughly every 2.4 seconds instead of GTK's default ~1s. *"the cursor
blinks slower than my heart now,"* mark wrote.

### "forgotten" line counter

The status bar shows `N lines`. If the active file hasn't been saved
in 90 seconds — measured against the wall-clock from the last
explicit save (`Ctrl+S`) — the counter changes to `N lines
(forgotten)`. Saving resets it.

`abyss` never auto-saves to your `.crc` files; only `Ctrl+S` does.
Thoughts (see below) **do** auto-save. Documents are also kept in
memory across tab switches, so you can edit several files without
losing changes.

### thoughts

A plain text area on the right. Every keystroke is written to
`~/.config/abyss/thoughts.txt`. It's loaded on startup. It is
**never** shown to the run / compile pipeline.

### him

A second small text area below "thoughts." Each time you type
something into the entry box and press Enter (or click "send"), the
panel adds a single line from a small embedded set of quotes —
sentences mark wrote into his notebooks. Some examples:

> *"i wrote a function called fade. i call it more than i should."*

> *"perhaps tomorrow. perhaps not. that's the only branch i write now."*

> *"pain is a float because integers couldn't hold it."*

The quotes are picked uniformly at random with `std::mt19937` seeded
from `steady_clock`. There is no network call. Nothing leaves the
machine.

### rain

The status bar has a `rain: on` / `rain: off` button. When on,
`abyss` opens a child process to `paplay --raw --rate=22050
--format=s16le --channels=1` (or `aplay -f S16_LE -r 22050 -c 1`
if `paplay` isn't there), and a worker thread streams generated
pink-ish noise to its stdin.

If neither audio player is installed, the toggle still flips, the
label still updates, and nothing is played. The application stays
silent. mark would have wanted that.

### darkening on a tomb

When a program returns at least one tomb entry, the window adds a
CSS class `darkened` for ~700 ms. The CSS for `.darkened` softens
the background and reduces the brightness of the editor; the effect
is brief and not interactive.

---

## running programs

When the user clicks **run**:

1. The current editor buffer is written to a temporary `.crc` file
   under `/tmp` via `mkstemps`.
2. `g_spawn_sync` calls `python3 <interpreter> run <tempfile>`.
3. `stdout` is shown in the console.
4. If the run produced `<tempfile>.tomb`, every line is appended to
   the console under `— tomb —` and styled with the accent color.
5. The temp file and tomb file are deleted.

The interpreter is found through, in order:

1. `$COREC_HOME/compiler/corec.py`;
2. `<binary_dir>/../compiler/corec.py`;
3. `<binary_dir>/../../compiler/corec.py`;
4. `<binary_dir>/corec.py`;
5. otherwise, the literal command `corec` is invoked.

---

## intentional omissions

Things this editor doesn't do, on purpose:

- no light theme, no theme switcher, no theme preferences;
- no plugins, no language server, no completion;
- no project tree — only a flat list of starter documents and pages
  you create in-session;
- no auto-save for source files;
- no network features of any kind;
- no telemetry;
- no per-line numbers in the gutter (the *forgotten counter* lives
  in the status bar instead).

---

## file locations

| path                              | purpose                                          |
|-----------------------------------|--------------------------------------------------|
| `~/.config/abyss/thoughts.txt`    | the notes pad, auto-saved on every change.       |
| `~/.config/abyss/<name>.crc`      | saved copies of any document you save (Ctrl+S).  |
| `/tmp/abyss-XXXXXX.crc`           | temporary files used during `run`. cleaned up.   |
| `*.crc.tomb`                      | written by the interpreter beside the source if  |
|                                   | a run produced silent failures.                  |

Nothing else is written.
