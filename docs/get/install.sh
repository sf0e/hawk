#!/bin/sh
# Hawk — "a browser that stays out of your way." install script
# You just ran a curl straight into your shell. Please understand the risks
# and maybe read this before doing that. There is nothing here that will eat
# your files, but there is a browser that occasionally closes itself.
#
# What this does:
#   1. checks you're on Linux (that's all we support)
#   2. installs build dependencies with your package manager
#   3. clones the Hawk source from GitHub
#   4. builds it
#   5. drops the binary + search manager at ~/.local/bin/
#   6. installs a desktop entry + icons so it shows up in rofi / app menus
#   7. all browser data (cookies, history, sessions) lives in ~/.hawk
#
# uninstall: curl -fsSL https://sf0e.github.io/hawk/get/uninstall.sh | sh
set -e

REPO="sf0e/hawk"
TAG="v0.1.0-alpha"
BINDIR="$HOME/.local/bin"

say()   { printf '\033[1;33mhawk\033[0m %s\n' "$*"; }
die()   { printf '\033[1;31mhawk\033[0m error: %s\n' "$*" >&2; exit 1; }

[ "$(uname -s)" = "Linux" ] || die "Hawk is Linux-only. Sorry. Not even an apology can fix it."

command -v curl >/dev/null 2>&1 || die "curl is required to run this installer."

say "checking build tools..."

has() { command -v "$1" >/dev/null 2>&1; }
needs_pkg() { ! has "$1"; }

# figure out the package manager once
PM=""
if has pacman; then PM=pacman; elif has apt-get; then PM=apt; elif has dnf; then PM=dnf; fi
[ -n "$PM" ] || die "no package manager I recognise (pacman/apt/dnf). Install the deps yourself, see the README."

pkgs=""
for c in gcc pkg-config meson ninja; do
  needs_pkg "$c" && pkgs="$pkgs $c"
done

# library dev headers vary by distro — always wanted
libs=""
case "$PM" in
  pacman) libs="gtk4 webkitgtk-6.0";;
  apt)    libs="libgtk-4-dev libwebkitgtk-6.0-dev";;
  dnf)    libs="gtk4-devel webkitgtk6.0-devel";;
esac

if [ -n "$pkgs" ] || [ -n "$libs" ]; then
  say "installing: $pkgs $libs"
  case "$PM" in
    pacman) yay -S --needed $pkgs $libs 2>/dev/null || sudo pacman -S --needed $pkgs $libs;;
    apt)    sudo apt-get update && sudo apt-get install -y $pkgs $libs;;
    dnf)    sudo dnf install -y $pkgs $libs;;
  esac
fi

# all deps present now?
for c in gcc pkg-config meson ninja; do
  has "$c" || die "could not install $c. install it manually and rerun."
done

say "downloading Hawk source..."
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
curl -fsSL "https://github.com/$REPO/archive/refs/tags/$TAG.tar.gz" -o "$TMP/hawk.tar.gz" \
  || curl -fsSL "https://github.com/$REPO/archive/refs/heads/master.tar.gz" -o "$TMP/hawk.tar.gz" \
  || die "could not download the source. is GitHub having a day?"

cd "$TMP"
tar xzf hawk.tar.gz
cd hawk-*

say "building (first run has to set up the Search engine, this takes a while)..."
meson setup build >/dev/null
ninja -C build

mkdir -p "$BINDIR"
install -m755 build/hawk "$BINDIR/hawk"
install -m755 scripts/hawk-searchd "$BINDIR/hawk-searchd"

say "installing menu entry and icons..."
APPS="$HOME/.local/share/applications"
ICONS="$HOME/.local/share/icons/hicolor"
for s in 16 24 32 48 64 128 256; do
  mkdir -p "$ICONS/$s/apps"
  install -m644 "data/icons/hicolor/$s/apps/org.hawk.Hawk.png" "$ICONS/$s/apps/"
done
mkdir -p "$ICONS/scalable/apps"
install -m644 data/icons/hicolor/scalable/apps/org.hawk.Hawk.svg "$ICONS/scalable/apps/"
mkdir -p "$APPS"
sed "s|^Exec=hawk|Exec=$BINDIR/hawk|" data/org.hawk.Hawk.desktop > "$APPS/org.hawk.Hawk.desktop"

say "done. run it with:"
echo
echo "    $BINDIR/hawk"
echo
say "first launch clones the SearXNG search engine, which is slow and makes"
say "coffee. subsequent launches are fine. enjoy."