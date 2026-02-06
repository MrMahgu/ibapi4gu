#include <iostream>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/thread.hpp>
#include <boost/version.hpp>

#include <google/protobuf/stubs/common.h>

#include "client/EClientSocket.h"

using namespace std;

static string boost_version_string() {
    const int v = BOOST_VERSION;
    const int major = v / 100000;
    const int minor = (v / 100) % 1000;
    const int patch = v % 100;
    return to_string(major) + "." + to_string(minor) + "." + to_string(patch);
}

int main() {
    cout << "[scanner]\n";

    cout << "boost version: " << boost_version_string() << "\n";
    cout << "boost hw_concurrency: " << boost::thread::hardware_concurrency() << "\n";

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
