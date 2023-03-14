#include "netbase.h"
#include "Utils/Log.h"
#include <cerrno>
#include <fcntl.h>

CNetBase::CNetBase()
{
    m_epfd = epoll_create(1);
    if (m_epfd < 0)
    {
        LOGE("epoll_create fail");
        return;
    }
}

CNetBase::~CNetBase()
{
}

void CNetBase::loop()
{
    for (;;)
    {
        struct epoll_event events[100];
        int eventNum = epoll_wait(m_epfd, events, 20, 500);

        if (eventNum < 0)
        {
            LOGE("errno: %d", errno);
        }
        if (eventNum > 0)
        {
            for (int i = 0; i < eventNum; ++i)
            {
                struct epoll_event event = events[i];
                handleEvent(event);
            }
        }
    }
}

bool CNetBase::updateEvents(int fd, int events, int op)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;
    LOGI("%s fd %d events read %d write %d\n", op == EPOLL_CTL_MOD ? "mod" : "add", fd, ev.events & EPOLLIN,
         ev.events & EPOLLOUT);
    int r = epoll_ctl(m_epfd, op, fd, &ev);
    if (r != 0)
    {
        LOGE("epoll ctl fail\n");
        return false;
    }

    return true;
}

bool CNetBase::setNonBlock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        LOGE("fcntl fail");
        return false;
    }
    int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if(r < 0)
    {
        LOGE("fcntl fail");
        return false;
    }

    return true;
}