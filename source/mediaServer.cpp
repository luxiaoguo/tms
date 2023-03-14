#include "mediaServer.h"
#include "Utils/Log.h"
#include "flvSender.h"
#include "netTcp.h"
#include "netUdp.h"
#include "recver.hpp"
#include "rtpRecver.h"
#include "rtspSender.h"
#include "sender.hpp"
#include <cstring>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/sendfile.h>


CMediaServer& CMediaServer::Inst()
{
    static bool start = false;
    static CMediaServer inst;
    if (start == false)
    {
        start = true;
        inst.Start();
    }

    return inst;
}

CMediaServer::CMediaServer():m_net(new CNetUdp())
{
}

CMediaServer::~CMediaServer()
{
}

void CMediaServer::Start()
{
    m_net->Start();
}

std::shared_ptr<CRecver> CMediaServer::CreateRtspRecver(const std::string &urn,
                                                    uint16_t serverPort, uint16_t remotePort, const TVideoInfo & videoInfo,
                                                    const TAudioInfo & audioInfo)
{
    if (findRecver(urn))
        return nullptr;

    auto recver = std::make_shared<CRtpRecver>(urn, serverPort, remotePort);
    {
        std::lock_guard<std::mutex> lock(m_RecvMutex);
        m_tRecvers.push_back(recver);
    }
    recver->m_tVideoInfo = videoInfo;
    recver->m_tAudioInfo = audioInfo;
    return recver;
}

std::shared_ptr<CSender> CMediaServer::CreateRtspSender(const std::string &urn,
                                                        const std::string &destHost, uint16_t serverPort,
                                                        uint16_t remotePort)
{
    auto recver = findRecver(urn);
    if (!recver)
        return nullptr;
    std::shared_ptr<CSender> sender = nullptr;
    sender = std::make_shared<CRtspSender>(destHost, serverPort, remotePort);
    recver->bindSender(sender);
    sender->SetRecver(recver.get());
    return sender;
}
std::shared_ptr<CSender> CMediaServer::CreateFlvSender(const std::string &urn, const std::shared_ptr<CWriter> &writer)
{
    auto recver = findRecver(urn);
    if (!recver)
        return nullptr;

    std::shared_ptr<CSender> sender = std::make_shared<CFlvSender>((CRecver*)recver.get(), urn, writer);
    recver->bindSender(sender);
    return sender;
}

//// 
std::shared_ptr<CRecver> CMediaServer::BindSender2Recver(const std::string &urn, std::shared_ptr<CSender> &sender)
{
    auto recver = findRecver(urn);
    if (!recver)
        return nullptr;

    recver->bindSender(sender);
    sender->SetRecver(recver.get());
    LOGD("bind %s to sender\n", urn.c_str());
    return recver;
}

bool CMediaServer::UnBindSender2Recver(std::shared_ptr<CRecver> &recver, std::shared_ptr<CSender> &sender)
{
    recver->unBindSender(sender);
    LOGD("unbind %s to sender\n", recver->Urn().c_str());
    return true;
}

bool CMediaServer::UnBindSender2Recver(CRecver *recver, CSender *sender)
{
    recver->unBindSender(sender);
    LOGD("unbind %s to sender\n", recver->Urn().c_str());
    return true;
}

void CMediaServer::DestoryRecver(std::shared_ptr<CRecver> &pRecver)
{
    std::lock_guard<std::mutex> lock(m_RecvMutex);
    auto it = m_tRecvers.begin();

    for (;it != m_tRecvers.end(); ++it)
    {
        if ((*it).get() == pRecver.get())
        {
            m_tRecvers.erase(it);
            return;
        }
    }
}

void CMediaServer::addEvent(int port, const read_cb &f_read_cb)
{
    m_net->AddEvent(port, f_read_cb);
}

void CMediaServer::delEvent(int port)
{
    m_net->DelEvent(port);
}

std::shared_ptr<CRecver> CMediaServer::findRecver(const std::string &urn)
{
    std::lock_guard<std::mutex> lock(m_RecvMutex);
    for (const auto &it : m_tRecvers)
    {
        if (it->Urn() == urn)
        {
            return it;
        }
    }

    return nullptr;
}