#include "rtpRecver.h"
#include "Utils/Log.h"
#include "Utils/Utils.h"
#include "define.h"
#include "flvSender.h"
#include "hlsWriter.h"
#include "libavcodec/codec_id.h"
#include "mediaServer.h"
#include "recver.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <numeric>
#include <resolv.h>
#include <string>
#include <thread>
#include <utility>


/* @see https://tools.ietf.org/html/rfc1889#section-5.1
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           timestamp                           |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |           synchronization source (SSRC) identifier            |
 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 |            contributing source (CSRC) identifiers             |
 |                             ....                              |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
CRtpHeader::CRtpHeader()
{
}

CRtpHeader::~CRtpHeader()
{

}


bool CRtpHeader::encode()
{
    return true;
}


bool CRtpHeader::decode(const uint8_t *pSrc, uint16_t size)
{
    if (size < RTP_HDR_LEN)
        return false;

    marker = pSrc[1] >> 7;
    payload_type = pSrc[1] >> 1;
    sequence = GetBE16(&pSrc[2]);
    timestamp = GetBE32(&pSrc[4]);
    ssrc = GetLE64(&pSrc[8]);

    return true;
}

uint16_t CRtpHeader::size()
{
    return RTP_HDR_LEN;
}

const CRtpHeader& CRtpPacket::Header()
{
    return m_header;
}

bool CRtpPayloadStapA::encode()
{
    return true;
}

bool CRtpPayloadStapA::decode(const uint8_t *pSrc, uint16_t size)
{
    /*0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                          RTP Header                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |STAP-A NAL HDR |         NALU 1 Size           | NALU 1 HDR    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         NALU 1 Data                           |
    :                                                               :
    +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |               | NALU 2 Size                   | NALU 2 HDR    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         NALU 2 Data                           |
    :                                                               :
    |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                               :...OPTIONAL RTP padding        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/
    // 跳start_code + STAP-A NAL HDR 
    uint16_t pos = 4 + 1;
    while (true)
    {
        CRtpPayload payload;
        uint16_t naluSize = GetBE16(pSrc + pos);
        pos += 2;
        payload.naluType = pSrc[pos] & 0x1f;
        uint8_t naluType = pSrc[pos] & 0x1f;

        uint8_t startCodeLen = 4;
        if (naluType == H264_SEI || naluType == H264_SPS || naluType == H264_PPS ||
            naluType == H264_AUD)
        {
            startCodeLen = 3;
        }

        payload.payload.reset(new uint8_t[naluSize + startCodeLen], std::default_delete<uint8_t[]>());
        memcpy(payload.payload.get() , StartCode(startCodeLen), startCodeLen);
        payload.payloadSize = naluSize + startCodeLen;
        memcpy(payload.payload.get() + startCodeLen, pSrc + pos, payload.payloadSize - startCodeLen);

        pos += (naluSize);
        payloads.push_back(payload);

        if (pos >= size)
        {
            break;
        }
    }
    
    return true;
}

bool CRtpPayloadStapA::decodeHevc(const uint8_t *pSrc, uint16_t size)
{
    /*0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                          RTP Header                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |STAP-A NAL HDR 16b          |         NALU 1 Size 16b          | 
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   NALU 1 HDR  16b          |         NALU 1 Data              |
    :                                                               :
    :                   NALU 1 Data                                 :
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   NALU 2 Size 16b         |           NALU 2 HDR 16b          |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         NALU 2 Data                           |
    :                                                               :
    |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                               :...OPTIONAL RTP padding        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/
    // 跳start_code + STAP-A NAL HDR 
    uint16_t pos = 4 + 2;
    while (true)
    {
        CRtpPayload payload;
        uint16_t naluSize = GetBE16(pSrc + pos);
        pos += 2;
        payload.naluType = (pSrc[pos] & 0x7e) >> 1;
        uint8_t naluType = pSrc[pos] & 0x1f;

        uint8_t startCodeLen = 4;
        if (naluType == H265_VPS || naluType == H265_SPS || naluType == H265_PPS ||
            naluType == H265_SEI)
        {
            startCodeLen = 3;
        }

        payload.payload.reset(new uint8_t[naluSize + startCodeLen], std::default_delete<uint8_t[]>());
        memcpy(payload.payload.get() , StartCode(startCodeLen), startCodeLen);
        payload.payloadSize = naluSize + startCodeLen;
        memcpy(payload.payload.get() + startCodeLen, pSrc + pos, payload.payloadSize - startCodeLen);

        pos += (naluSize);
        payloads.push_back(payload);

        if (pos >= size)
        {
            break;
        }
    }
    
    return true;
}

bool CRtpPlyloadFuA::CheckComplete()
{
    if (recvStart && recvEnd)
    {
        uint16_t startSeq = 0, endSeq = 0;
        for (const auto &it : payloads)
        {
            if (it.start)
            {
                startSeq = it.seq;
            }
            if (it.end)
            {
                endSeq = it.seq;
            }
        }

        if (uint16_t(endSeq - startSeq) == payloads.size() - 1u)
            return true;
    }
    
    return false;
}

void CRtpPlyloadFuA::Sort()
{
    payloads.sort();
}

uint8_t CRtpPacket::NaluType()
{
    return m_nalu;
}

const CRtpPayload &CRtpPacket::Payload()
{
    return m_payload;
}

CRtpPacketAAC::CRtpPacketAAC(CRecver *pRecver):m_recver(pRecver)
{
}

CRtpPacketAAC::~CRtpPacketAAC()
{
}

std::shared_ptr<Frame> CRtpPacketAAC::decode(const uint8_t *pData, uint16_t size)
{
    m_header.decode(pData, size);

    uint8_t adts_hdr[7] = {0};

    uint16_t pos = m_header.size();
    uint16_t auHeaderNum = GetBE16(pData + pos) / 16;
    pos += 2;

    std::vector<uint16_t> auHeaders;
    for (uint16_t index = 0; index < auHeaderNum; ++index)
    {
        uint16_t lenBit = 0;
        lenBit = (pData[pos++] & 0x1f) << 8;
        lenBit |= pData[pos++];

        auHeaders.push_back(lenBit/8);
    }

    // 把多个aac加上adts头，然后放到一帧里面防止ts过多填充
    uint16_t sumLen = std::accumulate(auHeaders.begin(), auHeaders.end(), 0);
    auto payload =
        std::shared_ptr<uint8_t>(new uint8_t[7 * auHeaders.size() + sumLen], std::default_delete<uint8_t[]>());
    auto p = payload.get();

    uint32_t profile = 2;
    for (auto len : auHeaders)
    {
        if (pos + len > size)
            return nullptr;

        adts_hdr[0] = (char)0xFF; // 11111111     = syncword
        adts_hdr[1] = (char)0xF9; // 1111 1 00 1  = syncword MPEG-2 Layer CRC
        adts_hdr[2] = (char)(((profile - 1) << 6) + (m_recver->AudioInfo().freqInx << 2) + (m_recver->AudioInfo().nbChannels >> 2));
        adts_hdr[6] = (char)0xFC;
        adts_hdr[3] = (char)(((m_recver->AudioInfo().nbChannels & 3) << 6) + ((7 + len) >> 11));
        adts_hdr[4] = (char)(((len + 7) & 0x7FF) >> 3);
        adts_hdr[5] = (char)((((len + 7) & 7) << 5) + 0x1F);
        memcpy(p, adts_hdr, 7);
        p += 7;
        memcpy(p, pData + pos, len);
        p += len;

        pos += len;
    }

    auto frame = std::make_shared<Frame>();
    frame->m_eMediaType = E_MEDIA_AUDIO;
    frame->m_eFrameType = E_FRAME_I;
    frame->m_seq = m_header.sequence;
    frame->m_pts = m_header.timestamp;
    frame->m_dts = frame->m_pts;
    frame->SetData(payload, 7 * auHeaders.size() + sumLen, 0);
    return frame;
}

void CRtpPacketAAC::encode()
{
    // todo
}

uint16_t CRtpPacket::PayloadSize()
{
    return m_payload.payloadSize;
}

void CRtpPacket::encode(uint8_t *pData, uint16_t size)
{
    // todo
}

void CRtpPacket::decode(const uint8_t *pData, uint16_t size)
{
    m_header.decode(pData, size);
    uint16_t pos = m_header.size();
    // uint8_t extNaluHDL = 0;

    m_nalu = pData[pos] & 0x1f;
    m_payload.naluType = m_nalu;
    uint8_t naluType =  pData[pos] & 0x1f;
    uint8_t startCodeLen = 4;

    if (naluType == H264_FU_A)
    {
        // extNaluHDL = 1;
        uint8_t srcNalu = pData[pos++] & 0x60;
        srcNalu |= (pData[pos] & 0x1f);
        m_payload.naluType = (pData[pos] & 0x1f);
        m_payload.start = (pData[pos] & 0x80) >> 7;
        m_payload.end = (pData[pos] & 0x40) >> 6;

        if (m_payload.start)
        {
            // 恢复nalu
            uint8_t *data = const_cast<uint8_t *>(pData);
            data[pos] = srcNalu;
        }
        else
        {
            pos += 1;
            startCodeLen = 0;
        }
    }

    m_payload.seq = m_header.sequence;
    if (m_payload.naluType == H264_SEI || m_payload.naluType == H264_SPS || m_payload.naluType == H264_PPS ||
        m_payload.naluType == H264_AUD)
    {
        startCodeLen = 3;
    }

    // m_payload.payloadSize = size - m_header.size() + startCodeLen - extNaluHDL;
    m_payload.payloadSize = size + startCodeLen - pos;
    m_payload.payload.reset(new uint8_t[m_payload.payloadSize], std::default_delete<uint8_t[]>());

    // add start code
    memcpy(m_payload.payload.get(), StartCode(startCodeLen), startCodeLen);
    memcpy(m_payload.payload.get() + startCodeLen, &pData[pos], m_payload.payloadSize - startCodeLen);
}

void CRtpPacket::decodeHevc(const uint8_t *pData, uint16_t size)
{
    m_header.decode(pData, size);
    uint16_t pos = m_header.size();

    m_nalu = (pData[pos] & 0x7e) >> 1;
    m_payload.naluType = m_nalu;
    uint8_t naluType =  m_nalu;
    uint8_t startCodeLen = 4;

    if (naluType == H265_FU_A)
    {
        // 2B FU identifier + FU Header 与264类似
        uint8_t srcNalu[2] = {0};
        // pos不偏移了，后面还要拷贝
        srcNalu[0] = pData[pos++] & 0x81;
        srcNalu[1] = pData[pos];
        // extNaluHDL = 1;
        m_payload.naluType = pData[pos + 1] & 0x3f;
        srcNalu[0] |= (m_payload.naluType) << 1;

        m_payload.start = (pData[pos + 1] & 0x80) >> 7;
        m_payload.end = (pData[pos + 1] & 0x40) >> 6;

        // FU_A第一片，保留nalu并添加start code
        if (m_payload.start)
        {
            // 恢复nalu
            uint8_t *data = const_cast<uint8_t *>(pData);
            data[pos] = srcNalu[0];
            data[pos + 1] = srcNalu[1];
        }
        else // 后续 FU_A 直接提取nalu后的body
        {
            pos += 2;
            startCodeLen = 0;
        }
    }

    m_payload.seq = m_header.sequence;
    if (m_payload.naluType == H265_VPS || m_payload.naluType == H265_SPS || m_payload.naluType == H265_PPS ||
        m_payload.naluType == H265_SEI)
    {
        startCodeLen = 3;
    }

    // m_payload.payloadSize = size - m_header.size() + startCodeLen - extNaluHDL;
    m_payload.payloadSize = size + startCodeLen - pos;
    m_payload.payload.reset(new uint8_t[m_payload.payloadSize], std::default_delete<uint8_t[]>());

    // add start code
    memcpy(m_payload.payload.get(), StartCode(startCodeLen), startCodeLen);
    memcpy(m_payload.payload.get() + startCodeLen, &pData[pos], m_payload.payloadSize - startCodeLen);
}

CFrameCache::CFrameCache(CRecver *pRecver) : m_pRecver(pRecver)
{
}
CFrameCache::~CFrameCache()
{
}

void CFrameCache::pushVideo(const uint8_t *pData, uint16_t size)
{
    m_sumInB += size;
    CRtpPacket pkt;
    if (m_pRecver->VideoInfo().codecID == AV_CODEC_ID_H264)
    {
        pkt.decode(pData, size);
    }
    else if (m_pRecver->VideoInfo().codecID == AV_CODEC_ID_HEVC)
    {
        pkt.decodeHevc(pData, size);
    }
    else
    {
        LOGE("unsupport codec");
    }

    LOGD("nalu Type: %#x, total in %u", pkt.NaluType(), m_sumInB);

    switch (pkt.NaluType())
    {
    case H264_NIDR:
    case H264_IDR:
    case H264_AUD:
    case H264_SEI:
    case H264_SPS:
    case H264_PPS: 
    case H265_TRAIL_N:
    case H265_IDR:
    case H265_VPS:
    case H265_SPS:
    case H265_PPS:
    case H265_SEI:
    {
        cacheFrame(E_MEDIA_VIDEO, GetFrameType(pkt.NaluType()), pkt.Header(), pkt.Payload().payload, pkt.PayloadSize());
    }
    break;
    case H264_FU_A: 
    case H265_FU_A:
    {
        pushFuAToCache1(pkt, pData, size);
    }
    break;
    case H265_STAP_A:
    case H264_STAP_A: {
        CRtpPayloadStapA pktStapA;
        if (m_pRecver->VideoInfo().codecID == AV_CODEC_ID_HEVC)
        {
            pktStapA.decodeHevc(pkt.Payload().payload.get(), pkt.PayloadSize());
        }
        else 
        {
            pktStapA.decode(pkt.Payload().payload.get(), pkt.PayloadSize());
        }
        // pktStapA.decode(pkt.Payload().payload.get(), pkt.PayloadSize());
        for (const auto &it : pktStapA.payloads)
        {
            uint8_t naluTtpe = it.naluType & 0x1f;
            cacheFrame(E_MEDIA_VIDEO, GetFrameType(naluTtpe), pkt.Header(), it.payload, it.payloadSize);
        }
    }
    break;
    default:
        LOGE("unsupport nalu %d", pkt.NaluType());
    }
}

void CFrameCache::pushAudio(const uint8_t *pData, uint16_t size)
{
    CRtpPacketAAC aacPkt(m_pRecver);
    auto frame = aacPkt.decode(pData, size);

    if (frame)
    {
        cacheFrame(std::move(frame));
    }
}

void CFrameCache::pushFuAToCache1(CRtpPacket &pkt, const uint8_t *pData, uint16_t size)
{
    uint32_t timestamp = pkt.Header().timestamp;
    {
        std::lock_guard<std::mutex> lock(m_cacheLock);
        auto it = m_pktCache.find(timestamp);
        if (it != m_pktCache.end())
        {
            it->second.payloads.push_back(pkt.Payload());

            if (pkt.Payload().start)
            {
                it->second.recvStart = true;
            }
            if (pkt.Payload().end)
            {
                it->second.recvEnd = true;
            }
            // 所有分片已经收到，将包放入帧队列
            if (it->second.CheckComplete())
            {
                it->second.Sort();
                auto sumSize = 0;
                for (const auto &buf : it->second.payloads)
                {
                    sumSize += buf.payloadSize;
                }

                auto pos = 0;
                auto buffers = std::shared_ptr<uint8_t>(new uint8_t[sumSize], std::default_delete<uint8_t[]>());
                for (const auto &buf : it->second.payloads)
                {
                    std::memcpy(buffers.get() + pos, buf.payload.get(), buf.payloadSize);
                    pos += buf.payloadSize;
                }
                LOGD("buffer size:%d", sumSize);
                cacheFrame(E_MEDIA_VIDEO, GetFrameType(pkt.Payload().naluType), pkt.Header(), buffers, sumSize);
            }
        }
        else
        {
            CRtpPlyloadFuA pktFua;
            if (pkt.Payload().start)
            {
                pktFua.recvStart = true;
            }
            else if (pkt.Payload().end)
            {
                pktFua.recvEnd = true;
            }
            pktFua.payloads.push_back(pkt.Payload());
            m_pktCache.insert(std::make_pair(timestamp, std::move(pktFua)));
        }
    }
}

void CFrameCache::cacheFrame(E_MEDIA_TYPE eMediaType, E_FRAME_TYPE eFrameType, const CRtpHeader &rtpHeader,
                             std::shared_ptr<uint8_t> data, uint32_t dataSize)
{
    auto frame = std::make_shared<Frame>();
    frame->m_pt = rtpHeader.payload_type;
    frame->m_seq = rtpHeader.sequence;
    frame->m_eMediaType = eMediaType;
    frame->SetData(data, dataSize);
    frame->m_eFrameType = eFrameType;
    frame->m_pts = rtpHeader.timestamp;
    frame->m_dts = frame->m_pts;
    
    if (eMediaType == E_MEDIA_VIDEO)
    {
        if (m_recvFristKeyFrame == true)
        {
            std::lock_guard<std::mutex> dtsLock(m_dtsLock);
            dtsgen_AddNextPTS(&m_dtsgen, frame->m_pts);
            frame->m_dts = dtsgen_GetDTS(&m_dtsgen);
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_framesLock);
            if (frame->m_eFrameType == E_FRAME_I)
            {
                std::lock_guard<std::mutex> dtsLock(m_dtsLock);
                m_recvFristKeyFrame = true;
                {
                    dtsgen_AddNextPTS(&m_dtsgen, frame->m_pts);
                    frame->m_dts = dtsgen_GetDTS(&m_dtsgen);
                    m_frameCache.insert(std::move(frame));
                }

                for (auto &it : m_frameCache2)
                {
                    dtsgen_AddNextPTS(&m_dtsgen, it->m_pts);
                    it->m_dts = dtsgen_GetDTS(&m_dtsgen);
                    m_frameCache.insert(std::move(it));
                }
            }
            else
            {
                m_frameCache2.insert(std::move(frame));
            }

            return;
        }
    }

    std::lock_guard<std::mutex> lock(m_framesLock);
    m_frameCache.insert(std::move(frame));
}

void CFrameCache::cacheFrame(std::shared_ptr<Frame> &&tFrame)
{
    std::lock_guard<std::mutex> lock(m_framesLock);
    m_frameCache.insert(std::move(tFrame));
}

std::shared_ptr<Frame> CFrameCache::PopFrame()
{
    std::lock_guard<std::mutex> lock(m_framesLock);
    if (!m_frameCache.empty())
    {
        auto frame = *m_frameCache.begin();
        m_frameCache.erase(m_frameCache.begin());
        return frame;
    }

    return nullptr;
}

CRtpRecver::CRtpRecver(const std::string urn, uint16_t localPort, uint16_t remotePort)
    : m_urn(urn), m_localPort(localPort), m_remotePort(remotePort), m_frameCache(this)
{
    CMediaServer::Inst().addEvent(localPort, [this](const TConnect &tConn) { this->on_video(tConn); });
    CMediaServer::Inst().addEvent(localPort + 2, [this](const TConnect &tConn) { this->on_aduio(tConn); });
    m_vSenders.push_back(std::make_shared<CHlsWriter>(this, urn));
    // m_vSenders.push_back(std::make_shared<CFlvSender>(this, urn));
    m_sendThread = std::async(std::launch::async, &CRtpRecver::send, this);
}

CRtpRecver::~CRtpRecver()
{
    CMediaServer::Inst().delEvent(m_localPort);
    CMediaServer::Inst().delEvent(m_localPort + 2);
}

std::string CRtpRecver::Urn()
{
    return m_urn;
}

std::vector<std::shared_ptr<CSender>> CRtpRecver::GetSenders()
{
    return m_vSenders;
}

void CRtpRecver::append(const char *buff, std::size_t size, bool isVideo)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto &it : m_vSenders)
    {
        if (it->StreamType() == E_STREAM_PS || it->StreamType() == E_STREAM_ES)
        {
            it->Send((const uint8_t *)buff, size, isVideo);
        }
    }
}

void CRtpRecver::on_video(const TConnect &tConn)
{
    struct sockaddr addr;
    char buff[1600];
    memset(buff, 0, 1600);

    int ret = 0;
    while (true)
    {
        ret = tConn.recvfrom(&addr, buff, 1600);
        if (ret <= 0)
            break;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto &sender : m_vSenders)
            {
                if (sender->StreamType() == E_STREAM_ES)
                    sender->Send((const uint8_t *)buff, ret, true);
            }
        }
        parseVideo((uint8_t*)buff, ret);
        ++m_videoFrameRecv;
    }
}

void CRtpRecver::on_aduio(const TConnect &tConn)
{
    struct sockaddr addr;
    char buff[2048];
    memset(buff, 0, 2048);

    // struct sockaddr_in *in = (struct sockaddr_in *)&addr;
    int ret = 0;
    while (true)
    {
        ret = tConn.recvfrom(&addr, buff, 2048);
        if (ret <= 0)
            break;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto &sender : m_vSenders)
            {
                if (sender->StreamType() == E_STREAM_ES)
                    sender->Send((const uint8_t *)buff, ret, false);
            }
        }

        parseAudio((uint8_t*)buff, ret);

        ++m_audioFrameRecv;
    }
}

void CRtpRecver::parseVideo(const uint8_t *buff, std::size_t size)
{
    m_frameCache.pushVideo(buff, size);
}

void CRtpRecver::parseAudio(const uint8_t *buff, std::size_t size)
{
    m_frameCache.pushAudio(buff, size);
}

void CRtpRecver::send()
{
    while (1)
    {
        auto frame = m_frameCache.PopFrame();
        if (!frame)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto &sender : m_vSenders)
        {
            if (sender->StreamType() == E_STREAM_TS || sender->StreamType() ==E_STREAM_FLV)
            {
                sender->Send(frame);
            }
        }
    }
}