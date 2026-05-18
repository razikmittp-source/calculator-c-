# how to run on debian / ubuntu

## 1. install runtime deps

```bash
sudo apt update
sudo apt install python3 libgtk-3-0
```

## 2. open the editor

The bundled binary is in `ide-native/abyss`. Point it at the project so
it can find the interpreter, then launch:

```bash
cd corec-abyss
COREC_HOME="$(pwd)" ./ide-native/abyss
```

Click **run** to execute the open document. Output appears in the
console at the bottom; silent failures appear under `— tomb —`.

Optional, for the rain sound: `sudo apt install pulseaudio-utils`
(or `alsa-utils`).

## 3. (optional) rebuild the binary yourself

```bash
sudo apt install build-essential libgtk-3-dev pkg-config
cd corec-abyss/ide-native
make
./abyss
```

## 4. run programs from the terminal

```bash
python3 compiler/corec.py run examples/here.crc
python3 compiler/corec.py run examples/heartbeat.crc
```

Or use `install.sh` to put `corec` and `abyss` into `~/.local/bin`.

## 5. write your own

Files end in `.crc`. Comments are `~~`. Start with:

```
voice main() {
    whisper("i wrote this.")
}
```

The full reference is in `docs/LANGUAGE.md`; mark's story is in
`docs/STORY.md`.
