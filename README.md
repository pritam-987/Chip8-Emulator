# Chip8-Emulator

A CHIP8 emulator in C and SDL.

## Features

- CHIP8 CPU and instructions
- 4 kb ram
- mono sound and color lerp
- keyboard support
- pause and restart controls

## Requirements

- C17 compiler
- SDL2
- Make

## Build

```bash
    make
```

## Run

```bash
    ./chip8 <rom>
```

## controls

- SPACE — Pause / Resume
- Restart
- ESC — Quit
- J/K — Change color lerp rate
- O/P — Change volume

- chip8 keypad:
  1 2 3 4
  Q W E R
  A S D F
  Z X C V

### Windows

- Download the latest release.
- Extract the ZIP.
- Drag and drop a `.ch8` file to the `run.bat` file.
