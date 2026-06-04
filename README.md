# MQ2CamHeight

A [MacroQuest](https://github.com/macroquest/macroquest) plugin for the EQEmu
**RoF2** client that raises (or lowers) the third-person camera's **focal point**
relative to your character. Instead of the character sitting dead-center when you
zoom out or mouselook, the view can be centered on a point **above** them — and it
**auto-lowers in low-ceiling areas** so it doesn't fight the camera collision.

EverQuest has no built-in setting for this: stock "camera height" (Alt+mouse-wheel)
raises the *camera*, but the character stays centered. MQ2CamHeight shifts the
*look-at point* instead, which the client exposes no control for.

## Install

1. Download **`MQ2CamHeight.dll`** from the
   [latest release](https://github.com/galenbrazell/MQ2CamHeight/releases/latest).
2. Drop it into your MacroQuest **`plugins`** folder (next to the other `MQ2*.dll`).
3. In game, run `/plugin MQ2CamHeight load` — or add `MQ2CamHeight=1` under
   `[Plugins]` in `MacroQuest.ini` to load it automatically.

That's it. Then `/camheight on` (or just press a bound key) and tune to taste.

**Requires the RoF2 EMU build of MacroQuest** — the official
[emu-rof2 release](https://github.com/macroquest/macroquest/releases/tag/rel-emu-rof2)
(v3.1.4.7 or a compatible RoF2-emu build). It will **not** load into Live/retail
MacroQuest (different client, different offsets).

## Commands

`/camheight` with no argument (or `status`) shows the current state.

| Command | Effect |
|---|---|
| `/camheight on` \| `off` \| `toggle` | enable/disable the offset |
| `/camheight up` \| `down` | nudge the offset by `step` (default 0.5) |
| `/camheight set <n>` | set the offset directly (also `/camheight <n>`) |
| `/camheight reset` | offset back to 0 |
| `/camheight step <n>` | change the up/down nudge size |
| `/camheight auto on` \| `off` | auto-lower under low ceilings (default on) |
| `/camheight room <low> <open>` | headroom thresholds: full offset at `open`, 0 at `low` |
| `/camheight smooth <n>` | easing factor for the auto-lower (0–1, default 0.15) |
| `/camheight info` | one-shot readout: playerZ, ceiling, headroom, offset |
| `/camheight watch on` \| `off` | print that readout live (for tuning) |

Negative offsets work too (`/camheight set -3`) to drop the focal point below the
character.

## Keybinds

Bind raise/lower (and an optional toggle) to keys with `/bind`:

```
/bind CamHeightUp Page_Up
/bind CamHeightDown Page_Down
/bind CamHeightToggle <key>     (optional)
```

Pressing up/down auto-enables the offset, so a key just works. `/bind list` shows
all binds.

## Settings

Persisted to your MacroQuest ini under `[MQ2CamHeight]` (default **off** until you
turn it on or set `Enabled=1`):

```ini
[MQ2CamHeight]
Enabled=1
Offset=5
Step=0.5
Auto=1
LowRoom=10
OpenRoom=30
Smooth=0.15
```

## How it works

The focal lever is the player's eye/view height (`ViewHeight`) — the point the
third-person camera looks at; raising it lifts the focal point so the character
sits lower on screen. The low-ceiling auto-lower reads the client's own
`CeilingHeightAtCurrLocation`, computes `headroom = ceiling − playerZ`, and smoothly
scales the offset toward 0 as headroom shrinks (and back up as it opens), so the
plugin gets out of the way indoors instead of amplifying the ceiling-snap.

## Building from source

For developers. This repo *is* the plugin folder — clone it into a MacroQuest
source tree set up for the RoF2 EMU client (the `emu` branch):

```
cd <MacroQuest>/plugins
git clone https://github.com/galenbrazell/MQ2CamHeight.git
```

Configure the MacroQuest build with `MQ_BUILD_CUSTOM_PLUGINS=ON` and build (emu =
`Release` / `Win32`); the DLL lands in `build/bin/release/plugins/MQ2CamHeight.dll`.
See MacroQuest's [build docs](https://docs.macroquest.org/main/building/).

## License

GPL-2.0 — see [LICENSE](LICENSE). MacroQuest is GPL-2.0 and this plugin uses its
API, so it's distributed under the same license.

## Credits

By Galen Brazell. Built on [MacroQuest](https://github.com/macroquest/macroquest)
(MQNext).
