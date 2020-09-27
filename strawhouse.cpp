#include "zmq.hpp"
#include "zmq_addon.hpp"
#include <string>
#include <thread>
#include <iostream>

// This is a "hard-wired" value shared with libzma internals
#define ZAP_ENDPOINT  "inproc://zeromq.zap.01"

int main(void)
{
    zmq::context_t ctx;

    // We add strawhouse in a very dumbly designed manner
    std::string good = "127.0.0.1";
    std::string bad = "157.240.0.35";

    std::thread t([&]{
        std::cerr << "starting zap on " << ZAP_ENDPOINT << std::endl;
        zmq::socket_t zap(ctx, zmq::socket_type::rep);
        zap.bind(ZAP_ENDPOINT);
        while (true) {
            zmq::multipart_t mp(zap);
            if (mp.size() != 6) {
                zap.send(zmq::message_t{}, zmq::send_flags::none);
                break;
            }
            std::cerr << "recv:" << std::endl;
            for (const auto& m : mp) {
                std::cerr << m.str() << std::endl;
            }
            zmq::multipart_t rep;
            rep.add(mp.pop());
            rep.add(mp.pop());
            std::string dom = mp.popstr();
            std::string ip = mp.popstr();
            std::string id = mp.popstr();
            std::string mech = mp.popstr();
            std::cerr << "dom:\"" << dom << "\" ip:\"" << ip << "\"" << std::endl;
            // Dumbest ACL ever:
            if (ip == good) {
                rep.addstr("200");
                rep.addstr("OK");
            }
            else if (ip == bad) {
                rep.addstr("400");
                rep.addstr("NOT OK");
            }
            else {              // the Internet has only two hosts
                rep.addstr("500");
                rep.addstr("WHO U?");
            }
            rep.addstr("user"); // user id
            rep.addstr("");     // metadata
            std::cerr << "send:" << std::endl;
            for (const auto& m : rep) {
                std::cerr << m.str() << std::endl;
            }
            rep.send(zap);
        }
    });


    std::cerr << "starting server\n";
    zmq::socket_t server(ctx, zmq::socket_type::push);
    server.set(zmq::sockopt::zap_domain, "global");

    std::string addr = "tcp://127.0.0.1:5678";
    server.bind(addr);

    std::cerr << "starting client\n";
    zmq::socket_t client(ctx, zmq::socket_type::pull);
    client.set(zmq::sockopt::zap_domain, "global");
    client.connect(addr);
    
    std::cerr << "server send:\n";
    auto res = server.send(zmq::str_buffer("hello"), zmq::send_flags::none);
    assert(res);

    std::cerr << "client recv:\n";
    zmq::message_t msg;
    res = client.recv(msg, zmq::recv_flags::none);
    assert(res);

    assert(msg.to_string() == "hello");

    std::cerr << "ending\n";
    zmq::socket_t zapkiller(ctx, zmq::socket_type::req);
    zapkiller.connect(ZAP_ENDPOINT);
    zmq::message_t empty;
    zapkiller.send(empty, zmq::send_flags::none);
    res = zapkiller.recv(empty, zmq::recv_flags::none);
    assert(res);

    t.join();

    return 0;
}
