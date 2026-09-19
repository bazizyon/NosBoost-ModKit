# NosBoost ModKit

This repo is the starting point for making one mod for NosTale that runs on NosBoost.

NosBoost is a small system that runs inside the game. It can load extra DLL files ("mods") while the game is running. Fork this repo to make your own mod.

## What is inside

- `ModContract.h` — this is the toolbox. It has all the functions and types a mod is allowed to use, like creating a widget on screen or reading a packet from the server.
- `ExampleMod/` — an example mod that already builds and runs. It shows how to use the tools in `ModContract.h`. You can rename it, change it, or replace it with your own code.

## How to make your mod

1. Fork this repo.
2. Open it in CLion (or any CMake-based IDE) and build it. You do not need to download anything else by hand. The build automatically fetches the SDK and ImGui for you.
3. Rename the `ExampleMod` folder and its `.cpp` file to your mod's name. Open the top-level `CMakeLists.txt` and change every place it says `ExampleMod` to your new name (the file names, the `add_library` name, and the target name in `target_include_directories` / `target_compile_options` / `set_target_properties`).
4. Open your mod's `.cpp` file and write your own code. Use the tools from `ModContract.h` to make widgets (buttons, labels, panels) or to read packets sent by the server.
5. Build your mod. It compiles into a DLL and is placed straight into the game's `mods` folder. The next time NosBoost starts, it finds your DLL and loads it automatically.

## What the SDK is for

While you write your mod, you will use class and packet definitions that come from the SDK repo (`NosSDK_-NosBoost-`). You do not need to set this up yourself — the build fetches it for you. It gives you things like the exact shape of a widget or a packet, so your mod can read and change them safely.

## A note for later

If a mod is not working, first check that the DLL is really in the game's `mods` folder, and that NosBoost's overlay shows your mod's name in its list. Most problems are caused by a stale build being copied instead of a fresh one.
