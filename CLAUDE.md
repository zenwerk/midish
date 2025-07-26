# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Midish is an open-source MIDI sequencer/filter for Unix-like operating systems. It's implemented as a command-line interpreter focused on being lightweight, fast, and reliable for real-time performance.

## Build Commands

```bash
# Configure the build (Linux uses ALSA by default, OpenBSD uses sndio)
./configure

# Build the project
make

# Run tests
make check

# Install (requires appropriate permissions)
make install

# Clean build artifacts
make clean

# Run a single test
cd regress && ./run-test <testname>.cmd
```

## Architecture Overview

The codebase follows a modular C architecture with clear separation of concerns:

- **Core Event System**: `ev.c/h` - MIDI event representation and handling
- **Multiplexer**: `mux.c/h` - MIDI device multiplexing and routing
- **Track Management**: `track.c/h`, `song.c/h` - Sequencer tracks and song structure
- **MIDI Devices**: `mididev.c/h`, `mdep_*.c` - Platform-specific MIDI device backends (ALSA, sndio, raw)
- **Filtering**: `filt.c/h` - Real-time MIDI filtering and transformation
- **Command Interface**: `user.c/h`, `builtin.c/h` - Command-line interpreter and built-in commands
- **State Management**: `state.c/h`, `frame.c/h` - Playback state and frame management
- **File I/O**: `smf.c/h` (Standard MIDI Files), `saveload.c/h` (native format)

The main entry point is in `main.c`, which initializes the system and calls `user_mainloop()` to start the command interpreter.

## Key Design Patterns

1. **Event-Driven Architecture**: MIDI events flow through the mux system with real-time filtering
2. **Command Pattern**: User commands are parsed and executed through the exec/builtin system
3. **Pool Memory Management**: `pool.c/h` provides efficient memory allocation for real-time operations
4. **Undo System**: `undo.c/h` implements unlimited undo functionality

## Testing

Tests are located in the `regress/` directory. Each test consists of:
- `.cmd` file: Commands to execute
- `.res` file: Expected results
- `.msh` file: Test data files

The test runner (`regress/run-test`) executes commands in batch mode and compares actual vs expected results.

## Platform-Specific Notes

- Linux: Uses ALSA sequencer API by default (`mdep_alsa.c`)
- OpenBSD: Uses sndio API by default (`mdep_sndio.c`)
- Fallback: Raw MIDI device support (`mdep_raw.c`)

The configure script automatically detects the platform and sets appropriate defaults.