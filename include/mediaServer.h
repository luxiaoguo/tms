#ifndef _MEDIA_SERVER_H_
#define _MEDIA_SERVER_H_

#include "define.h"
#include "netTcp.h"
#include "netbase.h"
#include "recver.hpp"
#include "sender.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

enum E_RECVER_TYPE
{
    E_RECVER_RTSP = 1
};



class CRtpRecver;

class CMediaServer
{
    friend CRtpRecver;
  public:
    static CMediaServer& Inst();

  public:
    CMediaServer();
    ~CMediaServer();

  public:
    void Start();

    std::shared_ptr<CRecver> CreateRtspRecver(const std::string &urn, uint16_t serverPort, uint16_t remotePort,
                                              const TVideoInfo &, const TAudioInfo &);
    std::shared_ptr<CSender> CreateRtspSender(const std::string &urn, const std::string &destHost, uint16_t serverPort,
                                              uint16_t remotePort);

    std::shared_ptr<CSender> CreateFlvSender(const std::string &urn, const std::shared_ptr<CWriter> &writer);

    std::shared_ptr<CRecver> BindSender2Recver(const std::string &urn, std::shared_ptr<CSender> &sender);
    bool UnBindSender2Recver(std::shared_ptr<CRecver> &recver, std::shared_ptr<CSender> &sender);
    bool UnBindSender2Recver(CRecver *recver, CSender *sender);

    void DestoryRecver(std::shared_ptr<CRecver> &pRecver);

  private:
    void addEvent(int port, const read_cb &f_read_cb);
    void delEvent(int port);
    std::shared_ptr<CRecver> findRecver(const std::string &urn);


  private:
    std::unique_ptr<CNetBase> m_net;
    std::vector<std::shared_ptr<CRecver>> m_tRecvers;

    std::mutex m_RecvMutex;
};

#endif