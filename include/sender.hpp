#ifndef _SENDER_H_
#define _SENDER_H_

#include "define.h"
// #include "recver.hpp"
#include <cstdint>
#include <type_traits>
#include <functional>

enum E_SENDER_TYPE
{
    E_SENDER_RTSP = 1,
    E_SENDER_FLV = 2,
};

class CRecver;
class CSender
{
  public:
    CSender():m_pRecver(nullptr)
    {
    };
    virtual ~CSender(){};

    virtual void Send(const std::shared_ptr<Frame> &tFrame) = 0;
    virtual void Send(const uint8_t *buffer, size_t size, bool isVideo) = 0;
    virtual void SetRecver(CRecver *pRecver)
    {
        m_pRecver = pRecver;
    }
    virtual void SetRecverDisconnectCB(std::function<void()>){};
    virtual void OnRecverDisconnectCB(){};

    // todo
    virtual void UpdateRemotePort(uint16_t port){};
    // todo
    virtual E_STREAM_TYPE StreamType(){return E_STREAM_ES;}

  protected:
    CRecver *m_pRecver;
};

#endif