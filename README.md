# portal-web

Share your screen (no sound) on local network, using (modern) browser as viewer.

This project aims to provide all major desktop platforms (Windows, MacOS, Linux) with a simple
solution to share your screen with others on the local network, with low latency when possible.

Currently only platforms listed below are supported.

- Linux
  - X11
  - FBDEV (linux framebuffer)
- Windows
  - GDI

The screenshot API (libportal) can be used as a library, see below.

## Build

Any c compiler is required for building portal-web.

Examples below assume that output directory is `./build` under project root.
Change it to whatever you like.

### Using CMAKE

Supported platforms: Linux, Windows.

```bash
mkdir build
cd build

cmake ..
make
```

### Using posix shell under Unix-like

Supported platforms: Linux, Cygwin.

```bash
mkdir build
cd build

../compile.sh
```

To enable platform A, set envar `PLATFORM_A` to `on`.
To disable platform A, set envar `PLATFORM_A` to `off`.
To modify C compiler path & c flags & ldflags, set envars `CC` & `CFLAGS` & `LDFLAGS`.

To forcely build for a specific platform, set envar `OS` to `linux`, `windows` or `cygwin`.

### Using cmd under Windows

Supported platform: Windows.

```bat
mkdir build
cd build

..\compile_win
```

To change C compiler, do something like this:

```bat
set CC=gcc && ..\compile_win
```

Also change `CFLAGS` and `LDFLAGS` to modify c flags and ld flags.

## Usage

```bash
./portalweb [options]
```

Details for options can be shown using `portalweb -h`.

## Library

The screenshot API (libportal under `src/libportal`) can be used as a single-header library.
You can directly embed `libportal.h` into your project.

# License

MIT

