#include <grpcpp/grpcpp.h>
#include <string>
#include "sendstrings.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using sendstrings::SendStrings;
using sendstrings::StringRequest;
using sendstrings::StringReply;

class SendStringsClient {
public:
    SendStringsClient(std::shared_ptr<Channel> channel) :
                         stub_(SendStrings::NewStub(channel)) {}

    std::string sendRequest(std::string a) {
        StringRequest request;
        request.set_sent(a);
        StringReply reply;
        ClientContext context;
        Status status = stub_->sendRequest(&context, request, &reply);
        if(status.ok()) {
            return "OK";
        } else {
            std::cout << status.error_code() << ": " << status.error_message();
            std::cout << std::endl;
            return "Rpc Failed";
        }
    }
private:
    std::unique_ptr<SendStrings::Stub> stub_;
};

void RunClient() {
    std::string target_address("0.0.0.0:50051");
    SendStringsClient client(
        grpc::CreateChannel(target_address,
        grpc::InsecureChannelCredentials()));

    std::string response;
    std::string a;
    std::cin >> a;

    response = client.sendRequest(a);
}

int main(int argc, char** argv) {
    RunClient();
    return 0;
}

