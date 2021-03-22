#include <grpcpp/grpcpp.h>
#include <string>
#include "dataTransportation.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using dataTransportation::caller;
using dataTransportation::operation;
using dataTransportation::operation_status;
using dataTransportation::file_from_server;
using dataTransportation::empty;

enum operations {
    INSERT = 0,
    DELETE = 1,
    UNDO = 2,
    REDO = 3
};

class SendStringsClient {
public:
    SendStringsClient(std::shared_ptr<Channel> channel) :
                         stub_(caller::NewStub(channel)) {}

    std::string sendRequest() {
        empty e;
        file_from_server reply;
        ClientContext context;
        Status status = stub_->sendFile(&context, e, &reply);
        if(status.ok()) {
            return "OK";
        } else {
            std::cout << status.error_code() << ": " << status.error_message();
            std::cout << std::endl;
            return "Rpc Failed";
        }
    }
private:
    std::unique_ptr<caller::Stub> stub_;
};

void RunClient() {
    std::string target_address("0.0.0.0:50051");
    SendStringsClient client(
        grpc::CreateChannel(target_address,
        grpc::InsecureChannelCredentials()));

    std::string a = client.sendRequest();
    std::cout << a << std::endl;
}

int main(int argc, char** argv) {
    RunClient();
    return 0;
}

