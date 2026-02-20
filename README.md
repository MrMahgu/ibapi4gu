# ibapi4gu

Cmake preset build instructions

**Defaults** (win, wsl, linux)

- IBKR TWS API: 1044.01
- Protobuf: 5.29.5
- IntelRDFPMathLib: 2.3
- Boost 1.90.0

`linux-gcc` uses the Windows IBAPI sources. If you want the Linux IBAPI sources, use `unix-gcc` and set `IBAPI_PROTOBUF_VERSION=3.12.4` during the initial configure (see below).

**Requirements (Win32)**

- Windows 11 (latest)
- Visual Studio 2026 Community Edition (latest)
  - Workload: `Desktop development with C++`
  - Individual components:
    - `MSVC Build Tools for x64/x86 (Latest)`
    - `C++ ATL for x64/x86 (Latest MSVC)`
    - `C++ CMake tools for Windows`
    - `Windows 11 SDK (10.0.22621.0)`

**Requirements (wsl/linux)**

- ninja
- msitools
- cmake 3.28.3
- gcc 13.3.0

**First time / after a clean build directory**

Windows (MSVC):
```bash
cmake --preset win-msvc -DIBKR_FETCH_TWSAPI=ON
cmake --build --preset win-msvc
```

WSL / Linux:
```bash
cmake --preset linux-gcc -DIBKR_FETCH_TWSAPI=ON
cmake --build --preset linux-gcc
```

Example output:

Linux (`build/linux-gcc/rundir/bin/gw_scanner`):
```
[scanner]
boost version: 1.90.0
boost hw_concurrency: 16
ibkr twsapi version: 1043.02
ibkr client version: 66
protobuf version: 5.29.5
ibkr socket ok: no
```

Windows (`.\build\win-msvc\rundir\bin\gw_scanner.exe`):
```
[scanner]
boost version: 1.90.0
boost hw_concurrency: 16
ibkr twsapi version: 1043.02
ibkr client version: 66
protobuf version: 5.29.5
ibkr socket ok: no
```

Linux/Unix/mac? (uses Linux IBAPI sources):
```bash
cmake --preset unix-gcc -DIBKR_FETCH_TWSAPI=ON -DIBAPI_PROTOBUF_VERSION=3.12.4
cmake --build --preset unix-gcc
```

Example output:

Linux/Unix (unix-gcc on Ubuntu, `build/unix-gcc/rundir/bin/gw_scanner`):
```
[scanner]
boost version: 1.90.0
boost hw_concurrency: 4
ibkr twsapi version: 1043.02
ibkr client version: 66
protobuf version: 3.12.4
ibkr socket ok: no
```

Windows (Visual Studio 2022/2026)

```bash
cmake --preset win-msvc-vs -DIBKR_FETCH_TWSAPI=ON
cmake --build --preset win-msvc-vs
```

Open in Visual Studio

```bash
start .\build\win-msvc-vs\ibapi_stack.sln
```

You can build it on the command line as well

```bash
cmake --build --preset win-msvc-vs
```

Example output:

Windows(.\build\win-msvc-vs\runtdir\bin\gw_scanner.exe)
```
[scanner]
boost version: 1.90.0
boost hw_concurrency: 16
ibkr twsapi version: 1043.02
ibkr client version: 66
protobuf version: 5.29.5
ibkr socket ok: no
```

**Subsequent builds (after the deps are cached)**

You can drop `IBKR_FETCH_TWSAPI` and `IBAPI_PROTOBUF_VERSION` once they have been fetched/configured:
```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc
```

**Changing IBAPI verison**

You can change the version of IBKR API during configure if it's already cached

WSL / Linux:
```bash
cmake --preset linux-gcc -DIBKR_FETCH_TWSAPI=ON -DIBKR_TWSAPI_VERSION="1044.01"
cmake --build --preset linux-gcc
```

**Notes**

- `IBKR_FETCH_TWSAPI` is required when you want to download/extract the TWS API source for the first time or re-fetch it.
- `IBKR_BOOST_VERSION` selects the Boost version used by the build.
- `IBAPI_PROTOBUF_VERSION` selects the Protobuf version used by the build.
- Presets are configured to use Ninja and Ninja Multi-Config.
- Boost and Protobuf may take up to 5 minutes or more to download.
- Build output (executable) will be found in `build/{preset}/rundir/bin`.

**Simple test app**

`apps/gw_scanner/main.cpp` is a simple sample app. The `ibkr socket ok: no` output is expected because it isn’t connected.
