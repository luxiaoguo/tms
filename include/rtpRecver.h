#ifndef _RTP_RECVER_H_
#define _RTP_RECVER_H_

#include "define.h"
#include "recver.hpp"
#include "sender.hpp"
#include "Utils/pts2dts.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <future>
#include <locale>
#include <memory>
#include <mutex>
#include <set>
#include <stack>
#include <queue>
#include <string>
#include <vector>
#include <list>
#include <map>
#define OUT

class CRtpHeader
{
  public:
    CRtpHeader();
    ~CRtpHeader();

    bool encode();
    bool decode(const uint8_t *pSrc, uint16_t size);
    uint16_t size();

  public:
    uint8_t padding_length;
    uint8_t cc;
    bool marker;
    uint8_t payload_type;
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
    uint32_t csrc[15];
    // todo extensions_;
    bool ignore_padding;
};

/**
Before C++17:
std::shared_ptr<char> ptr(new char[size_], std::default_delete<char[]>());

Since C++17:
shared_ptr gains array support similar to what unique_ptr already had from the beginning:
std::shared_ptr<char[]> ptr(new char[size_]);
*/

class CRtpPayload
{
  public:
    uint16_t seq;
    uint8_t naluType;
    bool start = false;
    bool end = false;
    std::shared_ptr<uint8_t> payload;
    uint16_t payloadSize;
    
  bool operator<(const CRtpPayload& in)
  {
      return seq < in.seq;
  }

};

class CRtpPayloadStapA
{
  public:
    bool encode();
    bool decode(const uint8_t *pSrc, uint16_t size);
    bool decodeHevc(const uint8_t *pSrc, uint16_t size);

    std::vector<CRtpPayload> payloads;
};

class CRtpPlyloadFuA
{
  public:

    bool recvStart = false;
    bool recvEnd = false;

    std::list<CRtpPayload> payloads;
    bool CheckComplete();
    void Sort();
};

class CRtpPacketAAC
{
  public:
    CRtpPacketAAC(CRecver *recver);
    ~CRtpPacketAAC();
  public:

    std::shared_ptr<Frame> decode(const uint8_t *pData, uint16_t size);
    void encode();

  private:
    CRecver *m_recver;
    CRtpHeader m_header;
};

// only for video
class CRtpPacket
{
  public:
    CRtpPacket(){}
    ~CRtpPacket(){}

  public:
    void decode(const uint8_t *pData, uint16_t size);
    void decodeHevc(const uint8_t *pData, uint16_t size);
    void encode(uint8_t *pData, uint16_t size);
    const CRtpHeader &Header();
    uint8_t NaluType();
    const CRtpPayload &Payload();
    uint16_t PayloadSize();

  private:
    uint8_t m_nalu;
    CRtpHeader m_header;
    CRtpPayload m_payload;
};

class CRtpRecver;
class CFrameCache
{
  public:
    CFrameCache(CRecver *m_pRecver);
    ~CFrameCache();

  public:
    void pushVideo(const uint8_t *pData, uint16_t size); 
    void pushAudio(const uint8_t *pData, uint16_t size); 
    std::shared_ptr<Frame> PopFrame();

  private:
    void pushFuAToCache1(CRtpPacket &pkt, const uint8_t *pData, uint16_t size);
    void cacheFrame(E_MEDIA_TYPE eMediaType, E_FRAME_TYPE eFrameType, const CRtpHeader &rtpHeader,
                    std::shared_ptr<uint8_t> data, uint32_t dataSize);
    void cacheFrame(std::shared_ptr<Frame> &&tFrame);

  private:
    std::set<std::shared_ptr<Frame>, SharedPtrComp<Frame>> m_frameCache;
    // this cache wait first key frame for calc dts
    std::set<std::shared_ptr<Frame>, SharedPtrComp<Frame>> m_frameCache2;
    std::atomic_bool m_recvFristKeyFrame{false};
    std::mutex m_framesLock;
    std::map<uint32_t, CRtpPlyloadFuA> m_pktCache;
    std::mutex m_cacheLock;
    uint32_t m_sumInB = 0;
    dtsgen_t m_dtsgen;
    std::mutex m_dtsLock;
    CRecver *m_pRecver;
};


class CRtpRecver : public CRecver
{
  public:
    CRtpRecver(const std::string urn, uint16_t localPort, uint16_t remotePort);
    virtual ~CRtpRecver();

    std::string Urn() override;
    std::vector<std::shared_ptr<CSender>> GetSenders() override;
    
    bool operator==(const CRtpRecver *in)
    {
        return in->m_urn == m_urn;
    }

  private:
    // void bindSender(std::shared_ptr<CSender>& cSender) override;
    // void unBindSender(std::shared_ptr<CSender>& cSender) override;
    void append(const char *buff, std::size_t size, bool isVideo) override;

    void on_video(const TConnect &tConn);
    void on_aduio(const TConnect &tConn);

    void parseVideo(const uint8_t *buff, std::size_t size);
    void parseAudio(const uint8_t *buff, std::size_t size);

    void send();

  private:
    std::string m_urn;
    uint16_t m_localPort;
    uint16_t m_remotePort;

    // 先不限制缓存长度
    std::deque<uint8_t> m_buffer;
    // std::vector<std::shared_ptr<CSender>> m_vSenders;
    // std::mutex m_mutex;

    uint32_t m_videoFrameRecv = 0;
    uint32_t m_audioFrameRecv = 0;
    CFrameCache m_frameCache;
    std::future<void> m_sendThread;
};

#endif