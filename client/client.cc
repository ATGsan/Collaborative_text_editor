#include <fstream>
#include <string>
#include <vector>

// grpc libraries
#include <grpcpp/grpcpp.h>
#include "operationTransportation.grpc.pb.h"

// common grpc structures
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

// structures and service from protobuf
using operationTransportation::clientService;
using operationTransportation::editor_request;
using operationTransportation::empty;
using operationTransportation::file_from_server;
using operationTransportation::OP_type;
using operationTransportation::last_executed_operation;

// main class for client
class ClientService {
private:
    std::unique_ptr<clientService::Stub> stub_;
    std::string file_name;
    uint64_t last_op;
public:
    // default constructor with initialize of channel between server and client
    ClientService(std::shared_ptr<Channel> channel, std::string& file_path) :
            stub_(clientService::NewStub(channel)), file_name(file_path) {}

    // command to initialize file in client side
    void initialize() {
        empty e;
        file_from_server reply;
        ClientContext context;
        // call rpc in server side
        Status status = stub_->initialize(&context, e, &reply);
        std::fstream file(file_name);
        for(int i = 0; i < reply.file_size(); i++) {
            file << reply.file(i) << std::endl;
        }
        last_op = reply.op_id();
    }

    // main method to send operation to server
    void OPs(OP_type operation, uint64_t pos, uint64_t line, char sym, uint32_t user_id) {
        last_executed_operation op;
        editor_request request;
        request.set_op(operation);
        request.set_pos(pos);
        request.set_line(line);
        request.set_sym(sym);
        request.set_user_id(user_id);
        request.set_op_id(last_op);
        ClientContext context;
        Status status = stub_->sendOP(&context, request, &op);
        last_op = op.op_id();
    }

    // call to write file from vector of strings to file in server file
    void writeToFile() {
        empty e;
        last_executed_operation op;
        ClientContext context;
        Status status = stub_->writeToFile(&context, e, &op);
        last_op = op.op_id();
    }
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
