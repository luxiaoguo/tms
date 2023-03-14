#include <arpa/inet.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <ratio>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include "Utils/Log.h"
#include "Utils/Utils.h"
#include "cstring"

#include "define.h"
#include "recver.hpp"
#include "rtspSender.h"
#include "libavcodec/codec_id.h"

#define SteadyClockNowMico (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())

CRtspSender::CRtspSender(std::string desHost, uint16_t destPort, uint16_t localPort, CRecver *pRecver)
    : m_strDestHost(desHost), m_dwDestPort(destPort), m_dwLocalPort(localPort), m_pRecver(pRecver)
{
    m_qwSsrc = 1;
    if (pRecver)
    {
        m_vCodecID = pRecver->VideoInfo().codecID;
        m_aCodecID = pRecver->AudioInfo().codecID;
    }

    auto f_creteSocket = [](int port) {
        // 初始化 video socket
        int fd = -1;
        if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        {
            LOGE("Create socket failed\n");
            return -1;
        }

        int ret;
        // 为udp的socket绑定指定IP或指定端口 start
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        // addr.sin_addr.s_addr = inet_addr("localRtpIp");
        addr.sin_addr.s_addr = INADDR_ANY;
        socklen_t addr_len = sizeof(struct sockaddr_in);

        ret = bind(fd, (struct sockaddr *)&addr, addr_len);
        if (ret < 0)
        {
            LOGE("bind video socket fail");
            close(fd);
            return -1;
        }

        return fd;
    };

    m_videoSocket = f_creteSocket(localPort);
    m_audioSocket = f_creteSocket(localPort + 2);

    if (m_videoSocket == -1 || m_audioSocket == -1)
    {
        LOGE("create socket fail");
    }

#ifdef SAVE_FILE
    char szVideoFile[260] = {0};
    sprintf(szVideoFile, "nmstor_video.264");
    m_pFileVideo = fopen(szVideoFile, "wb");
#endif
}

CRtspSender::~CRtspSender()
{
#ifdef SAVE_FILE
    fclose(m_pFileVideo);
#endif
}

/**
 * @brief 将nalu使用Fu-a方式进行拆包 https://blog.csdn.net/m0_60259116/article/details/124273788
 * 将8bit的Nalu Header 改成 FU indication + FU header

 * FU indication
 * +---------------+
 * |0|1|2|3|4|5|6|7|
 * +-+-+-+-+-+-+-+-+
 * |F|NRI|   Type  |
 * +---------------+
 * F    坏帧为1，其他为0
 * NRI  该Nalu的重要性，取原nalu的重要性
 * Type Nalu Type28，表示切片Nalu
 * F和NRI直接取源nalu中的数据

 * FU header
 * +---------------+
 * |0|1|2|3|4|5|6|7|
 * +-+-+-+-+-+-+-+-+
 * |S|E|R|   Type  |
 * S 分片开始标志位
 * E 分片结束标志位
 * R 保留位 0
 * Type 源Nalu的Type

+---------------+
 * @param pData 
 * @param dwSize 
 * @param bIsAudio 
 */
void CRtspSender::Send(const std::shared_ptr<Frame> &tFrame)
{
    m_vCodecID = m_pRecver->VideoInfo().codecID;

    if (tFrame->m_eMediaType == E_MEDIA_VIDEO)
    {
        if (m_vCodecID == AV_CODEC_ID_H264)
        {
            sendH264(*tFrame);
        }
        else if (m_vCodecID == AV_CODEC_ID_HEVC)
        {
            sendHevc(*tFrame);
        }
        else
        {
            LOGE("unknow codecID %d",m_vCodecID);
        }
    }
    else if (tFrame->m_eMediaType == E_MEDIA_AUDIO)
    {
    #ifdef SAVE_FILE
        {
            fwrite(tFrame.Data().get() + tFrame.HeaderSize(), tFrame.BodySize(), 1, m_pFileVideo);
            fflush(m_pFileVideo);
        }
    #endif
        sendAudio(*tFrame);
    }
}

void CRtspSender::Send(const uint8_t *buff, size_t size, bool isVideo)
{
    sendRtpPkt(buff, size, isVideo);
}

void CRtspSender::SetRecverDisconnectCB(std::function<void()> cb) 
{
    m_recvDisconnectCB = cb;
}

void CRtspSender::OnRecverDisconnectCB()
{
    if (m_recvDisconnectCB)
        m_recvDisconnectCB();
}

// 帧数据先按照 AnnexB 存储类型处理
void CRtspSender::sendH264(Frame &tFrame)
{
    // pData 前面预留了rtp 头
    uint64_t qwSsrc = m_qwSsrc;
    uint16_t *pSeq = &m_u16VideoSeq;

    auto startCodeAndSPSPPSLen = 0;

    // 关键帧 发送sps pps
    if (tFrame.m_eFrameType == E_FRAME_I)
    {
        // 并且把数据移动到sps和pps后面
        uint32_t framePos = findExtData(tFrame.Data().get() + tFrame.HeaderSize(), tFrame.BodySize());
        startCodeAndSPSPPSLen = framePos;
        sendExtData(tFrame.m_pts, tFrame.m_pt);
    }
    // 非关键帧
    else
    {
        // 把 00 00 00 01 偏移掉
        startCodeAndSPSPPSLen = 4;
    }

    // 把关键帧前的sps pps startcode 去掉
    auto pData = tFrame.Data().get() + startCodeAndSPSPPSLen;
    // 不包含 rtp 头的size
    auto dwDataSize = tFrame.BodySize() - startCodeAndSPSPPSLen;

    // 无须分片 RTP_MAX_PACKET_BUFF rtp body最大长度
    if (dwDataSize <= RTP_MAX_PACKET_BUFF)
    {
        makeRtpHeader(pData, 1, (*pSeq)++, tFrame.m_pts, qwSsrc, tFrame.m_pt);
        sendRtpPkt(pData, tFrame.TotalSize() - startCodeAndSPSPPSLen, true);
    }
    else
    {
        /*
         * 当前内存里的数据结构应为
         * RTP_HDR_LEN(12字节, 为RTP头预留) + NALU_SLICE_HEAD(2字节，为分片nalu头预留) + 帧数据(nalu头+数据)
         */

        // 分片时,每一片需要使用分片的nalu头
        uint32_t pos = 0;
        // 实际需要发送的消息的长度
        uint32_t leftSize = dwDataSize;
        // 帧的实际 nalu header
        uint8_t srcNalu = pData[RTP_HDR_LEN + H264_NALU_SLICE_HEAD];
        // 偏移掉原始nalu头
        pData += 1;
        // 把原始nalu头去掉
        leftSize -= 1;

        while (leftSize > 0)
        {
            // 本次需要发送的长度
            uint32_t sendSize = leftSize > RTP_MAX_PACKET_BUFF ? RTP_MAX_PACKET_BUFF : leftSize;
            // 还剩下的消息长度
            leftSize -= sendSize;
            makeRtpHeader(pData + pos, leftSize > 0 ? 0 : 1, (*pSeq)++, tFrame.m_pts, qwSsrc, tFrame.m_pt);
            // 重新打nalu
            makeH264SliceNalu(pData + pos + RTP_HDR_LEN, srcNalu, pos == 0, leftSize == 0);
            sendRtpPkt(pData + pos, sendSize + RTP_HDR_LEN + H264_NALU_SLICE_HEAD, true);
            pos += sendSize;
        }
    }
}

void CRtspSender::sendHevc(Frame &tFrame)
{
    // pData 前面预留了rtp 头
    uint64_t qwSsrc = m_qwSsrc;
    uint16_t *pSeq = &m_u16VideoSeq;

    auto startCodeAndSPSPPSLen = 0;

    // 关键帧 发送sps pps
    if (tFrame.m_eFrameType == E_FRAME_I)
    {
        // 并且把数据移动到sps和pps后面
        uint32_t framePos = findExtData(tFrame.Data().get() + tFrame.HeaderSize(), tFrame.BodySize());
        startCodeAndSPSPPSLen = framePos;
        sendExtData(tFrame.m_pts, tFrame.m_pt);
    }
    // 非关键帧
    else
    {
        // 把 00 00 00 01 偏移掉
        startCodeAndSPSPPSLen = 4;
    }

    // 把关键帧前的sps pps startcode 去掉
    auto pData = tFrame.Data().get() + startCodeAndSPSPPSLen;
    // 不包含 rtp 头的size
    auto dwDataSize = tFrame.BodySize() - startCodeAndSPSPPSLen;

    // 无须分片 RTP_MAX_PACKET_BUFF rtp body最大长度
    if (dwDataSize <= RTP_MAX_PACKET_BUFF)
    {
        makeRtpHeader(pData, 1, (*pSeq)++, tFrame.m_pts, qwSsrc, tFrame.m_pt);
        sendRtpPkt(pData, tFrame.TotalSize() - startCodeAndSPSPPSLen, true);
    }
    else
    {
        /*
         * 当前内存里的数据结构应为
         * RTP_HDR_LEN(12字节, 为RTP头预留) + NALU_SLICE_HEAD(2字节，为分片nalu头预留) + 帧数据(nalu头+数据)
         */

        // 分片时,每一片需要使用分片的nalu头
        uint32_t pos = 0;
        // 实际需要发送的消息的长度
        uint32_t leftSize = dwDataSize;
        // 帧的实际 nalu header
        uint8_t srcNalu[2];
        srcNalu[0] = pData[RTP_HDR_LEN + HEVC_NALU_SLICE_HEAD];
        srcNalu[1] = pData[RTP_HDR_LEN + HEVC_NALU_SLICE_HEAD + 1];

        // 偏移掉原始nalu头
        pData += 2;
        // 把原始nalu头去掉
        leftSize -= 2;

        while (leftSize > 0)
        {
            // 本次需要发送的长度
            uint32_t sendSize = leftSize > RTP_MAX_PACKET_BUFF ? RTP_MAX_PACKET_BUFF : leftSize;
            // 还剩下的消息长度
            leftSize -= sendSize;
            makeRtpHeader(pData + pos, leftSize > 0 ? 0 : 1, (*pSeq)++, tFrame.m_pts, qwSsrc, tFrame.m_pt);
            // 重新打nalu
            makeHevcSliceNalu(pData + pos + RTP_HDR_LEN, srcNalu, pos == 0, leftSize == 0);
            sendRtpPkt(pData + pos, sendSize + RTP_HDR_LEN + HEVC_NALU_SLICE_HEAD, true);
            pos += sendSize;
        }
    }
}

void CRtspSender::sendAudio(Frame &tFrame)
{
    uint64_t qwSsrc = m_qwSsrc + 1000;
    uint16_t *pSeq = &m_u16AudioSeq;
    
    auto dwDataSize = tFrame.BodySize();
    auto  pData = tFrame.Data().get();

    if (dwDataSize <= RTP_MAX_PACKET_BUFF)
    {
        makeRtpHeader(pData, 1, (*pSeq)++, tFrame.m_pts, qwSsrc, tFrame.m_pt);
        if (tFrame.m_pt == 102)
        {
            makeAACHeader(pData + RTP_HDR_LEN, tFrame.BodySize());
        }
        sendRtpPkt(pData, tFrame.TotalSize(), false);
    }
    // 音频帧有分片么
    else
    {
        uint32_t pos = 0;
        uint32_t leftSize = dwDataSize;

        while (leftSize)
        {
            uint32_t sendSize = leftSize > RTP_MAX_PACKET_BUFF ? RTP_MAX_PACKET_BUFF : leftSize;
            leftSize -= sendSize;
            makeRtpHeader(pData + pos, leftSize > 0 ? 0 : 1, (*pSeq)++, tFrame.m_pts, qwSsrc, tFrame.m_pt);
            sendRtpPkt(pData + pos, sendSize + RTP_HDR_LEN, false);
            pos += sendSize;
        }
    }
}

uint32_t CRtspSender::findExtData(uint8_t *pData, uint32_t dwSize)
{
    uint32_t framePos = 0;
    auto fCpToNextStartCode = [](std::string &dest, uint8_t *src, uint32_t dwSize) -> uint32_t{
        uint32_t pos = 0;
        uint32_t startCodeLen = 0;
        while (pos + 4 < dwSize)
        {
            if (src[pos + 1] == 0 && src[pos + 2] == 0 && src[pos + 3] == 0 && src[pos + 4] == 1)
            {
                dest = std::string((char *)src, pos + 1);
                startCodeLen = 4;
                break;
            }
            else if (src[pos + 1] == 0 && src[pos + 2] == 0 && src[pos + 3] == 1)
            {
                dest = std::string((char *)src, pos + 1);
                startCodeLen = 3;
                break;
            }
            ++pos;
        }
        return startCodeLen;
    };

    uint32_t pos = 3;
    while (pos < dwSize)
    {
        if (pData[pos - 1] == 1 && pData[pos - 2] == 0 && pData[pos - 3] == 0 )
        {
            uint8_t naluType = 0;
            /* h264 */
            if (m_vCodecID == AV_CODEC_ID_H264)
            {
                naluType = pData[pos] & 0x1f;
                // sps
                if (naluType == 7)
                {
                    std::string sps;
                    uint32_t frameStartCodeLen = fCpToNextStartCode(sps, pData + pos, dwSize - pos);
                    m_tExtData.push_back(std::make_pair(naluType, sps));
                    framePos = pos + sps.size() + frameStartCodeLen;
                }
                // pps
                else if (naluType == 8)
                {
                    std::string pps;
                    uint32_t frameStartCodeLen = fCpToNextStartCode(pps, pData + pos, dwSize - pos);
                    m_tExtData.push_back(std::make_pair(naluType, pps));
                    framePos = pos + pps.size() + frameStartCodeLen;
                }
                // sei
                else if (naluType == 6)
                {
                    std::string sei;
                    uint32_t frameStartCodeLen = fCpToNextStartCode(sei, pData + pos, dwSize - pos);
                    m_tExtData.push_back(std::make_pair(naluType, sei));
                    framePos = pos + sei.size() + frameStartCodeLen;
                }
            }
            /* h265 */
            else if (m_vCodecID == AV_CODEC_ID_HEVC)
            {
                naluType = (pData[pos] & 0x7e) >> 1;
                // vps
                if (naluType == 32)
                {
                    std::string vps;
                    uint32_t frameStartCodeLen = fCpToNextStartCode(vps, pData + pos, dwSize - pos);
                    m_tExtData.push_back(std::make_pair(naluType, vps));
                    framePos = pos + vps.size() + frameStartCodeLen;
                }
                // sps
                else if (naluType == 33)
                {
                    std::string sps;
                    uint32_t frameStartCodeLen = fCpToNextStartCode(sps, pData + pos, dwSize - pos);
                    m_tExtData.push_back(std::make_pair(naluType, sps));
                    framePos = pos + sps.size() + frameStartCodeLen;
                }
                // pps
                else if (naluType == 34)
                {
                    std::string pps;
                    uint32_t frameStartCodeLen = fCpToNextStartCode(pps, pData + pos, dwSize - pos);
                    m_tExtData.push_back(std::make_pair(naluType, pps));
                    framePos = pos + pps.size() + frameStartCodeLen;
                }
                // sei
                else if (naluType == 39 || naluType == 40)
                {
                    std::string sei;
                    uint32_t frameStartCodeLen = fCpToNextStartCode(sei, pData + pos, dwSize - pos);
                    m_tExtData.push_back(std::make_pair(naluType, sei));
                    framePos = pos + sei.size() + frameStartCodeLen;
                }
            }
        }
        ++pos;
    }

    return framePos;
}

/**
 * @brief 创建rtp头
 * 
 * @param pData 
 * @param marker_flag 分包的最后一包，marker_flag = 1
 * @param cseq 
 * @param curpts 
 * @param ssrc 
 * @return int 
 */
int CRtspSender::makeRtpHeader(uint8_t *pData, int marker_flag, unsigned short cseq, long long curpts, unsigned int ssrc, uint16_t pt)
{
    bits_buffer_s bitsBuffer;
    if (pData == NULL)
        return -1;
    bitsBuffer.i_size = RTP_HDR_LEN;
    bitsBuffer.i_data = 0;
    bitsBuffer.i_mask = 0x80;
    bitsBuffer.p_data = (unsigned char *)(pData);
    memset(bitsBuffer.p_data, 0, RTP_HDR_LEN);
    bits_write(&bitsBuffer, 2, RTP_VERSION);   /* rtp version  */
    bits_write(&bitsBuffer, 1, 0);             /* rtp padding  */
    bits_write(&bitsBuffer, 1, 0);             /* rtp extension  */
    bits_write(&bitsBuffer, 4, 0);             /* rtp CSRC count */
    bits_write(&bitsBuffer, 1, (marker_flag)); /* rtp marker   */
    bits_write(&bitsBuffer, 7, pt);            /* rtp payload type*/
    bits_write(&bitsBuffer, 16, (cseq));       /* rtp sequence    */
    bits_write(&bitsBuffer, 32, (curpts));     /* rtp timestamp   */
    bits_write(&bitsBuffer, 32, (ssrc));       /* rtp SSRC    */
    return 0;
}
int CRtspSender::makeAACHeader(uint8_t *pData, int framesize)
{
    pData[0] = 0x00;
    pData[1] = 0x10;
    pData[2] = (framesize & 0x1fe0) >> 5;
    pData[3] = (framesize & 0x1f) << 3;

    return 0;
}

bool CRtspSender::makeH264SliceNalu(uint8_t *pData, uint8_t srcNalu, bool bStart, bool bEnd)
{
    pData[0] = (srcNalu & 0xe0) | 0x1c;
    pData[1] = (srcNalu & 0x1f) | bStart << 7 | bEnd << 6;
    
    return true;
}

bool CRtspSender::makeHevcSliceNalu(uint8_t *pData, uint8_t *srcNalu, bool bStart, bool bEnd)
{
    pData[0] = 0x31 << 1 | (srcNalu[0] & 0x1);
    pData[1] = srcNalu[1];
    pData[2] = bStart << 7 | bEnd << 6 | (srcNalu[0] & 0x7e) >> 1;

    return true;
}

int CRtspSender::sendRtpPkt(const uint8_t *pData, uint32_t dwSize, bool bIsVideo)
{
    /* 设置address */
    struct sockaddr_in addr_serv;
    int len;
    memset(
        &addr_serv, 0,
        sizeof(addr_serv)); // memset 在一段内存块中填充某个给定的值，它是对较大的结构体或数组进行清零操作的一种最快方法
    addr_serv.sin_family = AF_INET;
    addr_serv.sin_addr.s_addr = inet_addr(m_strDestHost.c_str());
    int port = bIsVideo ? m_dwDestPort : m_dwDestPort + 2;
    addr_serv.sin_port = htons(port);
    len = sizeof(addr_serv);

    int fd = bIsVideo ? m_videoSocket : m_audioSocket;
    int ret = sendto(fd, pData, dwSize, 0, (struct sockaddr *)&addr_serv,
                     len); // send函数专用于TCP链接，sendto函数专用与UDP连接。
    if (ret != 0)
    {
        ++ m_sendFrames;
        if (!(m_sendFrames % 200))
        {
            LOGD("udp sendto ret=%d, port:%d, fd:%d, addr: %s", ret, port, fd, m_strDestHost.c_str());
        }
    }
    return ret;
}

void CRtspSender::sendExtData(uint64_t qwPts, uint16_t pt)
{
    uint8_t buff[1024] = {0};

    for (const auto &it : m_tExtData)
    {
        memset(buff, 0, 1024);
        makeRtpHeader(buff, 0, m_u16VideoSeq++, qwPts, m_qwSsrc, pt);
        memcpy(buff + RTP_HDR_LEN, it.second.c_str(), it.second.size());
        sendRtpPkt(buff, RTP_HDR_LEN + it.second.size(), true);
    }
    m_tExtData.clear();
}

uint32_t CRtspSender::findFramePos(uint8_t *pData, uint32_t dwSize)
{
    uint32_t pos = 3;

    while (pos < dwSize)
    {
        if (pData[pos - 1] == 1 && pData[pos - 2] == 0 && pData[pos - 3] == 0)
        {
            uint8_t naluType;
            if (m_vCodecID == AV_CODEC_ID_H264)
            {
                naluType = pData[pos] & 0x1f;
                if (naluType >= 1 && naluType <= 5)
                {
                    return pos;
                }
            }
            else if (m_vCodecID == AV_CODEC_ID_HEVC)
            {
                naluType = (pData[pos] & 0x7e) >> 1;
                if ((naluType >= 19 && naluType <= 20) || (naluType >= 0 && naluType <= 9) ||
                    (naluType >= 16 && naluType <= 18) || naluType == 21)
                {
                    return pos;
                }
            }
            else
            {
                return 0;
            }
        }
        ++pos;
    }

    return 0;
}
