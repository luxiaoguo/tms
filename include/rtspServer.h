#ifndef _RTSP_SERVER_H_
#define _RTSP_SERVER_H_

#include "define.h"
#include "mediaServer.h"
#include "mp4Reader.h"
#include "netbase.h"
#include "rtpRecver.h"
#include "rtspParse.h"
#include "sender.hpp"

#include <atomic>
#include <cstdint>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct TRtspSessionInfo
{
    enum E_MOD
    {
        Recv,
        SendLive,
        SendRecord
    };
    TRtspSessionInfo();
    ~TRtspSessionInfo();

    void GetAudioInfo(TAudioInfo &audioInfo);
    void GetVideoInfo(TVideoInfo &videoInfo);

    int m_fd;
    TConnect m_tConnect;
    std::string m_strUrn;
    std::string m_strAddr;
    std::string m_strSession;
    std::string m_Url;
    std::string m_vUrl;
    std::string m_aUrl;
    std::string m_strAccept;
    uint16_t m_clientVideoPort;
    uint16_t m_clientAudioPort;
    uint16_t m_serverVideoPort;
    TSdpInfo m_tSdpInfo;
    E_MOD m_eMod;
    uint32_t m_cSeq;

    std::shared_ptr<CFileReader> m_pReader = nullptr;
    std::shared_ptr<CSender> m_pSender = nullptr;
    std::shared_ptr<CRecver> m_pRecver = nullptr;
};

class CRtspServer
{
  public:
    CRtspServer(uint16_t port = 554);
    ~CRtspServer();

  public:
    bool Start();

  private:
    void handleConnect(const TConnect &tConn);
    void handleDisconnect(int fd);
    void handleReq(const TConnect &tReq);
    // 处理主动断开连接的情况
    void handleSessionFree();

    std::shared_ptr<TRtspSessionInfo> getSession(int fd);
    std::shared_ptr<TRtspSessionInfo> getRecver(const std::string uri);

    std::string createSDP(const TSdpInfo &tSdpInfo);
    void parseSDP(const std::string &sdp, TSdpInfo &tSdpInfo);

    void handle_OPTIONS(TRtspSessionInfo *session, RtspReq &rtspReq, RtspRsp &rtspRsp);
    void handle_ANNOUNCE(TRtspSessionInfo *session, RtspReq &rtspReq, RtspRsp &rtspRsp);
    void handle_DESCRIBE(TRtspSessionInfo *session, RtspReq &rtspReq, RtspRsp &rtspRsp);
    void handle_SETUP(TRtspSessionInfo *session, RtspReq &rtspReq, RtspRsp &rtspRsp);
    void handle_PLAY(TRtspSessionInfo *session, RtspReq &rtspReq, RtspRsp &rtspRsp);
    void handle_RECORD(TRtspSessionInfo *session, RtspReq &rtspReq, RtspRsp &rtspRsp);
    void handle_PAUSE(TRtspSessionInfo *session, RtspReq &rtspReq, RtspRsp &rtspRsp);
    void handle_TEARDOWN(TRtspSessionInfo *session, RtspReq &rtspReq, RtspRsp &rtspRsp);

    void SendTearDown(TRtspSessionInfo *session);

  private:
    uint16_t m_listenPort;
    std::unique_ptr<CNetBase> m_cNet;

    std::list<std::shared_ptr<TRtspSessionInfo>> m_tRtspSession;
    std::list<std::shared_ptr<TRtspSessionInfo>> m_tRtspSessionTobeFree;
    std::mutex m_sessionMutex;
    std::mutex m_sessionFreeMutex;
    std::future<void> m_sessionFreeThread;
    std::atomic_int32_t m_curlPort{5000};
};
#endif