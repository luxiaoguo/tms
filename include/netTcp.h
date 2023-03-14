#ifndef _NET_TCP_H_
#define _NET_TCP_H_

#include "netbase.h"
#include <cstdint>
#include <future>
#include <string>

class CNetTcp : public CNetBase
{
  public:
    CNetTcp(const std::string host, uint16_t port);
    ~CNetTcp();

   bool Start() override;
   void OnConnect(const connect_cb &f_connect_cb) override;
   void OnDisConnect(const disconnect_cb &f_disconnect_cb) override;
   void OnRead(const read_cb &f_read_cb) override;
   void OnWrite() override;

 private:
   void handleEvent(const struct epoll_event &tEvent) override;

   void handleAccept();
   void handleRead(int fd);
   void handleWrite(int fd);
   void handleDisconnect(int fd);

   int recv(int fd, char *buffer, int size);
   int send(int fd, const char *buffer, int size);
   bool isReadable(int fd);

 private:
   std::string m_host;
   uint16_t m_port;
  //  int m_epfd = -1;
   int m_socketfd = -1;
   connect_cb m_f_connect_cb;
   disconnect_cb m_f_disconnect_cb;
   read_cb m_f_read_cb;

   std::future<void> m_loopThread;
};

#endif