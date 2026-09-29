# ibapi4gu

Reproducible CMake integration for the Interactive Brokers C++ TWS API on
x86-64 Linux/WSL and Windows. The project downloads a verified source archive,
builds its exact dependency set, and exposes a single CMake target:
`ibapi4gu::twsapi`.

## Dependency baseline

| Component | Version |
| --- | --- |
| IBKR TWS API | 10.51.01 |
| Protobuf | 5.29.5 |
| Intel Decimal Floating-Point Math Library | 2.0 Update 4 |

IBKR bundles generated C++ files, not their `.proto` inputs, and Protobuf C++
requires generated code and runtime versions to match exactly. Do not override
the selected Protobuf version.

## Linux requirements

Install the standard native build tools:

```bash
sudo apt install git build-essential cmake ninja-build ca-certificates
```

No system Boost, Protobuf, `protoc`, or `unzip` package is required. The minimum
supported CMake version is 3.28.3 and the compiler must support C++20 (GCC 13+
is the tested baseline).

## Build and test

The Linux preset uses IBKR's unified cross-platform source archive:

```bash
cmake --workflow --preset linux-gcc
```

On Windows, install Visual Studio 2026 with Desktop development with C++,
CMake 4.2 or newer, and the Windows 11 SDK. Choose Ninja Multi-Config or the
Visual Studio generator:

```powershell
cmake --workflow --preset windows-msvc
cmake --workflow --preset windows-vs2026
```

The first configure downloads and verifies the selected IBKR source,
Protobuf, and Intel RDFP. Later builds reuse `build/<preset>/_deps`. The example
program is written to `build/<preset>/bin` on Linux and
`build/<preset>/bin/Release` for multi-config generators.

Example output:

```text
[ibapi4gu]
ibkr twsapi version: 1051.01
ibkr client version: 66
protobuf version: 5.29.5
ibkr socket ok: no
```

The disconnected socket result is expected; the example does not contact TWS
or IB Gateway.

## Use from another CMake project

Add this repository with `add_subdirectory()` or `FetchContent`, then link the
namespaced target:

```cmake
add_subdirectory(path/to/ibapi4gu)

target_link_libraries(my_application PRIVATE ibapi4gu::twsapi)
```

Useful configuration variables:

- `IBAPI4GU_IBKR_SOURCE_DIR=/path/to/extracted/source` skips the IBKR download
  and uses a matching, already-extracted 10.51.01 source tree.
- `IBAPI4GU_BUILD_EXAMPLES=OFF` disables the example; it already defaults off
  when this project is a subdirectory.
- `BUILD_TESTING=OFF` disables this project's tests.

If a parent already defines `protobuf::libprotobuf`, its runtime version must
exactly match the selected IBKR generated sources or configuration stops with
an error.

## Maintainer upgrade checklist

IBKR versions are deliberately not free-form cache options because source URLs,
checksums, generated Protobuf versions, and local patches must move together.
To adopt a new Latest API release:

1. Download the official IBKR archive and calculate its SHA-256 sum.
2. Inspect an included generated `.pb.h` and pin the corresponding exact
   Protobuf runtime artifact and checksum.
3. Review `client/Decimal.cpp`. Remove or rebase the guarded patch only after
   the decimal regression suite passes.
4. Update `cmake/Ibapi4guRelease.cmake` as one change.
5. Run the Linux and Windows CI matrix from clean build directories.

## Vendor patch and licensing

IBKR 10.51.01's `Decimal.cpp` passes uninitialized status words to Intel RDFP
and mishandles positive scientific exponents. The build creates a corrected
copy only after the vendor file matches its reviewed SHA-256. Details are in
[`docs/ibkr-decimal-patch.md`](docs/ibkr-decimal-patch.md).

The repository does not yet declare a license for its original build code.
Downloaded dependencies and the version-scoped vendor patch remain subject to
their respective terms. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
Do not publish packaged binaries without completing the project's licensing
review.
