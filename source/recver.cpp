#include "recver.hpp"

CRecver::CRecver()
{

}

CRecver::~CRecver()
{

}

void CRecver::bindSender(std::shared_ptr<CSender> &cSender)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vSenders.push_back(cSender);
}

void CRecver::unBindSender(std::shared_ptr<CSender> &cSender)
{
    unBindSender(cSender.get());
}

void CRecver::unBindSender(CSender *pcSender)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_vSenders.begin();
    for (; it < m_vSenders.end(); ++it)
    {
        if (it->get() == pcSender)
        {
            m_vSenders.erase(it);
            return;
        }
    }
}

const TAudioInfo& CRecver::AudioInfo()
{
    return m_tAudioInfo;
}
const TVideoInfo& CRecver::VideoInfo()
{
    return m_tVideoInfo;
}
