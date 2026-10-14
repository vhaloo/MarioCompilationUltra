#!/bin/sh
set -eu
mcu_package=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
chmod +x "$mcu_package/Play.sh" "$mcu_package/MarioCompilationUltra"
mcu_applications="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
mkdir -p "$mcu_applications"
# Escape the Desktop Entry quoted argument syntax, including paths with spaces.
mcu_exec=$(printf '%s' "$mcu_package/Play.sh" | sed 's/\\/\\\\/g; s/"/\\"/g; s/`/\\`/g; s/\$/\\$/g; s/%/%%/g')
mcu_desktop="$mcu_applications/mario-compilation-ultra.desktop"
cat > "$mcu_desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Mario Compilation Ultra
Exec="$mcu_exec" --menu
Icon=$mcu_package/assets/icon.png
Terminal=false
Categories=Game;Emulator;
EOF
chmod +x "$mcu_desktop"
if command -v xdg-user-dir >/dev/null 2>&1; then
    mcu_desktop_dir=$(xdg-user-dir DESKTOP)
    if [ -d "$mcu_desktop_dir" ]; then cp "$mcu_desktop" "$mcu_desktop_dir/"; fi
fi
printf '%s\n' "$mcu_desktop"
