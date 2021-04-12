#include <fstream>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>
#include "operationTransportation.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using operationTransportation::clientService;
using operationTransportation::editor_request;
using operationTransportation::empty;
using operationTransportation::file_from_server;
using operationTransportation::OP_type;

class ClientService {
public:
    ClientService(std::shared_ptr<Channel> channel, std::string& file_path) :
                         stub_(clientService::NewStub(channel)), file_name(file_path) {}

    void sendRequest() {
        empty e;
        file_from_server reply;
        ClientContext context;
        Status status = stub_->sendFile(&context, e, &reply);
        std::fstream file(file_name);
        for(int i = 0; i < reply.file_size(); i++) {
            file << reply.file(i);
            file << '\n';
        }
    }
    void OPs(OP_type operation, uint64_t pos, uint64_t line, char sym, uint32_t user_id) {
        empty e;
        editor_request request;
        request.set_op(operation);
        request.set_pos(pos);
        request.set_line(line);
        request.set_sym(sym);
        request.set_user_id(user_id);
        ClientContext context;
        Status status = stub_->sendOP(&context, request, &e);
    }
private:
    std::unique_ptr<clientService::Stub> stub_;
    std::string file_name;
};

void RunClient(std::string& file, std::string& server_address) {
    ClientService client(
        grpc::CreateChannel(server_address,
        grpc::InsecureChannelCredentials()), file);
}

int main(int argc, char* argv[]) {
    std::string file = "output.txt", server_address = "0.0.0.0:50052";
    if (argc >= 3) {
        file = argv[1];
        server_address = argv[2];
    }
    RunClient(file, server_address);
    return 0;
}
