#ifndef _RTP_SEND_ES_H_
#define _RTP_SEND_ES_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

#include "define.h"
#include "recver.hpp"
#include "sender.hpp"

// #define SAVE_FILE

using TExtData = std::vector<std::pair<int, std::string>>;

class CRtspSender : public CSender
{
  public:
    CRtspSender(std::string desHost, uint16_t destPort, uint16_t localPort, CRecver *pRecver = nullptr);
    ~CRtspSender();

    virtual void Send(const std::shared_ptr<Frame> &tFrame) override;
    virtual void Send(const uint8_t *buff, size_t size, bool isVideo) override;
    virtual void SetRecverDisconnectCB(std::function<void()> cb) override;
    virtual void OnRecverDisconnectCB() override;

    virtual E_STREAM_TYPE StreamType() override
    {
        return E_STREAM_ES;
    }
    virtual void UpdateRemotePort(uint16_t port) override
    {
        m_dwDestPort = port;
    }
    virtual void SetRecver(CRecver *pRecver) override
    {
        m_pRecver = pRecver;
        m_vCodecID = pRecver->VideoInfo().codecID;
        m_aCodecID = pRecver->AudioInfo().codecID;
    }

  private:
    void sendH264(Frame &tFrame);
    void sendHevc(Frame &tFrame);
    void sendAudio(Frame &tFrame);
    uint32_t findExtData(uint8_t *pData, uint32_t dwSize);
    void sendExtData(uint64_t qwPts, uint16_t pt);
    uint32_t findFramePos(uint8_t *pData, uint32_t dwSize);
    int makeRtpHeader(uint8_t *pData, int marker_flag, unsigned short cseq, long long curpts, unsigned int ssrc,
                      uint16_t pt);
    int makeAACHeader(uint8_t *pData, int frameSize);
    bool makeH264SliceNalu(uint8_t *pData, uint8_t srcNalu, bool bStart, bool bEnd);
    bool makeHevcSliceNalu(uint8_t *pData, uint8_t *srcNalu, bool bStart, bool bEnd);
    int sendRtpPkt(const uint8_t *pData, uint32_t dwSize, bool bIsVideo);

  private:
    int m_videoSocket;
    int m_audioSocket;

    std::string m_strDestHost;
    uint32_t m_dwDestPort;
    uint32_t m_dwLocalPort;

    uint64_t m_qwSsrc;
    uint16_t m_u16VideoSeq = 1;
    uint16_t m_u16AudioSeq = 1;

    TExtData m_tExtData;

    int m_vCodecID;
    int m_aCodecID;
    uint32_t m_sendFrames = 0;
    std::function<void()> m_recvDisconnectCB;
    CRecver *m_pRecver;

#ifdef SAVE_FILE
    FILE *m_pFileVideo = nullptr;
#endif
};

#endif