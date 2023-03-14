#include "rtspServer.h"
#include "Utils/Log.h"
#include "Utils/Utils.h"
#include "Utils/h264SpsParse.h"
#include "define.h"
#include "mediaServer.h"
#include "mp4Reader.h"
#include "netTcp.h"
#include "netbase.h"
#include "rtpRecver.h"
#include "rtspParse.h"
#include "rtspSender.h"
#include "sdp.h"
#include "sender.hpp"

#include <algorithm>
#include <chrono>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <sys/ucontext.h>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace sdp;

TRtspSessionInfo::TRtspSessionInfo()
{
}

TRtspSessionInfo::~TRtspSessionInfo()
{
    if (m_pReader)
    {
        m_pReader->Stop();
    }
    // 解绑本会话的接收器与发送器
    if (m_pSender && m_pRecver && m_eMod == SendLive)
    {
        CMediaServer::Inst().UnBindSender2Recver(m_pRecver, m_pSender);
    }

    if (m_pRecver && m_eMod == Recv)
    {
        CMediaServer::Inst().DestoryRecver(m_pRecver);
    }
}

void TRtspSessionInfo::GetAudioInfo(TAudioInfo &audioInfo)
{
    audioInfo.codecID = GetCodecId(m_tSdpInfo.aCodecName);
    audioInfo.nbChannels = m_tSdpInfo.aChannelNum;
    audioInfo.sample_rate = m_tSdpInfo.aSampleRate;
    audioInfo.freqInx = samples2adtsFreqIdx(audioInfo.sample_rate);
    audioInfo.audioSpecificConfig = m_tSdpInfo.asc;
}

void TRtspSessionInfo::GetVideoInfo(TVideoInfo &videoInfo)
{
    videoInfo.codecID = GetCodecId(m_tSdpInfo.vCodecName);
    videoInfo.timebase.den = m_tSdpInfo.vSampleRate;
    videoInfo.timebase.num = 1;
    videoInfo.vps = m_tSdpInfo.vps;
    videoInfo.sps = m_tSdpInfo.sps;
    videoInfo.pps = m_tSdpInfo.pps;
    videoInfo.h = m_tSdpInfo.height;
    videoInfo.w = m_tSdpInfo.width;
}

CRtspServer::CRtspServer(u_int16_t port) : m_listenPort(port), m_cNet(new CNetTcp("0.0.0.0", port))
{
}

CRtspServer::~CRtspServer()
{
}

bool CRtspServer::Start()
{
    m_sessionFreeThread = std::async(std::launch::async, &CRtspServer::handleSessionFree, this);
    m_cNet->OnConnect(std::bind(&CRtspServer::handleConnect, this, std::placeholders::_1));
    m_cNet->OnDisConnect(std::bind(&CRtspServer::handleDisconnect, this, std::placeholders::_1));
    m_cNet->OnRead(std::bind(&CRtspServer::handleReq, this, std::placeholders::_1));

    m_cNet->Start();
    LOGI("Rtsp Server Start");
    return true;
}

void CRtspServer::handleConnect(const TConnect &tConn)
{
    auto ptSessionInfo = std::make_shared<TRtspSessionInfo>();
    ptSessionInfo->m_tConnect = tConn;
    ptSessionInfo->m_fd = tConn.fd;
    ptSessionInfo->m_strAddr = tConn.addr;

    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        m_tRtspSession.push_back(ptSessionInfo);
        LOGI("Recv New Rtsp Req");
    }
}

void CRtspServer::handleDisconnect(int fd)
{
    std::unique_lock<std::mutex> lock(m_sessionMutex);
    std::vector<std::shared_ptr<CSender>> senders;

    auto it = m_tRtspSession.begin();
    for (; it != m_tRtspSession.end(); ++it)
    {
        if ((*it)->m_fd == fd)
        {
            // 如果断掉的链接是接收器，则同时销毁发送器
            if ((*it)->m_eMod == TRtspSessionInfo::Recv)
            {
                senders = (*it)->m_pRecver->GetSenders();
            }
            m_tRtspSession.erase(it);
            break;
        }
    }

    // 关闭接收器绑定的发送器
    if (!senders.empty())
    {
        for (auto sender : senders)
        {
            auto it = m_tRtspSession.begin();
            for (; it != m_tRtspSession.end(); ++it)
            {
                if (sender == (*it)->m_pSender)
                {
                    // 不能直接关闭链接，会出现fin_wait2
                    // close fd之后 epoll 会自动清除,不需要再去del了
                    // https://stackoverflow.com/questions/8707601/is-it-necessary-to-deregister-a-socket-from-epoll-before-closing-it
                    // 把需要主动断开链接单独放到一个线程里需关闭，防止epoll消息处理线程阻塞
                    {
                        std::lock_guard<std::mutex> lock(m_sessionFreeMutex);
                        m_tRtspSessionTobeFree.push_back(*it);
                    }
                    (*it)->m_pSender->OnRecverDisconnectCB();
                    m_tRtspSession.erase(it);
                    break;
                }
            }
        }
    }

    LOGI("disconnect fd: %d, rtsp session num [%lu]\n", fd, m_tRtspSession.size());
}

std::shared_ptr<TRtspSessionInfo> CRtspServer::getSession(int fd)
{
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto it : m_tRtspSession)
    {
        if (it->m_fd == fd)
            return it;
    }

    return nullptr;
}

std::shared_ptr<TRtspSessionInfo> CRtspServer::getRecver(const std::string uri)
{
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto it : m_tRtspSession)
    {
        if (it->m_strUrn == uri && it->m_eMod == TRtspSessionInfo::Recv)
            return it;
    }

    return nullptr;
}

void CRtspServer::handleSessionFree()
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(m_sessionFreeMutex);
            if (m_tRtspSessionTobeFree.empty())
            {
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            auto pSession = m_tRtspSessionTobeFree.front();
            m_tRtspSessionTobeFree.pop_front();

            SendTearDown(pSession.get());
        }
    }
}

void CRtspServer::handleReq(const TConnect &tConn)
{
    char buffer[1024];
    memset(buffer, 0, 1024);

    auto pSession = getSession(tConn.fd);
    // 可能此时socket已经断链
    if (!pSession)
        return;

    int ret = tConn.recv(buffer, 1024);
    if (ret > 0)
    {
        LOGI("ret %d, \n%s\n", ret, buffer);
        std::string strReq(buffer, ret);
        RtspReq rtspReq(strReq);
        RtspRsp rtspRsp;

        rtspRsp.setStatus(200);
        rtspRsp.setHeader("CSeq", std::to_string(rtspReq.GetCSeq()));
        pSession->m_cSeq = rtspReq.GetCSeq();
        rtspRsp.setHeader("Server", "WangHao");
        if (!pSession->m_strSession.empty())
        {
            rtspRsp.setHeader("Session", pSession->m_strSession);
        }

        std::string mod = rtspReq.GetMod();
        if (mod == "OPTIONS")
        {
            handle_OPTIONS(pSession.get(), rtspReq, rtspRsp);
        }
        else if (mod == "ANNOUNCE")
        {
            handle_ANNOUNCE(pSession.get(), rtspReq, rtspRsp);
        }
        else if (mod == "DESCRIBE")
        {
            handle_DESCRIBE(pSession.get(), rtspReq, rtspRsp);
        }
        else if (mod == "SETUP")
        {
            handle_SETUP(pSession.get(), rtspReq, rtspRsp);
        }
        else if (mod == "PLAY")
        {
            handle_PLAY(pSession.get(), rtspReq, rtspRsp);
        }
        else if (mod == "RECORD")
        {
            handle_RECORD(pSession.get(), rtspReq, rtspRsp);
        }
        else if (mod == "PAUSE")
        {
            handle_PAUSE(pSession.get(), rtspReq, rtspRsp);
        }
        else if (mod == "TEARDOWN")
        {
            handle_TEARDOWN(pSession.get(), rtspReq, rtspRsp);
        }

        std::string strRsp = rtspRsp.toString();
        tConn.send(strRsp.c_str(), strRsp.size());
    }
}

void CRtspServer::handle_OPTIONS(TRtspSessionInfo *pSession, RtspReq &rtspReq, RtspRsp &rtspRsp)
{
    pSession->m_strUrn = rtspReq.GetUri();
    rtspRsp.setHeader("CSeq", std::to_string(rtspReq.GetCSeq()));
    rtspRsp.setHeader(
        "Public", "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, ANNOUNCE, RECORD, SET_PARAMETER, GET_PARAMETER");
}

void CRtspServer::handle_ANNOUNCE(TRtspSessionInfo *pSession, RtspReq &rtspReq, RtspRsp &rtspRsp)
{
    char buffer[1024];
    pSession->m_Url = rtspReq.GetUrl();
    pSession->m_eMod = TRtspSessionInfo::Recv;
    std::string sdp = rtspReq.GetBody();
    // 读到 content 但是没读到body,再读一次
    if (sdp.empty() && rtspReq.GetContentLength())
    {
        memset(buffer, 0, 1024);
        int ret = pSession->m_tConnect.recv(buffer, 1024);
        if (ret <= 0)
        {
            rtspRsp.setStatus(404);
            std::string strRsp = rtspRsp.toString();
            // pSession->m_tConnect.send(strRsp.c_str(), strRsp.size());
            return;
        }

        sdp.assign(buffer, ret);
    }
    parseSDP(sdp, pSession->m_tSdpInfo);

    pSession->m_serverVideoPort = m_curlPort.fetch_add(2);
    pSession->m_strSession = CreateSessionID(12);
    rtspRsp.setHeader("Session", pSession->m_strSession);
}

void CRtspServer::handle_DESCRIBE(TRtspSessionInfo *pSession, RtspReq &rtspReq, RtspRsp &rtspRsp)
{
    pSession->m_Url = rtspReq.GetUrl();
    pSession->m_serverVideoPort = m_curlPort.fetch_add(2);
    pSession->m_strSession = CreateSessionID(12);
    std::string body;
    std::string filePath = "./data/" + pSession->m_strUrn;
    int fileStatus = access(filePath.c_str(), F_OK | R_OK);

    /* record */
    if (fileStatus == 0)
    {
        pSession->m_eMod = TRtspSessionInfo::SendRecord;
        auto reader = std::make_shared<CFileReader>(filePath, pSession->m_strAddr, pSession->m_clientVideoPort,
                                              pSession->m_serverVideoPort, E_STREAM_ES);
        if (!reader->Open())
        {
            LOGE("open file fail\n");
            rtspRsp.setStatus(404);
            return;
        }

        pSession->m_pReader = reader;
        pSession->m_strAccept = rtspReq.GetAccept();

        TMediaInfo tMediaInfo;
        tMediaInfo = reader->GetProperty();

        TSdpInfo tSdpInfo;
        tSdpInfo.vCodecName = GetCodecName(tMediaInfo.video.codecID);
        tSdpInfo.vPt = GetMediaType(tMediaInfo.video.codecID);
        tSdpInfo.vSampleRate = tMediaInfo.video.timebase.den;
        tSdpInfo.aCodecName = GetCodecName(tMediaInfo.audio.codecID);
        tSdpInfo.aSampleRate = tMediaInfo.audio.sample_rate;
        tSdpInfo.aChannelNum = tMediaInfo.audio.nbChannels;
        tSdpInfo.aPt = GetMediaType(tMediaInfo.audio.codecID);

        body = createSDP(tSdpInfo);
    }
    else /* live */
    {
        pSession->m_eMod = TRtspSessionInfo::SendLive;
        auto pRecver = getRecver(pSession->m_strUrn);
        if (!pRecver)
        {
            LOGE("not find stream\n");
            rtspRsp.setStatus(404);
            return;
        }

        body = createSDP(pRecver->m_tSdpInfo);
    }

    rtspRsp.setHeader("x-Accept-Dynamic-Rate", "1");
    rtspRsp.setHeader("x-Accept-Retransmit", "our-retransmit");

    if (!body.empty())
    {
        rtspRsp.setContent(body, "application/sdp");
    }
}
void CRtspServer::handle_SETUP(TRtspSessionInfo *pSession, RtspReq &rtspReq, RtspRsp &rtspRsp)
{
    std::string url = rtspReq.GetUrl();
    TTransPort tTransPort = rtspReq.GetTransport();
    TTransPort rspTransPort;

    if (tTransPort.clientPort > 0)
    {
        /*video*/
        char streamID = url.at(url.size() - 1);
        if (streamID == '0')
        {
            pSession->m_vUrl = url;
            pSession->m_clientVideoPort = tTransPort.clientPort;
            if (pSession->m_eMod == TRtspSessionInfo::Recv)
            {
                TAudioInfo audioInfo;
                pSession->GetAudioInfo(audioInfo);

                TVideoInfo videoInfo;
                pSession->GetVideoInfo(videoInfo);

                pSession->m_pRecver =
                    CMediaServer::Inst().CreateRtspRecver(pSession->m_strUrn, pSession->m_serverVideoPort,
                                                      tTransPort.clientPort, videoInfo, audioInfo);

                if (!pSession->m_pRecver)
                {
                    LOGE("create recv fail\n");
                    rtspRsp.setStatus(404);
                    return;
                }
            }
            else if (pSession->m_eMod == TRtspSessionInfo::SendLive)
            {
                auto pRecver = getRecver(pSession->m_strUrn);
                if (!pRecver)
                {
                    LOGE("not find stream\n");
                    rtspRsp.setStatus(404);
                    return;
                }
                auto pSender =
                    CMediaServer::Inst().CreateRtspSender(pSession->m_strUrn, pSession->m_strAddr,
                                                          pSession->m_clientVideoPort, pSession->m_serverVideoPort);
                pSession->m_pSender = pSender;
                // TODO
                // pSession->m_pSender->SetRecverDisconnectCB([pSender](){});
                //// pSession->m_pRecver = CMediaServer::Inst().BindSender2Recver(pSession->m_strUrn, pSender);
            }
            else if (pSession->m_eMod == TRtspSessionInfo::SendRecord)
            {
                pSession->m_pReader->UpdateClientPort(pSession->m_clientVideoPort);
            }

            rspTransPort.serverPort = pSession->m_serverVideoPort;
        }
        else if (streamID == '1')
        {
            m_curlPort.fetch_add(2);
            pSession->m_aUrl = url;
            pSession->m_clientAudioPort = tTransPort.clientPort;
            rspTransPort.serverPort = pSession->m_serverVideoPort + 2;
        }
    }
    rspTransPort.clientPort = tTransPort.clientPort;
    rtspRsp.setHeader("Transport", rspTransPort.to_string());
}
void CRtspServer::handle_PLAY(TRtspSessionInfo *pSession, RtspReq &rtspReq, RtspRsp &rtspRsp)
{
    // 直播一般 range.end 为空
    if (pSession->m_eMod == TRtspSessionInfo::SendRecord)
    {
        TRange tRange;
        if (rtspReq.GetRange(tRange))
        {
            rtspRsp.setHeader("Range", tRange.to_string());
            pSession->m_pReader->Seek(tRange.begin);
        }
        pSession->m_pReader->Start();
    }

    TRTPInfo tRtpInfo;
    tRtpInfo.videoUrl = pSession->m_vUrl;
    tRtpInfo.audioUrl = pSession->m_aUrl;

    rtspRsp.setHeader("RTP-Info", tRtpInfo.to_string());
    LOGI("start play %s, port: %d", pSession->m_strSession.c_str(), pSession->m_clientVideoPort);
}

void CRtspServer::handle_RECORD(TRtspSessionInfo *pSession, RtspReq &rtspReq, RtspRsp &rtspRsp)
{
    TRTPInfo tRtpInfo;
    tRtpInfo.videoUrl = pSession->m_vUrl;
    tRtpInfo.audioUrl = pSession->m_aUrl;
    rtspRsp.setHeader("RTP-Info", tRtpInfo.to_string());
    LOGI("start recv %s, port: %d", pSession->m_strSession.c_str(), pSession->m_clientVideoPort);
}

void CRtspServer::handle_PAUSE(TRtspSessionInfo *pSession, RtspReq &rtspReq, RtspRsp &rtspRsp)
{
    pSession->m_pReader->Pause();
}

void CRtspServer::handle_TEARDOWN(TRtspSessionInfo *pSession, RtspReq &rtspReq, RtspRsp &rtspRsp)
{
    auto pRecver = getRecver(pSession->m_strUrn);
    if (!pRecver)
    {
        LOGE("not find stream\n");
        rtspRsp.setStatus(404);
        return;
    }

    // pRecver->m_pRecver->UnBindSender(pSession->m_pSender);
}
void CRtspServer::SendTearDown(TRtspSessionInfo *pSession)
{
    RtspReq2Client req;
    req.SetMod("TEARDOWN", pSession->m_Url);
    req.SetHeader("CSeq", std::to_string(++(pSession->m_cSeq)));
    req.SetHeader("Session", pSession->m_strSession);

    std::string strReq = req.toString();
    pSession->m_tConnect.send(strReq.c_str(), strReq.size());
}

/**
 * @brief 创建server SDP
 *
 * @param tMediaInfo
 * @return std::string
 */
std::string CRtspServer::createSDP(const TSdpInfo &tSdpInfo)
{
    sdp::SdpRoot sdpRoot;

    /* v= */
    SdpVersion *ver = (SdpVersion *)(SdpFactory::createNode(sdp::SDP_NODE_VERSION));
    ver->version = 0;
    sdpRoot.add(ver);

    /* o= */
    SdpOrigin *sdpOri = (SdpOrigin *)SdpFactory::createNode(sdp::SDP_NODE_ORIGIN);
    sdpOri->userName = '-';
    sdpOri->sid = "0";
    sdpOri->sessVersion = 0;
    sdpOri->netType = SDP_NET_IN;
    sdpOri->addrType = SDP_ADDR_IP4;
    sdpOri->addr = "0.0.0.0";
    sdpRoot.add(sdpOri);

    /* s= */
    SdpSessName *sessionName = (SdpSessName *)SdpFactory::createNode(sdp::SDP_NODE_SESSION_NAME);
    sessionName->name = "Play";
    sdpRoot.add(sessionName);

    /* c= */
    SdpConn *conn = (SdpConn *)SdpFactory::createNode(sdp::SDP_NODE_CONNECTION);
    conn->netType = SDP_NET_IN;
    conn->addrType = SDP_ADDR_IP4;
    conn->addr = "0.0.0.0";
    sdpRoot.add(conn);

    /* t= */
    SdpTime *time = (SdpTime *)SdpFactory::createNode(sdp::SDP_NODE_TIME);
    time->start = 0;
    time->stop = 0;
    sdpRoot.add(time);

    /* a=range:npt*/
    SdpAttr *range = (SdpAttr *)SdpFactory::createNode(sdp::SDP_NODE_ATTRIBUTE);
    range->attrType = SDP_ATTR_RANGE;
    range->val = "npt=now-";
    sdpRoot.add(range);

    /* a=control:**/
    SdpAttr *control = (SdpAttr *)SdpFactory::createNode(sdp::SDP_NODE_ATTRIBUTE);
    control->attrType = sdp::SDP_ATTR_CONTROL;
    control->val = "*";
    sdpRoot.add(control);

    /* m=video */
    int videoPt = tSdpInfo.vPt;
    SdpMedia *video = (SdpMedia *)SdpFactory::createNode(sdp::SDP_NODE_MEDIA);
    video->mediaType = SDP_MEDIA_VIDEO;
    video->port = 0;
    video->proto = SDP_PROTO_RTP_AVP;
    video->supportedPTs = {videoPt};

    SdpAttrFmtp *fmtp = (SdpAttrFmtp *)SdpFactory::createAttr(sdp::SDP_ATTR_FMTP);
    fmtp->pt = videoPt;
    // fmtp->param = "packetization-mode=1; profile-level-id=42002A;
    // sprop-parameter-sets=Z0IAKpY1QPAET8s3AQEBAg==,aM4xsg==";

    fmtp->param = tSdpInfo.vFtm;
    video->addCandidate(fmtp);

    SdpAttrRtpMap *vRtpMap = (SdpAttrRtpMap *)SdpFactory::createAttr(sdp::SDP_ATTR_RTPMAP);
    vRtpMap->pt = videoPt;
    vRtpMap->enc = tSdpInfo.vCodecName + "/" + std::to_string(tSdpInfo.vSampleRate);
    // vRtpMap->enc = "H264/12800";
    video->addCandidate(vRtpMap);

    SdpAttr *vControl = (SdpAttr *)SdpFactory::createAttr(sdp::SDP_ATTR_CONTROL);
    vControl->val = "trackID=0";
    video->addCandidate(vControl);

    /*add video*/
    sdpRoot.add(video);

    /* m=audio*/
    int audioPt = tSdpInfo.aPt;
    SdpMedia *audio = (SdpMedia *)SdpFactory::createNode(sdp::SDP_NODE_MEDIA);
    audio->mediaType = sdp::SDP_MEDIA_AUDIO;
    audio->port = 0;
    audio->proto = SDP_PROTO_RTP_AVP;
    audio->supportedPTs = {audioPt};

    SdpAttrRtpMap *aRtpMap = (SdpAttrRtpMap *)SdpFactory::createAttr(sdp::SDP_ATTR_RTPMAP);
    aRtpMap->pt = audioPt;

    // TODO create
    SdpAttrFmtp *afmtp = (SdpAttrFmtp *)SdpFactory::createAttr(sdp::SDP_ATTR_FMTP);
    afmtp->pt = audioPt;
    if (tSdpInfo.aFtm.empty())
        afmtp->param = "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210";
    else
        afmtp->param = tSdpInfo.aFtm;
    audio->addCandidate(afmtp);

    std::string enc =
        tSdpInfo.aCodecName + "/" + std::to_string(tSdpInfo.aSampleRate) + "/" + std::to_string(tSdpInfo.aChannelNum);
    // aRtpMap->enc = "PCMA/8000/1";
    aRtpMap->enc = enc;
    audio->addCandidate(aRtpMap);

    SdpAttr *aControl = (SdpAttr *)SdpFactory::createAttr(sdp::SDP_ATTR_CONTROL);
    aControl->val = "trackID=1";
    audio->addCandidate(aControl);

    /*add audio*/
    sdpRoot.add(audio);

    std::string sdp;
    SdpWriter().write(sdp, sdpRoot);

    return sdp;
}

/**
 * @brief 提取客户端SDP
 *
 * @param sdp
 * @param tSdpInfo
 */
void CRtspServer::parseSDP(const std::string &sdp, TSdpInfo &tSdpInfo)
{
    sdp::SdpRoot sdpRoot;
    sdp::SdpReader read;
    if (read.parse(sdp, sdpRoot) != 0)
    {
        LOGE("GetVideoPayLoadNew failed %s\n", sdp.c_str());
        return;
    }

    std::vector<SdpNode *> mediaList;
    sdpRoot.find(SDP_NODE_MEDIA, mediaList);

    for (auto it : mediaList)
    {
        SdpMedia *media = (SdpMedia *)it;

        SdpAttr *rtpMap = nullptr;
        media->find(SDP_ATTR_RTPMAP, rtpMap);

        if (rtpMap)
        {
            if (media->mediaType == SDP_MEDIA_VIDEO)
            {
                tSdpInfo.vPt = ((SdpAttrRtpMap *)rtpMap)->pt;
                std::string enc = ((SdpAttrRtpMap *)rtpMap)->enc;

                std::smatch out;
                std::regex value("(.+)/(\\d+)");
                if (std::regex_search(enc, out, value))
                {
                    tSdpInfo.vCodecName = out[1].str();
                    tSdpInfo.vSampleRate = stoi(out[2].str());
                }
            }
            else if (media->mediaType == sdp::SDP_MEDIA_AUDIO)
            {
                tSdpInfo.aPt = ((SdpAttrRtpMap *)rtpMap)->pt;
                std::string enc = ((SdpAttrRtpMap *)rtpMap)->enc;

                std::smatch out;
                std::regex value("(.+)/(\\d+)/(\\d+)");
                if (std::regex_search(enc, out, value))
                {
                    tSdpInfo.aCodecName = out[1].str();
                    tSdpInfo.aSampleRate = stoi(out[2].str());
                    tSdpInfo.aChannelNum = stoi(out[3].str());
                }
            }
        }

        SdpAttr *fmtp = nullptr;
        media->find(SDP_ATTR_FMTP, fmtp);
        if (fmtp)
        {
            if (media->mediaType == SDP_MEDIA_VIDEO)
            {
                tSdpInfo.vFtm = ((SdpAttrFmtp *)fmtp)->param;
            }
            else if (media->mediaType == sdp::SDP_MEDIA_AUDIO)
            {
                tSdpInfo.aFtm = ((SdpAttrFmtp *)fmtp)->param;
            }
        }
    }

    if (tSdpInfo.vCodecName == "H265")
    {
        extract_vps_sps_pps(sdp, tSdpInfo.vps, tSdpInfo.sps, tSdpInfo.pps);
    }
    else
    {
        extract_sps_pps(sdp, tSdpInfo.sps, tSdpInfo.pps);
        if (!tSdpInfo.sps.empty())
        {
            sps_info_struct info;
            memset(&info, 0, sizeof(sps_info_struct));
            h264_parse_sps(&tSdpInfo.sps[0], tSdpInfo.sps.size(), &info);
            tSdpInfo.width = info.width;
            tSdpInfo.height = info.height;
        }
    }

    // #TODO prase from sdp
    // https://blog.csdn.net/Ritchie_Lin/article/details/121730749
    tSdpInfo.asc.push_back(0x12);
    tSdpInfo.asc.push_back(0x10);
}
