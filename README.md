# ibapi4gu

Reproducible CMake integration for the Interactive Brokers C++ TWS API on
x86-64 Linux/WSL and Windows. The project checks out a pinned release from
IBKR's private Git repository, builds its exact dependency set, and exposes a
single CMake target: `ibapi4gu::twsapi`.

> [!IMPORTANT]
> Building this project requires an IBKR-authorized GitHub account with access
> to the private
> [`InteractiveBrokers/tws-api`](https://github.com/InteractiveBrokers/tws-api)
> repository and an SSH key configured for that account. The public IBKR
> installer download is not used.

## Dependency baseline

| Component | Windows-generated sources | Unix-generated sources |
| --- | --- | --- |
| IBKR TWS API | 10.51.01 | 10.51.01 |
| Protobuf | 5.29.5 | 3.12.4 |
| Intel Decimal Floating-Point Math Library | 2.0 Update 4 | 2.0 Update 4 |

The IBKR tag contains the `.proto` inputs and two sets of generated C++ files.
Protobuf C++ requires generated code and runtime versions to match exactly, so
do not override the selected Protobuf version.

## Linux requirements

Install the standard native build tools:

```bash
sudo apt install git build-essential gcc-14 g++-14 ninja-build ca-certificates
```

Install CMake 4.2 or newer from Kitware when the distribution package is older.

Configuration requires SSH access to IBKR's private repository. Verify the
GitHub account and SSH key before configuring:

```bash
git ls-remote git@github.com:InteractiveBrokers/tws-api.git refs/tags/1051.01
```

No system Boost, Protobuf, `protoc`, or `unzip` package is required. The
compiler must support C++26 (GCC 14+ is the tested baseline).

## Build and test

The normal Linux preset uses IBKR's Unix-generated Protobuf sources:

```bash
CC=gcc-14 CXX=g++-14 cmake --workflow --preset linux-gcc
```

To exercise IBKR's Protobuf 5.29.5 generated sources under Linux or WSL:

```bash
CC=gcc-14 CXX=g++-14 cmake --workflow --preset linux-gcc-modern
```

On Windows, install Visual Studio 2026 with Desktop development with C++,
CMake 4.2 or newer, and the Windows 11 SDK. C++26 support currently uses
MSVC's preview `/std:c++latest` mode. Choose Ninja Multi-Config or the Visual
Studio generator:

```powershell
cmake --workflow --preset windows-msvc
cmake --workflow --preset windows-vs2026
```

The first configure shallow-clones and verifies the pinned IBKR tag, then
downloads Protobuf and Intel RDFP. Later builds reuse `build/<preset>/_deps`.
The example program is written to `build/<preset>/bin` on Linux and
`build/<preset>/bin/Release` for multi-config generators.

Example output:

```text
[ibapi4gu]
ibkr twsapi version: 1051.01
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

- `IBAPI4GU_IBKR_DISTRIBUTION=UNIX|WINDOWS` selects the matching generated
  Protobuf source set from the checkout. It defaults to the native set for the
  host.
- `IBAPI4GU_IBKR_SOURCE_DIR=/path/to/tws-api` skips the private Git fetch and
  uses an existing checkout of the matching 10.51.01 tag.
- `IBAPI4GU_BUILD_EXAMPLES=OFF` disables the example; it already defaults off
  when this project is a subdirectory.
- `BUILD_TESTING=OFF` disables this project's tests.

If a parent already defines `protobuf::libprotobuf`, its runtime version must
exactly match the selected IBKR generated sources or configuration stops with
an error.

## Maintainer upgrade checklist

IBKR versions are deliberately not free-form cache options because the Git tag
and commit, generated Protobuf versions, dependency checksums, and local patches
must move together. To adopt a new release:

1. Locate the release tag in `InteractiveBrokers/tws-api` and record its exact
   commit ID.
2. Inspect a generated `.pb.h` from both C++ source sets and pin the matching
   exact Protobuf runtime artifacts and checksums.
3. Review `client/Decimal.cpp`. Remove or rebase the guarded patch only after
   the decimal regression suite passes.
4. Update `cmake/Ibapi4guRelease.cmake` as one change.
5. Run the native Linux, modern-Protobuf Linux, and Windows test matrix locally
   from clean build directories.

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
