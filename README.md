# CubeChip - WORK IN PROGRESS

A multi-system emulator written in C++20 and utilizing SDL3 that started as an effort to learn the language. Aiming to be as accurate as possible from an HLE standpoint, while also aiming to be fairly fast on its feet, at least as far as pure interpreters go. Input, audio and video are handled through SDL3, with the interface being (slowly) fleshed out with ImGUI. There's loads of work to be done, and there's always something to refactor to be more flexible, robust, or faster.

This project simultaneously stands in as an experiment bed for all sorts of different little libraries, interfaces and abstractions I write for myself. As a result, much of the source code in the Components/Utilities folders can run solo or with minimal other required includes, increasing their potential of use in other projects of mine in the future (or others). Some stuff will be deprecated as demands change, or knowledge accumulates toward better solutions.

## Supported Systems

### BytePusher

Merely there for the sake of it being there, and also because it's one of the few systems with such constrained runtime requirements that helped expose certain shortcomings of my existing library designs.

### GameBoy

Been on the backburner now for a long while, left in a hardly-started state, as I'm busy tackling refactors across the entire program and implementing more conveniences, required libraries, abstractions between the frontend and backend, and generally planning on how to handle a proper UI interface. See the Planned Features section below for a rough idea.

### CHIP-8

Currently supports the following major variants:

- CHIP-8 [^1]
- SUPERCHIP [^2]
- XOCHIP [^3]
- MEGACHIP [^4]

[^1]: Also supports legacy behavior mode (planned to be cycle-accurate, but not currently).
[^2]: Also supports legacy behavior mode (seen in HP48 graphing calculators).
[^3]: Also supports 4-planes rendering, and an instruction declined from the official spec.
[^4]: Potentially officially the first emulator since Mega8 to [run the Mega8 demo properly](https://www.youtube.com/watch?v=Z215BO9Gkko).

The following platform extensions/mods are available:

- HIRES MPD [^5] (currently not re-implemented)
- CHIP-8E [^6]
- CHIP-8X [^7]
- HWCHIP64 [^8] (currently not re-implemented)
- SUPERCHIP-8X [^9] (currently not re-implemented)

Some extension combinations aren't possible, see footnotes for now. Due to major refactoring of the codebase, many variants are currently unimplemented, their code unported and in temporary limbo.

[^5]: Stands for Multi-Page-Display. Runs both original 2-page and 4-page roms, as well as patched roms.
[^6]: Exclusive mod to CHIP-8 and SUPERCHIP. Does not work with HIRES MPD. Mutually exclusive with CHIP-8X.
[^7]: Exclusive mod to CHIP-8 and SUPERCHIP. Works with HIRES MPD. Mutually exclusive with CHIP-8E.
[^8]: Extension to XOCHIP, mutually exclusive with all aforementioned extensions. Designed by [@NinjaWeedle](https://github.com/NinjaWeedle/HyperWaveCHIP-64/tree/master).
[^9]: Fantasy extension to SUPERCHIP, combination with CHIP-8X. Designed by me as a fun experiment of a what-if.

## Planned Features

- [x] Implement SHA1 hashing for identifying programs that lack metadata.
- [ ] Utilize JSON for program database use.
- [x] Implement TOML for application settings.
- [x] Implement ImGUI to get a rudimentary and reactive interface going instead of the current single render window.
- [ ] Expand the input system to allow for forms of input and bindings beyond simple keyboard/mouse detection.
- [x] Refactor application to decouple the frontend from the backend emulation worker.
- [x] Implement thread-allocation logic to control which CPU threads the program threads have access to.
- [x] Refactor system interfacing to allow interacting with multiple (and possibly concurrently active) systems, as well as specialized cores for each.
- [x] Implement a triple-buffer middle-man between the frontend and backend worker.
- [x] Allow fetching and displaying rudimentary OSD statistics on a per-system basis.
- [x] Implement File Picker dialog for when drag-n-drop doesn't work, for some reason.
- [x] Refactor cmake build script for smarter generation and cached vendoring of fetched third party libraries.
- [x] Allow choosing desired core to boot when loading a program file that is eligible with multiple specialized cores.
- [ ] Implement stepping controls for debugging, initially limited to simple full-frame and per-instruction stepping.
- [x] Implement (initially) a slot-in ImGUI interface where custom ImGUI code can be pointer'd to to execute, and defined elsewhere as desired.
- [ ] By extension, a configuration ImGUI layer for each system, optionally expandable by each specialized core.
- [ ] Figure out a good way to serialize, version, and differentiate the full state of different cores for savestate support.
- [ ] Implement an abstraction layer to allow core-driven copying of state data and ImGUI presentation layer that (initially) the frontend will passively read from as a means of displaying debug information, among other things.
- [x] Provide flexible audio with easily manageable streams, voices, and mastering for each.
- [x] Provide command-line arguments for those who enjoy them, and to facilitate headless runs in the future (for the likes of unit tests, etc).
- [x] Implement more color types, conversions between them, lerps, waveforms, and other color/match related features for flexibility.
- [ ] Find time to focus entirely on finishing the Gameboy (Color) core.
