#include <grpcpp/grpcpp.h>
#include <string>
#include <iostream>
#include <fstream>
#include <inttypes.h>
#include "dataTransportation.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using dataTransportation::caller;
using dataTransportation::operation;
using dataTransportation::operation_status;
using dataTransportation::file_from_server;
using dataTransportation::empty;

enum operation_types {
    INSERT = 0,
    DELETE = 1,
    UNDO = 2,
    REDO = 3
};

void insert(std::string file_path, char sym, uint32_t pos, uint32_t line) {
    std::fstream file(file_path);
    uint32_t line_counter = 0;
    std::string buf;
    while(line_counter != line && !file.eof()) {
        char s;
        file >> s;
        buf += s;
        line_counter += (s == '\n');
    }
    if(file.eof()) {
        fprintf(stderr, "wrong line\n");
        return;
    }
    uint32_t cur_pos = 0;
    while(cur_pos != pos && !file.eof()) {
        char s;
        file >> s;
        if(cur_pos == pos) {
            buf += sym;
        }
        ++cur_pos;
        buf += s;
    }
    if(file.eof()) {
        fprintf(stderr, "wrong position\n");
        return;
    }
    // to refactor
    file.close();
    file.open(file_path, std::ofstream::trunc | std::ofstream::out);
    file.close();
    file.open(file_path, std::ofstream::out);
    file << buf;
    file.close();
}

void del(std::string file_path, uint32_t pos, uint32_t line) {
    std::fstream file(file_path);
    uint32_t line_counter = 0;
    std::string buf;
    while(line_counter != line && !file.eof()) {
        char s;
        file >> s;
        buf += s;
        line_counter += (s == '\n');
    }
    if(file.eof()) {
        fprintf(stderr, "wrong line\n");
        return;
    }
    uint32_t cur_pos = 0;
    while(cur_pos != pos && !file.eof()) {
        char s;
        file >> s;
        if(cur_pos == pos) {
            break;
        }
        ++cur_pos;
        buf += s;
    }
    if(file.eof()) {
        fprintf(stderr, "wrong position\n");
        return;
    }
    // to refactor
    file.close();
    file.open(file_path, std::ofstream::trunc | std::ofstream::out);
    file.close();
    file.open(file_path, std::ofstream::out);
    file << buf;
    file.close();
}

void undo() {

}

void redo() {

}

class ServerService final : public caller::Service {
private:
    std::string file_name;
public:
    ServerService(std::string file) : file_name(file) {}
    Status sendFile(ServerContext* context, const empty* request, file_from_server* ret) {
        std::string a;
        std::ifstream file_stream(file_name);
        char c;
        while (!file_stream.eof()) {
            file_stream >> c;
            a += c;
        }
        ret->set_file(a);
        return Status::OK;
    }
    Status sendOP(ServerContext* context, const operation* OP, file_from_server* ret) {
        uint32_t op_type = OP->op();
        uint32_t pos = OP->pos();
        uint32_t line = OP->line();
        std::string s = (OP->sym());
        char sym;
        if(s.size() >= 1) {
            sym = s[0];
        }
        uint32_t user_id = OP->user_id();
        switch (op_type) {
            case INSERT:
                insert(file_name, sym, pos, line);
                break;
            case DELETE:
                del(file_name, pos, line);
                break;
            case UNDO:
                undo();
                break;
            case REDO:
                redo();
                break;
        }
        return Status::OK;
    }
};

void RunServer() {
    std::string server_adress("0.0.0.0:50051");
    std::string file_name = "input.txt";
    ServerService service(file_name);
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

