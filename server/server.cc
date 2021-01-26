#include <grpcpp/grpcpp.h>
#include <string>
#include <iostream>
#include "sendstrings.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using sendstrings::SendStrings;
using sendstrings::StringRequest;
using sendstrings::StringReply;

class SendStringService final : public SendStrings::Service {
    Status sendRequest(ServerContext* context, const StringRequest* request,
                       StringReply* reply) override {
        std::string a = request->sent();
        std::cout << a << std::endl;
        return Status::OK;
    }
};

void RunServer() {
    std::string server_adress("0.0.0.0:50051");
    SendStringService service;
    ServerBuilder builder;
    builder.AddListeningPort(server_adress, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on port: " << server_adress << std::endl;
    
    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}

