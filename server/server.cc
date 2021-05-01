#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <deque>
#include <utility>

// grpc libraries
#include <grpcpp/grpcpp.h>
#include "operationTransportation.grpc.pb.h"

// common grpc structures
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// structures and services
using operationTransportation::clientService;
using operationTransportation::editor_request;
using operationTransportation::file_from_server;
using operationTransportation::empty;
using operationTransportation::OP_type;
using operationTransportation::last_executed_operation;

bool file_exist(const std::string& file_path) {
    if(fopen(file_path.c_str(), "r+")) {
        return true;
    }
    return false;
}

// vector for executed operations
class EOpVector{
private:
    // deque because complexity of insert/delete from back/front is O(1)
    std::deque<editor_request> executed_operations;
    size_t size;
    size_t pos;
public:
    EOpVector() : size(0), pos(0) {}

    size_t get_size() const {
        return size;
    }
    size_t get_pos() const {
        return pos;
    }
    size_t set_size(size_t arg) {
        size = arg;
        return size;
    }
    size_t set_pos(size_t arg) {
        pos = arg;
        return pos;
    }

    void add(editor_request op) {
        if(pos != size) {
            executed_operations.erase(executed_operations.begin() + pos, executed_operations.end());
            size = pos;
        }
        if(size != 64) {
            executed_operations.push_front(op);
            ++size;
            ++pos;
        } else {
            executed_operations.push_front(op);
            executed_operations.pop_back();
        }
    }

    editor_request get_for_undo() {
        if(pos == 0) {
            editor_request a;
            a.set_op(operationTransportation::INVALID);
            return a;
        }
        auto a = *(executed_operations.begin() + pos-- - 1);
        switch (a.op()) {
            case operationTransportation::INSERT :
                a.set_op(operationTransportation::DELETE);
                break;
            case operationTransportation::DELETE :
                a.set_op(operationTransportation::INSERT);
                break;
            case operationTransportation::ADD_LINE :
                a.set_op(operationTransportation::DEL_LINE);
                break;
            case operationTransportation::DEL_LINE :
                a.set_op(operationTransportation::ADD_LINE);
                break;
            default:
                a.set_op(operationTransportation::INVALID);
                break;
        }
        return a;
    }
    editor_request get_for_redo() {
        if(pos == size) {
            editor_request a;
            a.set_op(operationTransportation::INVALID);
            return a;
        }
        auto a = *(executed_operations.begin() + pos++);
        return a;
    }

    void OPT(editor_request& request) {
        OP_type op_type = request.op();
        if(op_type == operationTransportation::UNDO || op_type == operationTransportation::REDO) {
            return;
        }
        uint64_t l = request.line();
        uint64_t p = request.pos();
        if(pos == 0) {
            return;
        }
        size_t i = pos - 1;
        editor_request tmp = executed_operations[i];
        while(request.op_id() != tmp.op_id() && i != 0) {
            switch (tmp.op()) {
                case operationTransportation::INSERT: {
                    if(tmp.line() == l && tmp.pos() < p) {
                        ++p;
                    }
                    break;
                }
                case operationTransportation::DELETE: {
                    if(tmp.line() == l && tmp.pos() < p) {
                        --p;
                    }
                    break;
                }
                case operationTransportation::ADD_LINE: {
                    if(tmp.line() < l) {
                        ++l;
                    } else if(tmp.line() == l && tmp.pos() < p) {
                        p = p - tmp.pos();
                    }
                    break;
                }
                case operationTransportation::DEL_LINE: {
                    if(tmp.line() <= l) {
                        --l;
                    }
                    break;
                }
                default:
                    break;
            }
            tmp = executed_operations[--i];
        }
        request.set_pos(p);
        request.set_line(l);
    }
};

void insert(std::vector<std::string>& content, char sym, uint64_t pos, uint64_t line) {
    content[line].insert(pos, 1, sym);
}
void del(std::vector<std::string>& content, char sym, uint64_t pos, uint64_t line) {
    content[line].erase(pos, 1);
}
void add_line(std::vector<std::string>& content, uint64_t pos, uint64_t line) {
    std::string str = content[line].substr(pos, content[line].size() - pos);
    content.insert(content.begin() + line + 1, "");
    content[line + 1] = str;
    content[line].erase(content[line].begin() + pos, content[line].end());
}
void del_line(std::vector<std::string>& content, uint64_t line) {
    content[line - 1].append(content[line]);
    content.erase(content.begin() + line);
}
void undo(EOpVector& exec_operations, std::vector<std::string>& content) {
    auto a = exec_operations.get_for_undo();
    OP_type op_type = a.op();
    uint64_t pos = a.pos();
    uint64_t line = a.line();
    char sym = a.sym();
    uint32_t user_id = a.user_id();
    uint64_t last_op = a.op_id();
    switch (op_type) {
        case operationTransportation::INSERT: {
            insert(content, sym, pos, line);
            break;
        }
        case operationTransportation::DELETE: {
            del(content, content[line][pos], pos, line);
            break;
        }
        case operationTransportation::ADD_LINE: {
            add_line(content, pos, line);
            break;
        }
        case operationTransportation::DEL_LINE: {
            del_line(content, line + 1);
            break;
        }
        default:
            return;
    }
}
void redo(EOpVector& exec_operations, std::vector<std::string>& content) {
    auto a = exec_operations.get_for_redo();
    OP_type op_type = a.op();
    uint64_t pos = a.pos();
    uint64_t line = a.line();
    char sym = a.sym();
    uint32_t user_id = a.user_id();
    uint64_t last_op = a.op_id();
    switch (op_type) {
        case operationTransportation::INSERT: {
            insert(content, sym, pos, line);
            break;
        }
        case operationTransportation::DELETE: {
            del(content, content[line][pos], pos, line);
            break;
        }
        case operationTransportation::ADD_LINE: {
            add_line(content, pos, line);
            break;
        }
        case operationTransportation::DEL_LINE: {
            del_line(content, line + 1);
            break;
        }
        default:
            return;
    }

}

class ServerService final : public clientService::Service {
private:
    std::string file_name;
    std::vector<std::string> content;
    EOpVector op_vector;
    uint64_t op_quantity;
public:
    ServerService(std::string &file) : file_name(std::move(file)) {
        std::ifstream f(file_name);
        std::string s;
        while (std::getline(f, s)) {
            content.push_back(s);
        }
        op_quantity = 0;
    }
    ~ServerService() {
        std::ofstream f(file_name);
        for(std::string& s : content) {
            f << s << std::endl;
        }
    }

    Status initialize(ServerContext *context, const empty* request, file_from_server *ret) override {
        for (std::string &s : content) {
            ret->add_file(s);
        }
        ret->set_op_id(op_quantity);
        return Status::OK;
    }

    Status writeToFile(ServerContext* context, const empty* request, last_executed_operation* write) override {
        std::ofstream f(file_name);
        for(std::string& s : content) {
            f << s << std::endl;
        }
        write->set_op_id(op_quantity);
        return Status::OK;
    }

    Status sendOP(ServerContext* context, const editor_request* OP, last_executed_operation* ret) override {
        editor_request op = *OP;
        op_vector.OPT(op);
        OP_type op_type = op.op();
        uint64_t pos = op.pos();
        uint64_t line = op.line();
        char sym = op.sym();
        uint32_t user_id = op.user_id();
        uint64_t last_user_op_id = op.op_id();
        switch (op_type) {
            case operationTransportation::INSERT: {
                op_vector.add(op);
                insert(content, sym, pos, line);
                break;
            }
            case operationTransportation::DELETE: {
                op.set_sym(content[line][pos]);
                op_vector.add(op);
                del(content, content[line][pos], pos, line);
                break;
            }
            case operationTransportation::UNDO: {
                undo(op_vector, content);
                break;
            }
            case operationTransportation::REDO: {
                redo(op_vector, content);
                break;
            }
            case operationTransportation::ADD_LINE: {
                op_vector.add(op);
                add_line(content, pos,line);
                break;
            }
            case operationTransportation::DEL_LINE: {
                op_vector.add(op);
                del_line(content,line);
                break;
            }
            default:
                return Status::CANCELLED;
        }
        ++op_quantity;
        ret->set_op_id(op_quantity);
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
    if(!file_exist(file)) {
        // TODO(Alex, 18.04.2020) : create file
    }
    RunServer(file, server_address);
    return 0;
}


