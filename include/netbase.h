#ifndef _NET_BASE_H_
#define _NET_BASE_H_

#include <functional>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "Utils/Log.h"

struct TConnect
{
    int fd;
    int port;
    std::string addr;
    std::function<int(char *buffer, int size)> recv;
    std::function<int(const char *buffer, int size)> send;
    std::function<int(struct sockaddr*, char *buffer, int size)> recvfrom;
    std::function<int(struct sockaddr*, char *buffer, int size)> sendto;
};

using connect_cb = std::function<void(const TConnect &)>;
using read_cb = std::function<void(const TConnect &)>;
using disconnect_cb = std::function<void(int)>;

class CNetBase
{
  public:
    CNetBase();
    virtual ~CNetBase();

    virtual bool Start() = 0;
    virtual void OnConnect(const connect_cb &f_connect_cb){};
    virtual void OnDisConnect(const disconnect_cb &f_disconnect_cb){};
    virtual void OnRead(const read_cb &f_read_cb){};
    virtual void OnWrite(){};
    virtual bool AddEvent(uint16_t port, const read_cb &f_read_cb){return true;};
    virtual bool DelEvent(uint16_t port){return true;};

  protected:
    virtual void handleEvent(const struct epoll_event &event) = 0;

    void loop();
    bool updateEvents(int fd, int events, int op);
    bool setNonBlock(int fd);

  protected:
    int m_epfd = -1;
};

#endif