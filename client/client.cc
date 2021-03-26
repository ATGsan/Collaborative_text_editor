#include <grpcpp/grpcpp.h>
#include <string>
#include <fstream>
#include <vector>
#include "dataTransportation.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using dataTransportation::caller;
using dataTransportation::operation;
using dataTransportation::operation_status;
using dataTransportation::file_from_server;
using dataTransportation::empty;

enum {
    INSERT = 0,
    DELETE = 1,
    UNDO = 2,
    REDO = 3
};

class ClientService {
public:
    ClientService(std::shared_ptr<Channel> channel, std::string file_path) :
                         stub_(caller::NewStub(channel)), file_name(file_path) {}

    std::string sendRequest() {
        empty e;
        file_from_server reply;
        ClientContext context;
        Status status = stub_->sendFile(&context, e, &reply);
        std::fstream file(file_name);
        for(int i = 0; i < reply.file_size(); i++) {
            file << reply.file(i);
            file << '\n';
        }
        if(status.ok()) {
            return "OK";
        } else {
            std::cout << status.error_code() << ": " << status.error_message();
            return "Rpc Failed";
        }
    }
    std::string OPs(uint32_t op_type, uint32_t pos, uint32_t line, const std::string& sym, uint32_t user_id) {
        empty e;
        operation OP;
        OP.set_op(op_type);
        OP.set_pos(pos);
        OP.set_line(line);
        OP.set_sym(sym);
        OP.set_user_id(user_id);
        ClientContext context;
        Status status = stub_->sendOP(&context, OP, &e);
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
    std::string file_name;
};

void RunClient() {
    std::string file = "output.txt";
    std::string target_address("0.0.0.0:50052");
    ClientService client(
        grpc::CreateChannel(target_address,
        grpc::InsecureChannelCredentials()), file);

    std::string a = client.sendRequest();
    a = client.OPs(DELETE, 2, 1, "$", 100);
    std::cout << a << std::endl;
}

int main(int argc, char** argv) {
    RunClient();
    return 0;
}

