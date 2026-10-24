#include <ibapi4gu/api.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: " << argv[0]
              << " HOST PORT CLIENT_ID SYMBOL\n";
    return 2;
  }

  ibapi4gu::SessionOptions options;
  options.host = argv[1];
  options.port = std::atoi(argv[2]);
  options.client_id = std::atoi(argv[3]);

  ibapi4gu::Result<std::shared_ptr<ibapi4gu::Session> > connected =
      ibapi4gu::Session::connect(options);
  if (!connected) {
    std::cerr << "connection failed: " << connected.error().message << '\n';
    return 1;
  }

  std::shared_ptr<ibapi4gu::Session> session = connected.value();
  ibapi4gu::Request<std::vector<ibapi4gu::ContractDetails> > request =
      session->contract_details(ibapi4gu::Stock(argv[4]));
  ibapi4gu::Result<std::vector<ibapi4gu::ContractDetails> > result =
      request.get();
  if (!result) {
    std::cerr << "request failed: " << result.error().message << '\n';
    return 1;
  }

  for (std::vector<ibapi4gu::ContractDetails>::const_iterator iterator =
           result.value().begin();
       iterator != result.value().end(); ++iterator) {
    std::cout << iterator->contract.symbol << "\t" << iterator->long_name
              << "\t" << iterator->contract.exchange << '\n';
  }
  session->shutdown();
  return 0;
}
