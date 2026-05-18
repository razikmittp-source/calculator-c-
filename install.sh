#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INSTALL_DIR="${INSTALL_DIR:-$HOME/.local/bin}"
mkdir -p "$INSTALL_DIR"

echo
echo "  abyss / core-c installer"
echo "  ------------------------"
echo "  a language by mark. the rain came with it."
echo

MISSING=()
command -v python3 >/dev/null || MISSING+=("python3")
if [ ${#MISSING[@]} -gt 0 ]; then
    echo "  missing: ${MISSING[*]}"
    echo "  debian/ubuntu: sudo apt install python3"
    exit 1
fi

COREC="$SCRIPT_DIR/compiler/corec.py"
if [ ! -f "$COREC" ]; then
    echo "  the interpreter at $COREC is missing."
    exit 1
fi

cat > "$INSTALL_DIR/corec" <<EOF
#!/usr/bin/env bash
exec python3 "$COREC" "\$@"
EOF
chmod +x "$INSTALL_DIR/corec"
echo "  installed: $INSTALL_DIR/corec"

if command -v pkg-config >/dev/null && pkg-config --exists gtk+-3.0; then
    if command -v g++ >/dev/null && command -v make >/dev/null; then
        echo "  building abyss (the editor)..."
        (cd "$SCRIPT_DIR/ide-native" && make >/dev/null)
        if [ -x "$SCRIPT_DIR/ide-native/abyss" ]; then
            cat > "$INSTALL_DIR/abyss" <<EOF
#!/usr/bin/env bash
export COREC_HOME="$SCRIPT_DIR"
exec "$SCRIPT_DIR/ide-native/abyss" "\$@"
EOF
            chmod +x "$INSTALL_DIR/abyss"
            echo "  installed: $INSTALL_DIR/abyss"
        fi
    else
        echo "  g++ / make not found; skipping abyss build."
    fi
else
    echo "  gtk+-3.0 not found; skipping abyss build."
    echo "  on debian: sudo apt install libgtk-3-dev build-essential pkg-config"
fi

if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    echo
    echo "  $INSTALL_DIR is not in your PATH."
    echo "  add this line to ~/.bashrc or ~/.zshrc:"
    echo "      export PATH=\"\$HOME/.local/bin:\$PATH\""
fi

echo
echo "  try:"
echo "      corec run $SCRIPT_DIR/examples/here.crc"
echo "      abyss"
echo
