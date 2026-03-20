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
- [x] Utilize TOML for storing/loading application settings.
- [x] Utilize ImGUI to get a rudimentary and reactive interface that replaces the old single-window video output.
- [x] Redesign frontend to decouple the main thread logic from the emulation worker thread(s).
- [x] Implement thread-allocation logic to manage CPU thread utilization on demand and ensure the GUI/main thread will never conflict with an emulation thread.
- [x] Refactor the System Interface and System Host to allow for concurrent emulation of multiple systems.
- [x] Utilize mailbox-style triple-buffering to decouple emulation frame rendering from frontend display rendering.
- [x] Allow compiling and displaying rudimentary OSD statistics on a per-system basis.
- [x] Implement File Picker dialog for when drag-n-drop doesn't work, for some reason.
- [x] Refactor the basic logger to safely receive cross-thread messages, and safely write to file via a threaded time/load-checking sink. Also implement Logger window to view logs in real-time.
- [x] Refactor cmake build script for smarter generation and cached vendoring of fetched third party libraries.
- [x] Allow free selection of which family/system core to boot a game file with, whether it is eligible for that core or not.
- [x] Provide flexible audio with easily manageable streams, voices, and mastering for each.
- [x] Provide command-line arguments for those who enjoy them, and to facilitate headless runs in the future (for the likes of unit tests, etc).
- [x] Implement more color types, conversions between them, lerps, waveforms, and other color/math related features for general use.
- [ ] Start and maintain a JSON (or TOML??) database, keyed by SHA1 hashes for system families lacking a concrete identifier mark, as well as to define defaults/overrides on a per-entry basis.
- [ ] Redesign the SDL input to allow for more forms of input (mouse, gamepads, etc) and binding control beyond a "X = Y or Z". This will require interaction with ImGUI to allow live mapping and feedback.
- [ ] By extension, also need to implement a hotkey manager that will be remappable live, and allow for hot-swapping new entries depending on which system family is currently active.
- [ ] Implement stepping framework for debugging, initially limited to simple full-frame and per-instruction stepping, for the system families/cores already implemented.
- [ ] Add configuration ImGUI panels for each family, extended optionally by each system core, to allow for live adjustment of various properties/quirks.
- [ ] Similarly, add further GUI controls for standard actions, such as pausing a system, toggling OSD, etc.
- [ ] Further, design standard ImGUI widgets to recycle for various uses, particularly in debugging tools, such as wave/pattern visualizers, memory viewers, disasm viewers, etc.
- [ ] Start work on supporting initializing system instances and feeding them custom data, and controlling exact runtime parameters to allow for actual support of single-step-tests later.
- [ ] Figure out a good way to serialize, version, and differentiate the full state of different systems for savestate support, with modular assembly of each eligible component of a system at the lower end, and also the inheritance layers at the high end.
- [ ] Modify the audio backend to allow for streams with differing stream io formats, rather than being restricted to having the same on both ends. Required for BytePusher running under custom framerate.
- [ ] Find time to focus entirely on progressing the Gameboy (Color) family interface and system cores.
