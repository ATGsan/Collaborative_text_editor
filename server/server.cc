#include <grpcpp/grpcpp.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <fstream>
#include <utility>
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

enum {
    INSERT = 0,
    DELETE = 1,
    UNDO = 2,
    REDO = 3
};

void insert(std::string file_path, char sym, uint32_t pos, uint32_t line) {
    FILE* f = fopen(&file_path[0], "r+");
    uint32_t line_counter = 0;
    std::vector<std::string> text;
    char c;
    std::string str;
    while(fscanf(f, "%c", &c) != EOF) {
        if(c == '\n') {
            ++line_counter;
            text.push_back(str);
            str = "";
            if(line_counter == line) {
                uint32_t cur_pos = 0;
                while(fscanf(f, "%c", &c) != EOF) {
                    if(c == '\n') {
                        text.push_back(str);
                        str = "";
                        break;
                    }
                    if(cur_pos == pos) {
                        str += sym;
                    }
                    str += c;
                    ++cur_pos;
                }
            }
        } else {
            str += c;
        }
    }
    // to refactor
    fclose(f);
    std::fstream file;
    file.open(file_path, std::ofstream::trunc | std::ofstream::out);
    file.close();
    file.open(file_path, std::ofstream::out);
    for(std::string& s : text) {
        file << s << '\n';
    }
    file.close();
}

void del(std::string file_path, uint32_t pos, uint32_t line) {
    FILE* f = fopen(&file_path[0], "r+");
    uint32_t line_counter = 0;
    std::vector<std::string> text;
    char c;
    std::string str;
    while(fscanf(f, "%c", &c) != EOF) {
        if(c == '\n') {
            ++line_counter;
            text.push_back(str);
            str = "";
            if(line_counter == line) {
                uint32_t cur_pos = 0;
                while(fscanf(f, "%c", &c) != EOF) {
                    if(c == '\n') {
                        text.push_back(str);
                        str = "";
                        break;
                    }
                    if(cur_pos != pos) {
                        str += c;
                    }
                    ++cur_pos;
                }
            }
        } else {
            str += c;
        }
    }
    text.push_back(str);
    // to refactor
    fclose(f);
    std::fstream file;
    file.open(file_path, std::ofstream::trunc | std::ofstream::out);
    file.close();
    file.open(file_path, std::ofstream::out);
    for(std::string& s : text) {
        file << s << '\n';
    }
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
    ServerService(std::string file) : file_name(std::move(file)) {}
    Status sendFile(ServerContext* context, const empty* request, file_from_server* ret) {
        FILE* f = fopen(&file_name[0], "r+");
        std::vector<std::string> file_lines;
        char c;
        std::string str;
        while (fscanf(f, "%c", &c) != EOF) {
            if(c == '\n') {
                file_lines.push_back(str);
                str = "";
            } else {
                str += c;
            }
        }
        for(std::string& s : file_lines) {
            ret->add_file(s);
        }
        fclose(f);
        return Status::OK;
    }
    Status sendOP(ServerContext* context, const operation* OP, empty* ret) {
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
            default:
                break;
        }
        return Status::OK;
    }
};

void RunServer() {
    std::string server_adress("0.0.0.0:50052");
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

