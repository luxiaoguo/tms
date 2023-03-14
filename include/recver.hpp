#ifndef _RECVER_H_
#define _RECVER_H_

#include "define.h"
#include <mutex>
#include <string>
#include "netbase.h"
#include "sender.hpp"

class CMediaServer;

class CRecver
{
  friend CMediaServer; 

  public:
    CRecver();
    virtual ~CRecver();

    virtual std::string Urn() = 0;
    virtual std::vector<std::shared_ptr<CSender>> GetSenders() = 0;
    virtual const TAudioInfo& AudioInfo();
    virtual const TVideoInfo& VideoInfo();

  protected:
    virtual void append(const char *buff, std::size_t size, bool isVideo) = 0;
    virtual void bindSender(std::shared_ptr<CSender> &cSender);
    virtual void unBindSender(std::shared_ptr<CSender> &cSender);
    virtual void unBindSender(CSender *pcSender);

  protected:
    std::vector<std::shared_ptr<CSender>> m_vSenders;
    std::mutex m_mutex;
    TAudioInfo m_tAudioInfo; 
    TVideoInfo m_tVideoInfo;
};

#endif