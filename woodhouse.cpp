#include "zmq.hpp"
#include "zmq_addon.hpp"

#include <string>
#include <thread>
#include <iostream>

// This is a "hard-wired" value shared with libzma internals
#define ZAP_ENDPOINT  "inproc://zeromq.zap.01"

void auth(zmq::context_t& ctx)
{

  const std::string username = "user";
  const std::string password = "secret";

  std::cerr << "agent: starting zap on " << ZAP_ENDPOINT << std::endl;
  zmq::socket_t zap(ctx, zmq::socket_type::rep);
  zap.bind(ZAP_ENDPOINT);
  while (true) {
    zmq::multipart_t mp(zap);
    if (mp.size() < 6) {
      std::cerr << "agent: non ZAP message, exiting" << std::endl;
      zap.send(zmq::message_t{}, zmq::send_flags::none);
      break;
    }
    std::cerr << "agent: recv:" << std::endl;
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
    std::cerr << "agent: dom:\"" << dom << "\" ip:\""
              << ip << "\"" << std::endl;
    bool ok = false;
    if (mech == "PLAIN" and ip == "127.0.0.1" ) {
      std::string u = mp.popstr();
      std::string p = mp.popstr();
      if (u == username and p == password) {
        ok = true;
      }
    }
    if (ok) {
      rep.addstr("200");
      rep.addstr("OK");
    }
    else {
      rep.addstr("400");
      rep.addstr("NOT OK");
    }
    rep.addstr(username);
    rep.addstr("");     // metadata
    std::cerr << "agent: send:" << std::endl;
    for (const auto& m : rep) {
      std::cerr << m.str() << std::endl;
    }
    rep.send(zap);
  }

}
int main(void)
{
    zmq::context_t ctx;

    std::thread t([&] { auth(ctx); });

    std::cerr << "starting server\n";
    zmq::socket_t server(ctx, zmq::socket_type::push);
    server.set(zmq::sockopt::zap_domain, "global");

    std::string addr = "tcp://127.0.0.1:5678";
    server.set(zmq::sockopt::plain_server, 1);
    server.bind(addr);

    std::cerr << "starting client\n";
    zmq::socket_t client(ctx, zmq::socket_type::pull);
    client.set(zmq::sockopt::zap_domain, "global");
    client.set(zmq::sockopt::plain_username, "user");
    client.set(zmq::sockopt::plain_password, "secret");
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
