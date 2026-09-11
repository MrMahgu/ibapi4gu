# ibapi4gu

Reproducible CMake integration for the Interactive Brokers C++ TWS API on
x86-64 Linux/WSL and Windows. The project downloads verified source artifacts,
builds their exact dependency set, and exposes a single CMake target:
`ibapi4gu::twsapi`.

## Dependency baseline

| Component | Windows distribution | Unix distribution |
| --- | --- | --- |
| IBKR TWS API | 10.50.02 | 10.50.02 |
| Protobuf | 5.29.5 | 3.12.4 |
| Intel Decimal Floating-Point Math Library | 2.0 Update 4 | 2.0 Update 4 |

The Protobuf versions intentionally differ. IBKR bundles generated C++ files,
not their `.proto` inputs, and Protobuf C++ requires generated code and runtime
versions to match exactly. Do not override the selected Protobuf version.

## Linux requirements

Install the standard native build tools:

```bash
sudo apt install git build-essential cmake ninja-build ca-certificates
```

The optional Windows-source build on Linux/WSL also needs `msiextract`:

```bash
sudo apt install msitools
```

No system Boost, Protobuf, `protoc`, or `unzip` package is required. The minimum
supported CMake version is 3.28.3 and the compiler must support C++20 (GCC 13+
is the tested baseline).

## Build and test

The normal Linux preset uses IBKR's native Unix source archive:

```bash
cmake --workflow --preset linux-gcc
```

To build the Windows IBKR source distribution under Linux or WSL:

```bash
cmake --workflow --preset linux-gcc-msi
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
ibkr twsapi version: 1050.02
ibkr client version: 66
protobuf version: 3.12.4
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

- `IBAPI4GU_IBKR_DISTRIBUTION=UNIX|WINDOWS` selects the source package. It
  defaults to the native package for the host.
- `IBAPI4GU_IBKR_SOURCE_DIR=/path/to/extracted/source` skips the IBKR download
  and uses a matching, already-extracted 10.50.02 distribution.
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

1. Download both official IBKR distributions and calculate their SHA-256 sums.
2. Inspect an included generated `.pb.h` from each distribution and pin the
   corresponding exact Protobuf runtime artifact and checksum.
3. Review `client/Decimal.cpp`. Remove or rebase the guarded patch only after
   the decimal regression suite passes.
4. Update `cmake/Ibapi4guRelease.cmake` as one change.
5. Run the native Linux, Linux MSI, and Windows CI matrix from clean build
   directories.

## Vendor patch and licensing

IBKR 10.50.02's `Decimal.cpp` passes uninitialized status words to Intel RDFP
and mishandles positive scientific exponents. The build creates a corrected
copy only after the vendor file matches its reviewed SHA-256. Details are in
[`docs/ibkr-decimal-patch.md`](docs/ibkr-decimal-patch.md).

The repository does not yet declare a license for its original build code.
Downloaded dependencies and the version-scoped vendor patch remain subject to
their respective terms. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
Do not publish packaged binaries without completing the project's licensing
review.
