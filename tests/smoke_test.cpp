#include <iostream>
#include <string>

#include <google/protobuf/stubs/common.h>

#include <client/EClientSocket.h>

#if defined(_MSC_VER)
#  define IBAPI4GU_CXX_LANGUAGE_LEVEL _MSVC_LANG
#else
#  define IBAPI4GU_CXX_LANGUAGE_LEVEL __cplusplus
#endif

#if IBAPI4GU_EXPECTED_CXX_STANDARD >= 26
static_assert(IBAPI4GU_CXX_LANGUAGE_LEVEL > 202302L,
              "expected C++26/latest mode");
#elif IBAPI4GU_EXPECTED_CXX_STANDARD >= 23
static_assert(IBAPI4GU_CXX_LANGUAGE_LEVEL > 202002L, "expected C++23 mode");
#elif IBAPI4GU_EXPECTED_CXX_STANDARD >= 20
static_assert(IBAPI4GU_CXX_LANGUAGE_LEVEL >= 202002L, "expected C++20 mode");
#elif IBAPI4GU_EXPECTED_CXX_STANDARD >= 17
static_assert(IBAPI4GU_CXX_LANGUAGE_LEVEL >= 201703L, "expected C++17 mode");
#elif IBAPI4GU_EXPECTED_CXX_STANDARD >= 14
static_assert(IBAPI4GU_CXX_LANGUAGE_LEVEL >= 201402L, "expected C++14 mode");
#else
static_assert(IBAPI4GU_CXX_LANGUAGE_LEVEL >= 201103L, "expected C++11 mode");
#endif

int main() {
    if (GOOGLE_PROTOBUF_VERSION != IBAPI4GU_EXPECTED_PROTOBUF_VERSION) {
        std::cerr << "unexpected Protobuf runtime: " << GOOGLE_PROTOBUF_VERSION
                  << '\n';
        return 1;
    }
    if (std::string(IBAPI4GU_TWSAPI_VERSION) != "1051.01") {
        std::cerr << "unexpected IBKR version: " << IBAPI4GU_TWSAPI_VERSION
                  << '\n';
        return 1;
    }

    EClientSocket client(nullptr);
    if (client.isSocketOK()) {
        std::cerr << "a disconnected EClientSocket unexpectedly reports open\n";
        return 1;
    }
}
