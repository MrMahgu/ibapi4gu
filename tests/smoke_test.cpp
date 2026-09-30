#include <iostream>
#include <string_view>

#include <google/protobuf/stubs/common.h>

#include <client/EClientSocket.h>

#if defined(_MSC_VER)
static_assert(_MSVC_LANG > 202302L,
              "ibapi4gu consumers must compile in MSVC C++ latest mode");
#else
static_assert(__cplusplus > 202302L,
              "ibapi4gu consumers must compile in C++26 mode");
#endif

int main() {
    if (GOOGLE_PROTOBUF_VERSION != IBAPI4GU_EXPECTED_PROTOBUF_VERSION) {
        std::cerr << "unexpected Protobuf runtime: " << GOOGLE_PROTOBUF_VERSION
                  << '\n';
        return 1;
    }
    if (std::string_view(IBAPI4GU_TWSAPI_VERSION) != "1051.01") {
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
