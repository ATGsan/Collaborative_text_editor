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
using operationTransportation::pos_message;
using operationTransportation::user_message;

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
            executed_operations.push_back(op);
            ++size;
            ++pos;
        } else {
            executed_operations.push_back(op);
            executed_operations.pop_front();
        }
    }

    void OPT(editor_request& request) {
        OP_type op_type = request.op();
        uint64_t l = request.line();
        uint64_t p = request.pos();
        if(pos == 0) {
            return;
        }
        size_t i = pos - 1;
        editor_request tmp = executed_operations[i];
        while(request.op_id() <= tmp.op_id() && i != 0) {
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
    if (content.empty()) {
        content.push_back("");
    }
    try {
        content[line].insert(pos, 1, sym);
        std::cout << "Inserted " << sym << " at position " << pos << " line " << line << std::endl;
    }
    catch (int e) {
        std::cout << "Exception " << e << "thrown. Operation not executed" << std::endl;
    }
}
void del(std::vector<std::string>& content, char sym, uint64_t pos, uint64_t line) {
    try {
        content[line].erase(pos - 1, 1);
        std::cout << "Erased character at position " << pos << " line " << line << std::endl;
    }
    catch (int e) {
        std::cout << "Exception " << e << "thrown. Operation not executed" << std::endl;
    }
}
void add_line(std::vector<std::string>& content, uint64_t pos, uint64_t line) {
    try {
        std::string str = content[line].substr(pos, content[line].size() - pos);
        content.insert(content.begin() + line + 1, "");
        content[line + 1] = str;
        content[line].erase(content[line].begin() + pos, content[line].end());
    }
    catch (int e) {
        std::cout << "Exception " << e << "thrown. Operation not executed" << std::endl;
    }
}
void del_line(std::vector<std::string>& content, uint64_t line) {
    try {
        content[line - 1].append(content[line]);
        content.erase(content.begin() + line);
    }
    catch (int e) {
        std::cout << "Exception " << e << "thrown. Operation not executed" << std::endl;
    }
}

struct pos_line{
    uint64_t pos;
    uint64_t line;
};

class ServerService final : public clientService::Service {
private:
    std::string file_name;
    std::vector<std::string> content;
    EOpVector op_vector;
    uint64_t op_quantity;
    std::vector<int> users;
    std::map<int, pos_line> pos_line_users;
public:
    ServerService(std::string &file) : file_name(std::move(file)) {
        std::ifstream f(file_name);
        std::string s;
        while (std::getline(f, s)) {
            content.push_back(s);
        }
        op_quantity = 0;
        if (content.empty()) content.push_back("");
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

    Status get_u_id(ServerContext* context, const empty* request, user_message* ret) override {
        ret->set_user_id(users.size());
        users.push_back(1);
        return Status::OK;
    }

    Status get_pos(ServerContext* context, const user_message* request, pos_message* ret) override {
        ret->set_pos(pos_line_users[request->user_id()].pos);
        ret->set_line(pos_line_users[request->user_id()].line);

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
//        op_vector.OPT(op);
        OP_type op_type = op.op();
        uint64_t pos = op.pos();
        uint64_t line = op.line();
        uint64_t tot_pos = op.tot_pos();
        char sym = op.sym();
        uint32_t user_id = op.user_id();
        uint64_t last_user_op_id = op.op_id();
        pos_line_users[user_id] = {pos, line};
        switch (op_type) {
            case operationTransportation::INSERT: {
                for(auto& it : pos_line_users) {
                    if(it.first == user_id) {
                        continue;
                    }
                    if(it.second.line == line) {
                        if(it.second.pos >= pos) {
                            ++it.second.pos;
                        }
                    }
                }
                op_vector.add(op);
                insert(content, sym, pos, line);
                break;
            }
            case operationTransportation::DELETE: {
                for(auto& it : pos_line_users) {
                    if(it.first == user_id) {
                        continue;
                    }
                    if(it.second.line == 0 && it.second.pos == 0) {
                        continue;
                    }
                    if(it.second.line == line) {
                        if(it.second.pos >= pos) {
                            --it.second.pos;
                        }
                    }
                }
                op.set_sym(content[line][pos - 1]);
                op.set_pos(op.pos() - 1);
                std::cout << op.sym() << std::endl;
                op_vector.add(op);
                del(content, content[line][pos], pos, line);
                break;
            }
            case operationTransportation::ADD_LINE: {
                op_vector.add(op);
                add_line(content, pos,line);
                break;
            }
            case operationTransportation::DEL_LINE: {
                for(auto& it : pos_line_users) {
                    if(it.first == user_id) {
                        continue;
                    }
                    if(it.second.line == 0) {
                        continue;
                    }
                    if(it.second.line >= line) {
                        --pos;
                    }
                }
                op_vector.add(op);
                del_line(content,line);
                break;
            }
            case operationTransportation::MOVE: {
                return Status::OK;
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
    RunServer(file, server_address);
    return 0;
}
