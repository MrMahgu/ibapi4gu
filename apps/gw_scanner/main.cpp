#include <iostream>

#include <google/protobuf/stubs/common.h>

#include "client/EClientSocket.h"

using namespace std;

int main() {
    cout << "[scanner]\n";

#ifdef IBKR_TWSAPI_VERSION
    cout << "ibkr twsapi version: " << IBKR_TWSAPI_VERSION << "\n";
#else
    cout << "ibkr twsapi version: (unknown)\n";
#endif
    cout << "ibkr client version: " << ibapi::client_constants::CLIENT_VERSION << "\n";

    cout << "protobuf version: "
         << google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION)
         << "\n";

    EClientSocket client(nullptr);
    cout << "ibkr socket ok: " << (client.isSocketOK() ? "yes" : "no") << "\n";

    return 0;
}
