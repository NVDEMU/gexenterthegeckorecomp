# Gex: Enter the Gecko Recomp

A native Windows/macOS recompilation project for the **USA PlayStation release of Gex: Enter the Gecko** using [PSXRecomp](https://github.com/RetroPortingToolKit/psxrecomp).

> **Development status:** initial bring-up automation. The workflow uses PSXRecomp's current project generator to probe the original disc, discover the boot executable, generate translated C, compile it natively, and package builds. A successful compile is not proof of a playable game: booting, graphics, sound, controls, FMVs, level loading, save data, and a full playthrough still need real testing.

## Build targets

- Windows x64
- macOS Apple Silicon (arm64)
- macOS Intel (x64)

GitHub Actions builds a **Nightly** release when commits reach `main` and can be started manually from **Actions → Gex Nightly → Run workflow**. Each release replaces the previous Nightly assets, rather than accumulating stale copies.

## Getting started

1. Download the zip for your platform from [Releases](https://github.com/NVDEMU/gexenterthegeckorecomp/releases).
2. Extract it and launch the included application.
3. Select your own legally obtained **Gex: Enter the Gecko (USA)** disc image when prompted. The game's original disc image is not included.
4. Report the exact platform, game revision, and any log/error details when a regression occurs.

The game is single-player. The project starts with PSXRecomp's standard launcher, runtime settings, and setup wizard; game-specific enhancements will be added only after baseline behavior is tested.

## Asset separation

The public repository contains build and project automation only. The private repository `NVDEMU/gexprivateassets` stores the CUE sheet and source metadata. The 254 MiB BIN is kept in your Dropbox and downloaded in CI using a GitHub Actions secret containing a Dropbox shared URL (simplest) or Dropbox OAuth credentials. The workflow validates the file's size and Dropbox content hash before using it. Original game data and generated game C are not committed to this public repository or included in release zips.

## Configure automated builds

Add the repository Actions secrets described in [Build setup](docs/BUILD_SETUP.md). Until the private-assets and Dropbox secrets are configured, the workflow will stop early with an actionable error.

## Toolchain and acknowledgements

- [PSXRecomp](https://github.com/RetroPortingToolKit/psxrecomp) — MIPS-to-C recompilation framework and PlayStation runtime.
- [recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui) — shared native launcher and UI.

Gex: Enter the Gecko and its original code/media are the property of their respective rights holders. This project is an independent compatibility/recompilation effort and is not affiliated with or endorsed by Crystal Dynamics, Midway, or Sony.
