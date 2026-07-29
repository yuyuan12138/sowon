[![Build Status](https://github.com/tsoding/sowon/workflows/CI/badge.svg)](https://github.com/tsoding/sowon/actions)

# Sowon

![demo](./demo.gif)

## Build

### Ubuntu

```console
$ sudo apt-get install -qq libgl-dev libxcursor-dev libxrandr-dev libxi-dev
$ make
$ ./sowon
```

### Fedora 42+

```console
$ sudo dnf install libXi-devel libXrandr-devel
$ make
$ ./sowon
```

### MacOS

```console
$ make
$ ./sowon
```

### Terminal UI only

```console
$ make tui
$ ./sowon-tui
```

### Windows

#### Visual Studio

- Enter the Visual Studio Command Line Development Environment <https://docs.microsoft.com/en-us/cpp/build/building-on-the-command-line>. Basically just find `vcvarsall.bat` and run `vcvarsall.bat x64` inside of cmd

```console
> cd path\to\sowon
> build_msvc
```

## Usage

### Modes

- Ascending mode: `./sowon`
- Descending mode: `./sowon <seconds>`
- Clock Mode: `./sowon clock`
- Terminal UI: `./sowon --tui <mode>` or `./sowon-tui <mode>`

### Flags

- Start in paused state: `./sowon -p <mode>`
- Exit sowon after countdown finished: `./sowon -e`
- Start in terminal UI mode: `./sowon -t` or `./sowon --tui`

### Key bindings

| Key | Description |
| --- | --- |
| <kbd>SPACE</kbd> | Toggle pause |
| <kbd>=</kbd> or <kbd>+</kbd> | Zoom in |
| <kbd>-</kbd> | Zoom out |
| <kbd>0</kbd> | Zoom 100% |
| <kbd>F5</kbd> | Restart |
| <kbd>F11</kbd> | Fullscreen |

Terminal UI also supports <kbd>R</kbd> to restart and <kbd>Q</kbd> to quit.
