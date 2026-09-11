# IBKR 10.50.02 decimal patch

The Windows MSI and native Unix ZIP for IBKR API 10.50.02 contain the same
`client/Decimal.cpp`, with SHA-256:

```text
9fa02137f50ec7c4753b50acd46471efa852928028e02018ccc8896b1b364678
```

Two correctness problems were confirmed during the repository audit:

1. All eight wrappers declare an `unsigned int flags` status word without
   initialization before passing its address to Intel RDFP. Intel routines can
   read or OR into that value, making the behavior undefined.
2. `decimalStringToDisplay` handles negative exponents but not positive ones.
   For example, the unmodified API renders `1E+2` as `1` and `-1E+3` as `-1`.

At configure time, `PatchDecimal.cmake` verifies the exact source hash, changes
each status word initialization to zero, and replaces the formatter with a
signed decimal-position implementation. The generated copy is stored only in
the build tree; the downloaded source remains untouched.

Regression coverage includes positive, negative, and zero exponents, ordinary
decimal strings, IBKR unset values, arithmetic operations, and double
conversion.

For every IBKR upgrade:

- If `Decimal.cpp` changes, configuration intentionally fails before patching.
- Review upstream behavior and the new source rather than updating the hash
  mechanically.
- Remove the patch when upstream passes the same regression cases.
- Otherwise, rebase it, record the new hash and findings, and run every source
  distribution/platform job.
