#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <deque>
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

class EOpVector{
private:
    std::deque<editor_request> executed_operations;
    size_t size;
public:
    EOpVector() : size(0) {}

    void add(editor_request op) {
        if(size != 64) {
            executed_operations.push_back(op);
            ++size;
        } else {
            executed_operations.pop_front();
            executed_operations.push_back(op);
        }
    }

    editor_request get_last_inv() {
        editor_request eop = executed_operations.back();
        switch (eop.op()) {
            case operationTransportation::INSERT:
                eop.set_op(operationTransportation::DELETE);
                break;
            case operationTransportation::DELETE:
                eop.set_op(operationTransportation::INSERT);
                break;
            default:
                break;
        }
        this->add(eop);
        return eop;
    }
};

void insert(std::vector<std::string>& content, char sym, uint64_t pos, uint64_t line) {
    content[line].insert(pos, 1, sym);
}

void del(std::vector<std::string>& content, char sym, uint64_t pos, uint64_t line) {
    content[line].erase(pos, 1);
}

void undo(EOpVector& exec_operations, std::vector<std::string>& content) {
    editor_request u = exec_operations.get_last_inv();
    char sym = u.sym();
    uint64_t pos = u.pos();
    uint64_t line = u.line();
    if(u.op() == operationTransportation::DELETE) {
        del(content, sym, pos, line);
    } else if (u.op() == operationTransportation::INSERT) {
        insert(content, sym, pos, line);
    } else {
//        TOADD(Alex) : error message
        return;
    }
}

void newLine(std::vector<std::string>& content, uint64_t line) {
    content.insert(content.begin() + line, "");
}

class ServerService final : public clientService::Service {
private:
    std::string file_name;
    std::vector<std::string> content;
    EOpVector op_vector;
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
            f << s << std::endl;
        }
    }

    Status sendFile(ServerContext *context, const empty* request, file_from_server *ret) override {
        for (std::string &s : content) {
            ret->add_file(s);
        }
        return Status::OK;
    }

    Status writeToFile(ServerContext* context, const empty* request, empty* write) override {
        std::ofstream f(file_name);
        for(std::string& s : content) {
            std::cout << s << std::endl;
            f << s << std::endl;
        }
        return Status::OK;
    }

    Status sendOP(ServerContext* context, const editor_request* OP, empty* ret) override {
        OP_type op_type = OP->op();
        uint64_t pos = OP->pos();
        uint64_t line = OP->line();
        char sym = OP->sym();
        uint32_t user_id = OP->user_id();
        switch (op_type) {
            case operationTransportation::INSERT:
                op_vector.add(*OP);
                insert(content, sym, pos, line);
                break;
            case operationTransportation::DELETE : {
                        editor_request a = *OP;
                        a.set_sym(content[line][pos]);
                        op_vector.add(a);
                        del(content, content[line][pos], pos, line);
                        break;
            }
            case operationTransportation::UNDO:
                undo(op_vector, content);
                break;
            case operationTransportation::NEWLINE:
                newLine(content, line);
                break;
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


