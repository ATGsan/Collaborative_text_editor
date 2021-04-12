#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include <grpcpp/grpcpp.h>
#include "operationTransportation.grpc.pb.h"

using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using operationTransportation::clientService;
using operationTransportation::editor_request;
using operationTransportation::file_from_server;
using operationTransportation::empty;
using operationTransportation::OP_type;

void insert(std::vector<std::string> &content, char sym, uint64_t pos, uint64_t line) {
    content[line].insert(pos, 1, sym);
}

void del(std::vector<std::string> &content, uint64_t pos, uint64_t line) {
    content[line].erase(pos);
}

void undo() {
    
}

void redo() {

}

class ServerService final : public clientService::Service {
private:
    std::string file_name;
    std::vector<std::string> content;
public:
    ServerService(std::string &file) : file_name(std::move(file)) {
        std::ifstream f(file_name);
        std::string s;
        while (std::getline(f, s)) {
            content.push_back(s);
        }
    }

    ~ServerService() {
        std::ofstream f(file_name);
        for(std::string& s : content) {
            f << s;
        }
        std::cout << "qq\n";
    }

    Status sendFile(ServerContext *context, const empty* request, file_from_server *ret) override {
        for (std::string &s : content) {
            ret->add_file(s);
        }
        return Status::OK;
    }

    Status sendOP(ServerContext* context, const editor_request* OP, empty* ret) override {
        OP_type op_type = OP->op();
        std::cout << op_type << std::endl;
        uint64_t pos = OP->pos();
        uint64_t line = OP->line();
        char sym = OP->sym();
        uint32_t user_id = OP->user_id();
        switch (op_type) {
            case operationTransportation::INSERT:
                insert(content, sym, pos, line);
                break;
            case operationTransportation::DELETE:
                del(content, pos, line);
                break;
            case operationTransportation::UNDO:
                return Status(grpc::UNIMPLEMENTED, "UNIMPLEMENTED\n");
            case operationTransportation::REDO:
                return Status(grpc::UNIMPLEMENTED, "UNIMPLEMENTED\n");
            default:
                return Status::CANCELLED;
        }
        return Status::OK;
    }
};

void RunServer(std::string &file, std::string &port) {
    ServerService service(file);
    ServerBuilder builder;
    builder.AddListeningPort(port, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on port: " << port << std::endl;

    server->Wait();
}

int main(int argc, char *argv[]) {
    std::string file = "input.txt", server_address = "0.0.0.0:50052";
    if (argc >= 3) {
        file = argv[1];
        server_address = argv[2];
    }
    RunServer(file, server_address);
    return 0;
}


