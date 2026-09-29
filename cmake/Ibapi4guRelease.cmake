include_guard(GLOBAL)

# A release is an indivisible set: IBKR generated C++ requires the exact
# Protobuf runtime used to generate it. Update these values together.
set(IBAPI4GU_IBKR_VERSION "1051.01")

set(IBAPI4GU_IBKR_URL
  "https://interactivebrokers.github.io/downloads/twsapi_1051_01.zip"
)
set(IBAPI4GU_IBKR_SHA256
  "cc5179e83d9056076bed62c062dc9ed5db0b7f21b2e76d9f51d759e120ee9143"
)
set(IBAPI4GU_IBKR_PROTOBUF_DIR "protobuf")
set(IBAPI4GU_REQUIRED_PROTOBUF_VERSION "5.29.5")
set(IBAPI4GU_REQUIRED_PROTOBUF_NUMERIC_VERSION "5029005")
set(IBAPI4GU_IBKR_PROTOBUF_MARKER "#if PROTOBUF_VERSION != 5029005")

set(IBAPI4GU_PROTOBUF_5_29_5_URL
  "https://github.com/protocolbuffers/protobuf/releases/download/v29.5/protobuf-29.5.tar.gz"
)
set(IBAPI4GU_PROTOBUF_5_29_5_SHA256
  "a191d2afdd75997ba59f62019425016703daed356a9d92f7425f4741439ae544"
)
set(IBAPI4GU_ABSEIL_VERSION "20240116.0")
set(IBAPI4GU_ABSEIL_URL
  "https://github.com/abseil/abseil-cpp/archive/refs/tags/20240116.0.tar.gz"
)
set(IBAPI4GU_ABSEIL_SHA256
  "338420448b140f0dfd1a1ea3c3ce71b3bc172071f24f4d9a57d59b45037da440"
)

set(IBAPI4GU_INTEL_RDFP_VERSION "2.0 Update 4")
set(IBAPI4GU_INTEL_RDFP_URL
  "https://www.netlib.org/misc/intel/IntelRDFPMathLib20U4.tar.gz"
)
set(IBAPI4GU_INTEL_RDFP_SHA256
  "1df86132e7a31fd74d784fee1c679b21a088f73a8ec979cfaf784c200392e125"
)

set(IBAPI4GU_IBKR_DECIMAL_SHA256
  "9fa02137f50ec7c4753b50acd46471efa852928028e02018ccc8896b1b364678"
)
