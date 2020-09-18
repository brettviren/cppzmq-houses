#include "zmq.hpp"
#include <string>

int main(void)
{
    zmq::context_t ctx;
    zmq::socket_t server(ctx, zmq::socket_type::push);

    std::string addr = "tcp://127.0.0.1:5678";
    server.bind(addr);

    zmq::socket_t client(ctx, zmq::socket_type::pull);
    client.connect(addr);
    
    auto res = server.send(zmq::str_buffer("hello"), zmq::send_flags::none);
    assert(res);

    zmq::message_t msg;
    res = client.recv(msg, zmq::recv_flags::none);
    assert(res);

    assert(msg.to_string() == "hello");
    return 0;
}
