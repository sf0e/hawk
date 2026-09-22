#!/bin/sh
# Hawk — "a browser that stays out of your way." uninstall script
# Removes Hawk from this machine: binary, search manager, desktop entry,
# icons, and (with --purge) all of ~/.hawk.
#
# Usage:
#   uninstall.sh          # remove the program, keep your data in ~/.hawk
#   uninstall.sh --purge  # remove the program AND all your data
set -e

BINDIR="$HOME/.local/bin"
APPS="$HOME/.local/share/applications"
ICONS="$HOME/.local/share/icons/hicolor"

say()  { printf '\033[1;33mhawk\033[0m %s\n' "$*"; }

unlink() {
    if [ -f "$1" ] || [ -L "$1" ]; then
        rm -f "$1" && say "removed $1"
    else
        say "did not find $1"
    fi
}

# stop any running backend first
if command -v "$BINDIR/hawk-searchd" >/dev/null 2>&1; then
    "$BINDIR/hawk-searchd" stop >/dev/null 2>&1 || true
fi
pkill -f 'searx\.webapp' 2>/dev/null || true

say "removing Hawk..."
unlink "$BINDIR/hawk"
unlink "$BINDIR/hawk-searchd"

if command -v desktop-file-validate >/dev/null 2>&1 || command -v update-desktop-database >/dev/null 2>&1; then
    :
fi

unlink "$APPS/org.hawk.Hawk.desktop"

for s in 16 24 32 48 64 128 256; do
    unlink "$ICONS/$s/apps/org.hawk.Hawk.png"
done
unlink "$ICONS/scalable/apps/org.hawk.Hawk.png"
unlink "$ICONS/scalable/apps/org.hawk.Hawk.svg"

# old-layout data dirs from earlier versions
for d in "$HOME/.local/share/hawk" "$HOME/.local/share/searc" "$HOME/.config/hawk"; do
    if [ -d "$d" ]; then
        rm -rf "$d"
        say "removed old data dir $d"
    fi
done

if [ "${1:-}" = "--purge" ]; then
    if [ -d "$HOME/.hawk" ]; then
        rm -rf "$HOME/.hawk"
        say "removed all browser data (~/.hawk)"
    fi
    say "Hawk is gone. So is everything it knew about you."
else
    say "Hawk removed. Your cookies, history and sessions are still in ~/.hawk"
    say "if you want them gone too: $0 --purge"
fi
say "done. Hawk, a browser that stays out of your way — no longer in it."