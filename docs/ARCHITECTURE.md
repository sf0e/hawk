# Hawk — architecture & design notes

## The "Safari look, the Orion way"

Orion (a macOS browser) looks like Safari for three concrete reasons, and we
copy them directly:

1. **One slim toolbar.** No menu bar, no clutter — back chevron, a reload,
   and one address/search bar.
2. **The pill bar.** A single rounded bar, centered content, placeholder
   text instead of labels ("Search or enter an address").
3. **WebKit under the hood.** Orion is WebKit-based; we use WebKitGTK 6.0.
   We even ship a Safari-flavoured fixed UA, exactly as Orion does.

GTK4 + CSS makes the pill trivial:

```css
.omnibar { border-radius: 999px; min-height: 36px; padding: 0 14px; }
```

## Why "written in C" without writing a rendering engine

A from-scratch engine is a decade of work (Blink ~30M LoC). The GNOME Web
architecture — a GTK shell in C around WebKitGTK — is how a real browser in
C ships. Every line Hawk owns is C; the rendering is handled by the
industry-standard engine. That is the honest version of "independent browser
in C", and it beats a personal fork nobody can maintain.

## Fingerprint resistance — what we lock, what we can't

WebKit cannot match Firefox's full `resistFingerprinting` (it's a Gecko
feature). What Hawk locks by default:

| Surface | Measure |
| --- | --- |
| User agent | Fixed Safari-flavoured UA for all installs |
| Third-party cookies | `ACCEPT_NO_THIRD_PARTY` |
| Camera / microphone | media capture disabled |
| WebRTC fingerprinting | no media-stream surface exposed |
| Site data | isolated default profile (own data dir) |
| Dark mode | GTK-level, no 3rd-party site detection via prefers-color-scheme leak machine |

Known limits (documented, not hidden): HTML5 canvas can still read back
fingerprints unless a content-blocker hides it (part of the v0.2 extension
plan), and WebRTC network fingerprinting is only indirectly reduced.

## Cookies persist, no telemetry, no logs

These are *product decisions* easily violated by accident — so they're
baked in structurally:

- No analytics, crash-reporting, or update-ping code exists in the repo.
- The only outbound traffic is: (a) pages you navigate to, (b) search
  queries (to your local SearXNG, *which* is what then forwards to the
  engine of your choice), and (c) SearXNG provisioning on first run.
- Downloads go straight to `~/Downloads`; nothing is recorded.
- All config is a plain file (`~/.config/hawk/hawk.ini`) — fully auditable.

## The on-demand SearXNG backend

`scripts/hawk-searchd` is the lifecycle manager:

1. `start` provisions SearXNG (source checkout into
   `~/.local/share/hawk/src`, venv with `pip install -r requirements.txt`)
   and writes a curated `settings.yml` (`127.0.0.1:8080`, limiter off — no
   Redis needed, raw Google/Bing off).
2. It spawns `python -m searx.webapp` (from the vendored source, with
   `SEARXNG_SETTINGS_PATH` pointing at our settings) via `setsid` and
   records its PID.
3. The browser TCP-health-checks `127.0.0.1:8080` (up to ~12 s), then loads
   the local instance as home.
4. On exit the browser kills the recorded PID and removes the pidfile
   (`hawk_backend_stop` in `src/backend.c`).

Fallback logic: if the backend never comes up, search goes to
`html.duckduckgo.com` and home to `duckduckgo.com`. **Never** a tracker by
default.

## Why not "both Chrome Web Store AND Mozilla add-ons"

We chose the WebExtensions shape (*Mozilla-guided*: manifest V3, content
scripts, `declarativeNetRequest`) as Hawk's add-on format. Honest
constraints:

- Firefox's XPI format and full `browser.*` API are implemented on Gecko;
  no separate engine can execute them as-is.
- WebKitGTK ships its own built-in WebExtensions runtime (MV3 subset +
  DNR) — that's what lets us run real content blockers.
- "Chrome store extensions" run Blink-specific APIs; they cannot run on
  WebKit either.
- Themes are not supported by WebKitGTK's extension runtime at all — we
  won't pretend otherwise.

Therefore: v0.2 wires WebKit's extension runtime (content blockers first);
v0.3 adds Hawk's own add-on store serving WebExtensions-format packages
(fully under our control, privacy-checked on the way in).

## Layout

```
src/main.c       app bootstrap (GtkApplication)
src/browser.c    window, toolbar pill, webview, privacy application
src/backend.c    SearXNG find/spawn/health/stop
src/settings.c   config (INI) + settings dialog
scripts/hawk-searchd   backend lifecycle manager (shell)
```