#!/bin/bash
# CoreC installer for Linux

set -e

echo ""
echo "════════════════════════════════════════"
echo "  CoreC Installer  v0.1"
echo "  Blazingly fast language → native C"
echo "════════════════════════════════════════"
echo ""

# Check dependencies
echo "→ Checking dependencies..."

MISSING=()
command -v python3 >/dev/null || MISSING+=("python3")
command -v gcc >/dev/null || MISSING+=("gcc")

if [ ${#MISSING[@]} -gt 0 ]; then
    echo "  ✗ Missing: ${MISSING[*]}"
    echo ""
    echo "Install them with:"
    echo "  Debian/Ubuntu: sudo apt install python3 gcc build-essential"
    echo "  Fedora:        sudo dnf install python3 gcc"
    echo "  Arch:          sudo pacman -S python gcc base-devel"
    exit 1
fi

echo "  ✓ python3: $(python3 --version)"
echo "  ✓ gcc:     $(gcc --version | head -1)"

# Install corec command
INSTALL_DIR="${INSTALL_DIR:-$HOME/.local/bin}"
mkdir -p "$INSTALL_DIR"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPILER="$SCRIPT_DIR/compiler/corec.py"

if [ ! -f "$COMPILER" ]; then
    echo "  ✗ Compiler not found at: $COMPILER"
    exit 1
fi

# Create wrapper
cat > "$INSTALL_DIR/corec" << EOF
#!/bin/bash
exec python3 "$COMPILER" "\$@"
EOF
chmod +x "$INSTALL_DIR/corec"

# Install IDE if available
IDE_BIN="$SCRIPT_DIR/ide-native/corec-ide"
if [ -f "$IDE_BIN" ]; then
    ln -sf "$IDE_BIN" "$INSTALL_DIR/corec-ide"
    echo "  ✓ Installed: $INSTALL_DIR/corec-ide"
fi

echo "  ✓ Installed: $INSTALL_DIR/corec"
echo ""

# Check PATH
if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    echo "⚠️  $INSTALL_DIR is not in your \$PATH"
    echo "    Add to your shell config (~/.bashrc, ~/.zshrc):"
    echo ""
    echo "    export PATH=\"\$HOME/.local/bin:\$PATH\""
    echo ""
fi

echo "════════════════════════════════════════"
echo "  Done!"
echo "════════════════════════════════════════"
echo ""
echo "Usage:"
echo "  corec run hello.crc        # Compile and run"
echo "  corec build hello.crc      # Build binary"
echo "  corec emit hello.crc       # Show generated C"
echo "  corec-ide                  # Launch GUI IDE"
echo ""
echo "Try:"
echo "  corec run $SCRIPT_DIR/examples/hello.crc"
echo ""
