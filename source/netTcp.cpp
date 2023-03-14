#include "netTcp.h"
#include "Utils/Log.h"
#include "netbase.h"
#include <arpa/inet.h>
#include <cstdint>
#include <fcntl.h>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <ostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

CNetTcp::CNetTcp(const std::string host, uint16_t port) : m_host(host), m_port(port)
{
}

CNetTcp::~CNetTcp()
{
    if (m_loopThread.valid())
    {
        m_loopThread.get();
    }
}

bool CNetTcp::Start()
{
    m_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    setNonBlock(m_socketfd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int opt_val = 1;
    if (setsockopt(m_socketfd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val)))
    {
        LOGE("set reuseport error: %d\n", errno);
        return false;
    }

    int ret = ::bind(m_socketfd, (struct sockaddr *)&addr, sizeof(struct sockaddr));
    if (ret != 0)
    {
        LOGE("bind failed %d", ret);
        return false;
    }

    if (listen(m_socketfd, 20) != 0)
    {
        LOGE("listren failed\n");
        return false;
    }

    if (!updateEvents(m_socketfd, EPOLLIN | EPOLLOUT | EPOLLET, EPOLL_CTL_ADD))
    {
        return false;
    }

    m_loopThread = std::async(std::launch::async, [this]() { this->loop(); });
    return true;
}

void CNetTcp::OnConnect(const connect_cb &f_connect_cb)
{
    m_f_connect_cb = f_connect_cb;
}

void CNetTcp::OnDisConnect(const disconnect_cb &f_disconnect_cb)
{
    m_f_disconnect_cb = f_disconnect_cb;
}

void CNetTcp::OnRead(const read_cb &f_read_cb)
{
    m_f_read_cb = f_read_cb;
}

void CNetTcp::OnWrite()
{
}

void CNetTcp::handleEvent(const struct epoll_event &tEvent)
{
    int event = tEvent.events;
    int fd = tEvent.data.fd;
    if (event & (EPOLLIN | EPOLLERR))
    {
        if (fd == m_socketfd)
        {
            handleAccept();
        }
        else
        {
            /**
             * 用 curl 请求中断时，会触发读事件，然后read然后0
             * 在windows用ffplay连接中断时会触发 0001 1001, read返回-1,
             * 在linux上用ffplay连接中断时，现象与curl一致，推断是平台的差异
             */
            if (event & EPOLLERR)
            {
                handleDisconnect(fd);
            }
            else
            {
                handleRead(fd);
            }
        }
    }

    if (event & EPOLLOUT)
    { // 请注意，例子为了保持简洁性，没有很好的处理极端情况，例如EPOLLIN和EPOLLOUT同时到达的情况
        handleWrite(fd);
    }
}

void CNetTcp::handleAccept()
{
    struct sockaddr_in raddr;
    socklen_t rsz = sizeof(raddr);
    int cfd = ::accept(m_socketfd, (struct sockaddr *)&raddr, &rsz);
    if (cfd <= 0)
    {
        LOGE("accept fail, cfd:%d\n", cfd);
        return;
    }

    LOGI("accept a connection from %s, fd: %d\n", inet_ntoa(raddr.sin_addr), cfd);
    setNonBlock(cfd);
    CNetBase::updateEvents(cfd, EPOLLIN /*| EPOLLOUT*/ | EPOLLET, EPOLL_CTL_ADD);

    TConnect tConn;
    tConn.recv = std::bind(&CNetTcp::recv, this, cfd, std::placeholders::_1, std::placeholders::_2);
    tConn.send = std::bind(&CNetTcp::send, this, cfd, std::placeholders::_1, std::placeholders::_2);
    tConn.addr = inet_ntoa(raddr.sin_addr);
    tConn.fd = cfd;

    if (m_f_connect_cb)
    {
        m_f_connect_cb(tConn);
    }
}

void CNetTcp::handleRead(int fd)
{
    TConnect tConn;
    tConn.fd = fd;
    tConn.recv = std::bind(&CNetTcp::recv, this, fd, std::placeholders::_1, std::placeholders::_2);
    tConn.send = std::bind(&CNetTcp::send, this, fd, std::placeholders::_1, std::placeholders::_2);

    if (m_f_read_cb)
    {
        m_f_read_cb(tConn);
    }
}

void CNetTcp::handleWrite(int fd)
{
    LOGI("fd %d can write\n", fd);
}

void CNetTcp::handleDisconnect(int fd)
{
    LOGE("close fd: %d\n", fd);
    if (m_f_disconnect_cb)
    {
        m_f_disconnect_cb(fd);
    }
    updateEvents(fd, EPOLLIN | EPOLLOUT | EPOLLET, EPOLL_CTL_DEL);
    close(fd);
}

int CNetTcp::recv(int fd, char *buffer, int size)
{
    if (!isReadable(fd))
    {
        return 0;
    }

    int readSize = 0;
    readSize = ::read(fd, buffer, size);

    // socket disconnect
    if (readSize == 0)
    {
        handleDisconnect(fd);
    }

    if (readSize < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
    }

    return readSize;
}

int CNetTcp::send(int fd, const char *buffer, int size)
{
    int writeSize = 0;
    writeSize = ::write(fd, buffer, size);

    if (writeSize < 0)
    {
        // todo disconnect
    }

    return writeSize;
}

bool CNetTcp::isReadable(int fd)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    timeval tv;
    tv.tv_sec = static_cast<long>(1);
    tv.tv_usec = static_cast<decltype(tv.tv_usec)>(200 * 1000);

    ssize_t res = false;
    while (true)
    {
        res = select(static_cast<int>(fd + 1), &fds, nullptr, nullptr, &tv);
        if (res < 0 && errno == EINTR)
        {
            continue;
        }
        break;
    }

    return res > 0;
}