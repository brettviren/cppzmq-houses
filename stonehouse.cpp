#include "zmq.hpp"
#include "zmq_addon.hpp"
#include "zmq_zactor.hpp"

#include <iostream>
#include <vector>

// This is a "hard-wired" value shared with libzma internals
#define ZAP_ENDPOINT  "inproc://zeromq.zap.01"

void auth(zmq::context_t &ctx, zmq::socket_t& link, char* pub, char* sec)
{
  const std::string username = "user";
  const std::string password = "secret";

  std::cerr << "agent: starting zap on " << ZAP_ENDPOINT << std::endl;
  zmq::socket_t zap(ctx, zmq::socket_type::rep);
  zap.bind(ZAP_ENDPOINT);
  
  zmq::poller_t<> poll;
  poll.add(link, zmq::event_flags::pollin);
  poll.add(zap, zmq::event_flags::pollin);
  std::vector<zmq::poller_event<>> events(2);
  std::chrono::milliseconds timeout{-1};

  // signal ready
  link.send(zmq::message_t{}, zmq::send_flags::none);

  while (true) {
  std::cerr << "agent: waiting" << std::endl;    
    const auto nin = poll.wait_all(events, timeout);
    if (!nin) {
      break;
    }
    for (size_t ind=0; ind<nin; ++ind) {
      if (events[ind].socket == link) {
        std::cerr << "agent: link hit, exiting" << std::endl;
        break;
      }

      zmq::multipart_t mp(zap);
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
      if (ip == "127.0.0.1") {
        if (mech == "PLAIN") {
          std::string u = mp.popstr();
          std::string p = mp.popstr();
          if (u == username and p == password) {
            ok = true;
          }
        }
        else if (mech == "CURVE") {
          auto msg = mp.pop();
          assert(32 == msg.size());
          char encoded[4];
          zmq_z85_encode(encoded, msg.data<uint8_t>(), msg.size());
          // ... fixme: actually check
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
}

int main()
{
  zmq::context_t ctx;

  char server_pub [41];
  char server_sec [41];
  {
    int rc = zmq_curve_keypair (server_pub, server_sec);
    assert (rc==0);
  }

  zmq::zactor_t zap(ctx, auth, server_pub, server_sec);

  std::cerr << "starting server" << std::endl;
  zmq::socket_t server(ctx, zmq::socket_type::push);
  server.set(zmq::sockopt::zap_domain, "global");
  server.set(zmq::sockopt::curve_server, 1);
  server.set(zmq::sockopt::curve_publickey, server_pub);
  server.set(zmq::sockopt::curve_secretkey, server_sec);  

  std::string addr = "tcp://127.0.0.1:5678";
  server.bind(addr);

  char client_pub [41];
  char client_sec [41];
  {
    int rc = zmq_curve_keypair (client_pub, client_sec);
    assert (rc==0);
  }

  std::cerr << "starting client\n";
  zmq::socket_t client(ctx, zmq::socket_type::pull);
  client.set(zmq::sockopt::zap_domain, "global");
  client.set(zmq::sockopt::curve_serverkey, server_pub);

  client.set(zmq::sockopt::curve_publickey, client_pub);
  client.set(zmq::sockopt::curve_secretkey, client_sec);  

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

  return 0;
}
