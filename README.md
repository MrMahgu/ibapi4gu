# ibapi4gu

Cmake preset build instructions

**Defaults** (win, wsl, linux)

- IBKR TWS API: 1051.01
- Protobuf: 5.29.5
- IntelRDFPMathLib: 2.3

The IBKR archive contains the sources for every supported platform. CMake maps
version `1051.01` to `twsapi_1051_01.zip` when deriving the download URL.

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
ibkr twsapi version: 1051.01
ibkr client version: 66
protobuf version: 5.29.5
ibkr socket ok: no
```

Windows (`.\build\win-msvc\rundir\bin\gw_scanner.exe`):
```
[scanner]
ibkr twsapi version: 1051.01
ibkr client version: 66
protobuf version: 5.29.5
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
ibkr twsapi version: 1051.01
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

**Changing IBAPI version**

You can change the version of IBKR API during configure if it's already cached

WSL / Linux:
```bash
cmake --preset linux-gcc -DIBKR_FETCH_TWSAPI=ON -DIBKR_TWSAPI_VERSION="1051.01"
cmake --build --preset linux-gcc
```

**Notes**

- `IBKR_FETCH_TWSAPI` is required when you want to download/extract the TWS API source for the first time or re-fetch it.
- `IBKR_TWSAPI_URL` can override the version-derived TWS API zip URL.
- `IBKR_TWSAPI_SHA256` can verify the downloaded zip when set to its SHA-256 digest.
- `IBAPI_PROTOBUF_VERSION` selects the Protobuf version used by the build.
- Presets are configured to use Ninja and Ninja Multi-Config.
- Boost and Protobuf may take up to 5 minutes or more to download.
- Build output (executable) will be found in `build/{preset}/rundir/bin`.

**Simple test app**

`apps/gw_scanner/main.cpp` is a simple sample app. The `ibkr socket ok: no` output is expected because it isn’t connected.
