#include <iostream>

#include <google/protobuf/stubs/common.h>

#include <client/EClientSocket.h>

int main() {
    std::cout << "[ibapi4gu]\n";
    std::cout << "ibkr twsapi version: " << IBAPI4GU_TWSAPI_VERSION << '\n';
    std::cout << "ibkr client version: "
              << ibapi::client_constants::CLIENT_VERSION << '\n';
    std::cout << "protobuf version: "
              << google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION)
              << '\n';

    EClientSocket client(nullptr);
    std::cout << "ibkr socket ok: " << (client.isSocketOK() ? "yes" : "no")
              << '\n';
}
