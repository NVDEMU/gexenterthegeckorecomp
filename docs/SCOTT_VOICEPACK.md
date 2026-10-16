# Scott the Woz Voice Pack — experimental test

This is an optional, disabled-by-default voice mod for the USA PlayStation game
Gex: Enter the Gecko (SLUS-00598). The four public MP3s in the repository root
are the source assets. Nightly builds resample them to mono 22,050-Hz PCM and
encode them into VAG/PS1 ADPCM resources, which the trusted plugin plays through
PSXRecomp's private SPU voice path. The original game's disc image is not
patched. This first prototype triggers the custom clips at mapped moments; it
does not yet suppress individual stock Gex voice samples, so the original line
or sound can sometimes still be heard underneath.

## Planned four clips

| Moment | Source MP3 | Generated resource |
|---|---|---|
| Level entry | hey-all-scott-here.mp3 | audio/level-entry.vag |
| Movement / jump | wii-play-do-you.mp3 | audio/movement-jump.vag |
| Death | anything-that-s-not-made-by-nintendo-is-bullshit-nintendo-scott-the-woz-e3-2017-games-video-game.mp3 | audio/death.vag |
| Idle | my-obligation-to-society.mp3 | audio/idle.vag |

## First-pass triggers and limitations

- **Level entry:** the first time the plugin observes a plausible live-player
  health state, and again after a sustained non-death state gap that resembles
  a level transition.
- **Movement / jump:** when directional input, the Cross button, or meaningful
  analog-stick movement begins. A cooldown prevents constant retriggering.
- **Death:** when the tracked health value transitions from 1–4 to zero.
- **Idle:** after ten seconds without controller input; it may repeat no more
  often than every thirty seconds. Playback duration is tracked from the encoded
  ADPCM length because PS1 voices can remain logically active after their end block.

The first-pass player-state guard uses RAM addresses commonly listed for the
USA release (0x80095310 lives, 0x80095314 health). They are not verified debug
symbols. The plugin fails closed when these values are implausible, but the
actual trigger timing still requires in-game validation. In particular, every
level transition is not yet guaranteed to be detected, and controller input
is only a proxy for character movement. This is experimental test code, not a
claim that all four gameplay events have been verified.

## Build

The Nightly workflow requires ffmpeg and tools/encode_psx_vag.py. It copies the
mod into the generated PSXRecomp project before code generation/build, and
ships the converted VAG resources with the bundled mod catalog. The feature is
off by default; enable Scott the Woz Voice Pack in the launcher's Mods list to
test it.
