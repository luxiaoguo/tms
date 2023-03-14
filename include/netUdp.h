#ifndef _NET_UDP_H_
#define _NET_UDP_H_

#include "netbase.h"
#include "httplib.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <sys/epoll.h>
#include <vector>
#include <future>

struct TEvent
{
    int fd;
    int port;
    read_cb cb;
};

class CNetUdp : public CNetBase
{
  public:
    CNetUdp();
    ~CNetUdp();

  public:
    virtual bool Start() override;
    virtual void OnRead(const read_cb &f_read_cb) override{};
    virtual void OnWrite() override{};
    bool AddEvent(uint16_t port, const read_cb &f_read_cb) override;
    bool DelEvent(uint16_t port) override;

  private:
    void handleEvent(const struct epoll_event &event) override;
    int createSocket(uint16_t port);
    int recv(int fd, struct sockaddr *addr, char *buffer, int size);
    int send(int fd, struct sockaddr *addr, const char *buffer, int size);

  private:
    std::vector<TEvent> m_tEventMap;
    std::mutex m_mutex;
    std::unique_ptr<httplib::ThreadPool> m_taskPool;

   std::future<void> m_loopThread;
};

#endif
