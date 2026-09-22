![Hawk](data/hawk.png)

# Hawk

A small browser for Linux. Written in C. It runs on GTK4 and WebKitGTK, and it
looks a little bit like Safari. That's about as far as the ambitions go.

**status: alpha.** it's a real, working browser — tabs, pills, dark mode,
local search — out in the world so people can poke at it. rough edges are
listed below so you can't say you weren't warned.

Hawk doesn't call home. It doesn't have an account system. It doesn't have AI
anything. It doesn't have a logo budget. What it has is a pill-shaped address
bar and a private search engine that runs on your own machine, on demand,
only while you're using it, and then quietly dies when you close the window.

## why

Most of the web is fine to read. The toll to read it is: some browser company
wants to know what you read, where you click, how long you look, and whether
you'd buy their Chromebook. Hawk is the alternative nobody asked for. It
tracks nothing because it was built by one person who couldn't be bothered to
set up tracking.

It's also an enormous C project now, which is its own kind of punishment.

## what it does

- one big pill bar for addresses and search, Safari style
- pill tabs that get smaller as you open more of them, like they're shy
- real tabs: Ctrl+T opens, Alt+W closes, Ctrl+L focuses the bar, Ctrl+R reloads
- window.open() lands in a new tab instead of a popup
- cookies persist. your logins survive. nothing is wiped on exit
- third-party cookies blocked by default, camera/mic off, one locked user-agent
- dark mode, the correct kind, set in one settings dialog

## search

Search runs through a local **SearXNG** instance bound to 127.0.0.1:8080.
Hawk starts it when you open the browser and kills it when you close the
browser. Your queries never leave your machine on their way to a search
engine, which is the entire point and also the slowest part of startup.

If the backend can't come up (first run has to clone and build SearXNG, and
the universe is cruel), Hawk gives up gracefully and falls back to DuckDuckGo.
It doesn't tell you unless you look at the logs, because what's one more
disappointment.

## build

You need meson, ninja, a C compiler, and the dev headers for gtk4 and
webkitgtk-6.0.

```sh
meson setup build
ninja -C build
./build/hawk
```

## install

Linux only. There is no Windows build and no macOS build and there won't be.
The current cut is the alpha release
[v0.1.0-alpha](https://github.com/sf0e/hawk/releases/tag/v0.1.0-alpha).

```sh
curl -fsSL https://sf0e.github.io/hawk/get/install.sh | sh
```

The installer builds from source and drops in a desktop entry plus the hawk
icon, so it shows up in rofi and your app menu. A proper distributable binary
and maybe a flatpak will arrive later, once we run out of other ways to
procrastinate.

## layout

```
src/browser.c   the whole browser: chrome, tabs, pills, cognitive load
src/backend.c   find/spawn/stop the local SearXNG engine
src/settings.c  one settings dialog, plain INI file
src/hawk.h      the shared bits
scripts/hawk-searchd   backend lifecycle manager (shell)
```

## known issues

- the window occasionally closes itself after about eight seconds on systems
  without a window manager. this is being investigated. it is not a feature.
- the tabs are closable in theory. in practice they sometimes refuse. talk
  to them about it.
- no history manager yet. kudos.
- SearXNG first boot is slow enough to make coffee. not our fault.

## license

BSD 2-Clause. See [LICENSE](LICENSE). It's short, it's permissive, it's yours.
Do whatever. At least someone in this relationship is giving.