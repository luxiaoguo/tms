#include "netUdp.h"
#include "Utils/Log.h"
#include "httplib.hpp"
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

CNetUdp::CNetUdp():m_taskPool(new httplib::ThreadPool(8))
{
}

CNetUdp::~CNetUdp()
{
}

bool CNetUdp::Start()
{
    m_loopThread = std::async(std::launch::async, &CNetUdp::loop, this);
    return true;
}

bool CNetUdp::AddEvent(uint16_t port, const read_cb &f_read_cb)
{
    int fd = createSocket(port);
    if (fd < 0)
    {
        LOGE("create socket fail, %d\n", fd);
        return false;
    }

    if (!updateEvents(fd, EPOLLIN | EPOLLOUT | EPOLLET, EPOLL_CTL_ADD))
    {
        LOGE("updateEvents fail, %d\n", fd);
        close(fd);
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_tEventMap.push_back({fd, port, f_read_cb});
    LOGI("updateEvents success, %d\n", fd);
    return true;
}

bool CNetUdp::DelEvent(uint16_t port)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    TEvent event;

    auto it = m_tEventMap.begin();
    for (;it < m_tEventMap.end(); ++it)
    {
        if (it->port == port)
        {
            event = *it;
            m_tEventMap.erase(it);
            lock.unlock();
            break;
        }
    }

    updateEvents(event.fd, EPOLLIN | EPOLLOUT | EPOLLET, EPOLL_CTL_DEL);
    return true;
}

void CNetUdp::handleEvent(const struct epoll_event& tEvent)
{
    int event = tEvent.events;
    int fd = tEvent.data.fd;

    if (event & (EPOLLIN | EPOLLERR))
    {
        read_cb f;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto &it : m_tEventMap)
            {
                if (it.fd == fd)
                {
                    f = it.cb;
                    break;
                }
            }
        }

        if (f)
        {
            TConnect tConn;
            tConn.fd = fd;
            tConn.recvfrom = std::bind(&CNetUdp::recv, this, fd, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            tConn.sendto = std::bind(&CNetUdp::recv, this, fd, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

            m_taskPool->enqueue([f, tConn]() { f(tConn); });
        }
        else
        {
            // 连接不在监听列表里，移除事件
            updateEvents(fd, EPOLLIN | EPOLLOUT | EPOLLET, EPOLL_CTL_DEL);
        }
    }

    if (event & EPOLLOUT)
    { // 请注意，例子为了保持简洁性，没有很好的处理极端情况，例如EPOLLIN和EPOLLOUT同时到达的情况
    }
}

int CNetUdp::createSocket(uint16_t port)
{
    int fd = -1;
    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    setNonBlock(fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int opt_val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val)))
    {
        LOGE("set reuseport error: %d\n", errno);
    }

    // uint32_t size = 1024 * 1024 * 1024;
    // if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(size)))
    // {
    //     LOGE("set recvbuf error: %d\n", errno);
    // }

    int ret = ::bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr));
    if (ret != 0)
    {
        LOGE("bind failed %d", ret);
        return false;
    }
    return fd;
}

int CNetUdp::recv(int fd, struct sockaddr *addr, char *buffer, int size)
{
    int readSize = 0;
    // readSize = ::read(fd, buffer, size);
    size_t addrSize = sizeof(sockaddr);
    readSize = recvfrom(fd, buffer, size, 0, addr, (socklen_t *)&addrSize);
    
    if (readSize < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
    }
    // LOGE("read %d\n", readSize);
    return readSize;
}

int CNetUdp::send(int fd, struct sockaddr *addr, const char *buffer, int size)
{
    int writeSize = 0;
    // writeSize = ::write(fd, buffer, size);
    sendto(fd, buffer, size, MSG_NOSIGNAL, addr, (socklen_t)sizeof(sockaddr));

    if (writeSize < 0)
    {
        // todo disconnect
    }

    return writeSize;
}