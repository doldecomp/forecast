Forecast Channel
================
[![Build Status]][actions] [![Code Progress]][progress] [![Data Progress]][progress]


[Build Status]: https://github.com/doldecomp/forecast/actions/workflows/build.yml/badge.svg
[actions]: https://github.com/doldecomp/forecast/actions/workflows/build.yml

[Code Progress]: https://decomp.dev/doldecomp/forecast.svg?mode=shield&measure=code&label=Code
[Data Progress]: https://decomp.dev/doldecomp/forecast.svg?mode=shield&measure=data&label=Data
[progress]: https://decomp.dev/doldecomp/forecast


A decompilation of The Wii's Forecast Channel.

This repository does **not** contain any game assets or assembly whatsoever. An existing copy of the channel WAD is required.

Supported versions:

- `HAFE`: v7 (USA/NTSC)

Dependencies
============

Windows
--------

On Windows, it's **highly recommended** to use native tooling. WSL or msys2 are **not** required.  
When running under WSL, [objdiff](#diffing) is unable to get filesystem notifications for automatic rebuilds.

- Install [Python](https://www.python.org/downloads/) and add it to `%PATH%`.
  - Also available from the [Windows Store](https://apps.microsoft.com/store/detail/python-311/9NRWMJP3717K).
- Download [ninja](https://github.com/ninja-build/ninja/releases) and add it to `%PATH%`.
  - Quick install via pip: `pip install ninja`

macOS
------

- Install [ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages):

  ```sh
  brew install ninja
  ```

[wibo](https://github.com/decompals/wibo), a minimal 32-bit Windows binary wrapper, will be automatically downloaded and used.

Linux
------

- Install [ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages).

[wibo](https://github.com/decompals/wibo), a minimal 32-bit Windows binary wrapper, will be automatically downloaded and used.

Building
========

- Clone the repository:

  ```sh
  git clone https://github.com/doldecomp/forecast.git
  ```

- Copy your WAD to `orig/HAFE`
- Configure:

  ```sh
  python configure.py
  ```

  To use a version other than `HAFE` (USA/NTSC v7), specify it with `--version`.

- Build:

  ```sh
  ninja
  ```

AI Usage
========
You are entirely responsible for whatever AI-generated code you submit, You should treat it as if you wrote it yourself.

tldr; no slop and you'll be fine

Diffing
=======

Once the initial build succeeds, an `objdiff.json` should exist in the project root.

Download the latest release from [encounter/objdiff](https://github.com/encounter/objdiff). Under project settings, set `Project directory`. The configuration should be loaded automatically.

Select an object from the left sidebar to begin diffing. Changes to the project will rebuild automatically: changes to source files, headers, `configure.py`, `splits.txt` or `symbols.txt`.

![](assets/objdiff.png)

Credits
=======
[ogws](https://github.com/doldecomp/ogws): RVL_SDK, MSL, nw4r

[Petari](https://github.com/SMGCommunity/Petari): MetroTRK
