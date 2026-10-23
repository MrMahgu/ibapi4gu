# Third-party notices

`ibapi4gu` fetches third-party sources during configuration. Those sources are
not committed to this repository and remain governed by their own licenses and
terms.

## Interactive Brokers TWS API

- Source: <https://github.com/InteractiveBrokers/tws-api>
- Pinned revisions: API 10.51.01 commit
  `34bc74df9f064d5bfde2ed18810f0db12a45acab` and future commit
  `198ce538b300be86929a197f725d8335fb8273a3`
- The bundled C++ sources contain Interactive Brokers LLC copyright notices
  and GNU General Public License, version 3 or later, headers.
- Use of the TWS API is also subject to the terms presented by Interactive
  Brokers on its API download site.

The build creates a patched copy of `Decimal.cpp` in the build directory. It
preserves the vendor copyright/license header and is compiled only after the
original source matches the reviewed release hash.

## Protocol Buffers

- Source: <https://github.com/protocolbuffers/protobuf>
- Pinned C++ runtime releases: 3.12.4, 5.29.5, 6.33.4, and 7.36.1
- License: BSD 3-Clause; see the `LICENSE` file in each downloaded archive.

## Abseil C++

- Source: <https://github.com/abseil/abseil-cpp>
- Pinned releases: 20240116.0 for Protobuf 5.29.5 and 20250512.1 for
  Protobuf 6.33.4 and 7.36.1.
- License: Apache License 2.0; see the `LICENSE` file in the downloaded archive.

## Intel Decimal Floating-Point Math Library

- Source: <https://www.intel.com/content/www/us/en/developer/articles/tool/intel-decimal-floating-point-math-library.html>
- Pinned release: 2.0 Update 4
- The official source archive includes `eula.txt`; review it before use or
  redistribution.

## Project license status

No license has yet been selected for the original `ibapi4gu` build scripts,
examples, tests, or documentation. This notice records provenance but does not
grant rights beyond the applicable third-party terms.
