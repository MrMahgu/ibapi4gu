#include <iostream>
#include <string_view>

#include <google/protobuf/stubs/common.h>

#include <client/EClientSocket.h>

int main() {
    if (GOOGLE_PROTOBUF_VERSION != IBAPI4GU_EXPECTED_PROTOBUF_VERSION) {
        std::cerr << "unexpected Protobuf runtime: " << GOOGLE_PROTOBUF_VERSION
                  << '\n';
        return 1;
    }
    if (std::string_view(IBAPI4GU_TWSAPI_VERSION) != "1050.02") {
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
