# ibapi4gu

Reproducible CMake integration for the Interactive Brokers C++ TWS API on
x86-64 Linux/WSL and Windows. The project checks out a pinned revision from
IBKR's private Git repository and builds its exact dependency set. It exposes
two CMake targets:

- `ibapi4gu::twsapi` is the unchanged raw IBKR C++ library.
- `ibapi4gu::api` is a normalized shared-session API linked to `twsapi`.

> [!IMPORTANT]
> Building this project requires an IBKR-authorized GitHub account with access
> to the private
> [`InteractiveBrokers/tws-api`](https://github.com/InteractiveBrokers/tws-api)
> repository and an SSH key configured for that account. The public IBKR
> installer download is not used.

## Protobuf build matrix

| Preset | IBKR revision | Generated source set | Protobuf C++ |
| --- | --- | --- | --- |
| `linux-gcc-pb3` | `34bc74d` (`1051.01`) | Unix | 3.12.4 |
| `linux-gcc-pb5` | `34bc74d` (`1051.01`) | Windows | 5.29.5 |
| `linux-gcc-pb6` | `6232d6b` (future commit) | Windows | 6.33.4 |
| `linux-gcc-pb7` | `6232d6b` (future commit) | Unix | 7.36.1 |

Both revisions contain the `.proto` inputs and two sets of generated C++
files. Protobuf C++ requires generated code and runtime versions to match
exactly, so do not override the selected Protobuf version. All profiles use
Intel Decimal Floating-Point Math Library 2.0 Update 4.

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

No system Boost, Protobuf, `protoc`, or `unzip` package is required. C++26 is
the default; C++11, 14, 17, 20, and 23 can also be selected. The dependency
profile determines the oldest supported mode:

| Protobuf C++ | Oldest language mode |
| --- | --- |
| 3.12.4 | C++11 |
| 5.29.5 | C++14 |
| 6.33.4 or 7.36.1 | C++17 |

## Build and test

Run any profile independently under Linux or WSL:

```bash
CC=gcc-14 CXX=g++-14 cmake --workflow --preset linux-gcc-pb3
CC=gcc-14 CXX=g++-14 cmake --workflow --preset linux-gcc-pb5
CC=gcc-14 CXX=g++-14 cmake --workflow --preset linux-gcc-pb6
CC=gcc-14 CXX=g++-14 cmake --workflow --preset linux-gcc-pb7
```

Select another language mode at configure time. A build directory represents
one language mode because Protobuf and Abseil must be compiled consistently:

```bash
cmake --preset linux-gcc-pb7 \
  -B build/linux-gcc-pb7-cxx20 \
  -DIBAPI4GU_CXX_STANDARD=20
cmake --build build/linux-gcc-pb7-cxx20
ctest --test-dir build/linux-gcc-pb7-cxx20 --output-on-failure
```

On Windows, install Visual Studio 2026 with Desktop development with C++,
CMake 4.2 or newer, and the Windows 11 SDK. When C++26 is selected, CMake uses
MSVC's preview `/std:c++latest` mode. Choose Ninja Multi-Config or the Visual
Studio generator:

```powershell
cmake --workflow --preset windows-msvc
cmake --workflow --preset windows-vs2026
```

The first configure fetches and verifies the profile's exact IBKR commit, then
downloads Protobuf, Abseil when required, and Intel RDFP. Later builds reuse
`build/<preset>/_deps`. The example program is written to
`build/<preset>/bin` on Linux and
`build/<preset>/bin/Release` for multi-config generators.

Example output from the newest native Unix profile under Ubuntu on WSL:

```text
[ibapi4gu]
ibkr twsapi version: 1051.01
ibkr client version: 66
protobuf version: 7.36.1
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

Link `ibapi4gu::api` instead for the high-level session facade:

```cpp
#include <ibapi4gu/api.hpp>

ibapi4gu::Result<std::shared_ptr<ibapi4gu::Session> > connected =
    ibapi4gu::Session::connect();
if (connected) {
  ibapi4gu::Request<std::vector<ibapi4gu::ContractDetails> > request =
      connected.value()->contract_details(ibapi4gu::Stock("AAPL"));
  ibapi4gu::Result<std::vector<ibapi4gu::ContractDetails> > details =
      request.get();
}
```

Connection becomes ready only after `nextValidId`. Requests are cancellable,
time out after 30 seconds by default, and cancel when abandoned. Requests also
support readiness checks, timed waits, nonblocking retrieval, and completion
callbacks scheduled through a caller-provided executor. Raw calls can share
the same serialized connection through `session->ibkr().post(...)`; its
`Request<void>` reports execution or failure, while an ignored result keeps
the original fire-and-forget behavior.
See [`docs/session-api.md`](docs/session-api.md) for threading, ID allocation,
historical bars, error classification, raw observer, and Protobuf profile
rules.

Historical bars are exposed as a finite normalized request:

```cpp
ibapi4gu::HistoricalBarsOptions history;
history.duration = ibapi4gu::HistoricalDuration(
    5, ibapi4gu::HistoricalDurationUnit::days);
history.bar_size = ibapi4gu::HistoricalBarSize::five_minutes;

ibapi4gu::Request<std::vector<ibapi4gu::HistoricalBar> > request =
    connected.value()->historical_bars(ibapi4gu::Stock("AAPL"), history);
ibapi4gu::Result<std::vector<ibapi4gu::HistoricalBar> > bars = request.get();
```

Automated historical-data tests use synthetic backend callbacks. A live TWS
or IB Gateway check remains manual and requires the relevant IBKR market-data
permissions.

The manual `ibapi4gu_contract_details` example accepts `HOST PORT CLIENT_ID
SYMBOL`. It is built with the examples but is intentionally not registered as
an automatic test because it requires a running TWS or IB Gateway.

Useful configuration variables:

- `IBAPI4GU_CXX_STANDARD=11|14|17|20|23|26` selects one language mode for the
  API, its pinned dependencies, and consumers. It defaults to `26`; profiles
  reject modes older than their matching Protobuf runtime supports.
- `IBAPI4GU_IBKR_DISTRIBUTION=UNIX|WINDOWS` selects the matching generated
  Protobuf source set from the checkout. It defaults to the native set for the
  host.
- `IBAPI4GU_IBKR_GENERATION=STABLE|FUTURE` selects the pinned parent or child
  IBKR commit. It defaults to `STABLE`.
- `IBAPI4GU_IBKR_SOURCE_DIR=/path/to/tws-api` skips the private Git fetch and
  uses an existing checkout of the selected revision.
- `IBAPI4GU_BUILD_EXAMPLES=OFF` disables the example; it already defaults off
  when this project is a subdirectory.
- `BUILD_TESTING=OFF` disables this project's tests.

If a parent already defines `protobuf::libprotobuf`, its runtime version must
exactly match the selected IBKR generated sources or configuration stops with
an error.

## Maintainer upgrade checklist

IBKR versions are deliberately not free-form cache options because each Git
commit, generated Protobuf version, dependency checksum, and local patch must
move together. To adopt a new revision:

1. Locate the release tag in `InteractiveBrokers/tws-api` and record its exact
   commit ID.
2. Inspect a generated `.pb.h` from both C++ source sets and pin the matching
   exact Protobuf runtime artifacts and checksums.
3. Review `client/Decimal.cpp`. Remove or rebase the guarded patch only after
   the decimal regression suite passes.
4. Update `cmake/Ibapi4guRelease.cmake` as one change.
5. Run all four Protobuf profiles and the Windows profiles locally from clean
   build directories.

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
