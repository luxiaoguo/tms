#include "mp4Reader.h"
#include "rtspSender.h"
#include "rtpSendPs.h"
#include "define.h"
#include "libavcodec/codec_id.h"
#include "libavcodec/packet.h"
#include "libavformat/avformat.h"
#include "Utils/Log.h"
#include "Utils/Utils.h"
#include "libavutil/avutil.h"
#include "libavutil/error.h"
#include "sender.hpp"

#include <algorithm>
#include <asm-generic/errno.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <strings.h>
#include <thread>
#include <future>

using std::cout;
using std::endl;

// #define READ_ALL

CFileReader::CFileReader(const std::string fileName, const std::string& host, uint32_t serverPort, uint32_t localPort, E_STREAM_TYPE eStreamType)
{
    if (eStreamType == E_STREAM_PS)
    {
        // m_rtpSend = new RtpSendPs(host.c_str(), serverPort, localPort);
    }
    else
    {
        std::shared_ptr<CSender> sender = std::make_shared<CRtspSender>(host.c_str(), serverPort, localPort, this);
        sender->SetRecver(this);
        bindSender(sender);
    }

    m_strFileName = fileName;
    m_pAV_format_context = nullptr;
    m_pSpsPps = new uint8_t[1024];
}

CFileReader::~CFileReader()
{
    if (m_sendThread.valid())
    {
        m_sendThread.get();
    }
    if (m_readThread.valid())
    {
        m_readThread.get();
    }

    // if (m_rtpSend)
    // {
    //     delete m_rtpSend;
    // }

    if (m_pAV_format_context)
    {
        avformat_free_context(m_pAV_format_context);
    }

    if(m_pSpsPps)
    {
        delete[] m_pSpsPps;
        m_pSpsPps = nullptr;
    }
}

std::string CFileReader::Urn()
{
    return m_strFileName;
}

std::vector<std::shared_ptr<CSender>> CFileReader::GetSenders()
{
    return m_vSenders;
}

bool CFileReader::Open()
{
    // m_pAV_format_context = avformat_alloc_context();

    int ret = 0;
    // m_pAV_format_context为null时，avformat_open_input会给m_pAV_format_context分配内存
    ret = avformat_open_input(&m_pAV_format_context, m_strFileName.c_str(), nullptr, nullptr);
    if (ret != 0)
    {
        LOGE("Open input fail, error: %d %s\n", ret, err2str(ret).c_str());
        return false;
    }

    if ((ret = avformat_find_stream_info(m_pAV_format_context, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    av_dump_format(m_pAV_format_context, 0, m_pAV_format_context->url, 0);

    return true;
}


/**
 * @brief mp4 h264 extradata parse
 * https://blog.csdn.net/tracydawn123/article/details/31773153
 * 5B + 3b + 5b(sps_num) + [2B(sps_size) + sps] + 1B(pps_num) + [2B(pps_size) + pps]
 * 
 * @return TMediaInfo 
 */

TMediaInfo CFileReader::GetProperty()
{
    cout << "nm stream: " << m_pAV_format_context->nb_streams << endl;
    for (uint32_t index = 0; index < m_pAV_format_context->nb_streams; ++ index)
    {
        AVStream *pStream = m_pAV_format_context->streams[index];
        if (pStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_tVideoInfo.streamIndex = index;
            m_tVideoInfo.durtion = pStream->duration;
            m_tVideoInfo.codecID = pStream->codecpar->codec_id;
            m_tVideoInfo.fps.den = pStream->avg_frame_rate.den;
            m_tVideoInfo.fps.num = pStream->avg_frame_rate.num;
            m_tVideoInfo.h = pStream->codecpar->height;
            m_tVideoInfo.w = pStream->codecpar->width;
            m_tVideoInfo.timebase.den = pStream->time_base.den;
            m_tVideoInfo.timebase.num = pStream->time_base.num;
        }

        if (pStream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            m_tAudioInfo.codecID = pStream->codecpar->codec_id;
            m_tAudioInfo.profile = pStream->codecpar->profile;
            m_tAudioInfo.nbChannels = pStream->codecpar->ch_layout.nb_channels;
            m_tAudioInfo.sample_rate = pStream->codecpar->sample_rate;
            m_tAudioInfo.audioSpecificConfig.insert(m_tAudioInfo.audioSpecificConfig.end(),
                                                    pStream->codecpar->extradata,
                                                    pStream->codecpar->extradata + pStream->codecpar->extradata_size);
            m_nFreqIdxAAC = samples2adtsFreqIdx(m_tAudioInfo.sample_rate);
        }
    }

    if (m_tVideoInfo.codecID == AV_CODEC_ID_H264)
    {
        open_bitstream_filter(m_pAV_format_context->streams[m_tVideoInfo.streamIndex], "h264_mp4toannexb");
    }
    else if (m_tVideoInfo.codecID == AV_CODEC_ID_HEVC)
    {
        open_bitstream_filter(m_pAV_format_context->streams[m_tVideoInfo.streamIndex], "hevc_mp4toannexb");
    }

    // GetSPSPPS(m_pAV_format_context->streams[m_tVideoInfo.streamIndex]->codecpar->extradata, m_pAV_format_context->streams[m_tVideoInfo.streamIndex]->codecpar->extradata_size);

    TMediaInfo mediaInfo;
    mediaInfo.audio =m_tAudioInfo;
    mediaInfo.video = m_tVideoInfo;

    return mediaInfo;
}

void CFileReader::Start()
{
    if (m_eStreamStatus == E_STREAM_INIT) 
    {
        m_readThread = std::async(std::launch::async, [this]() { this->read(); });
        m_sendThread = std::async(std::launch::async, [this]() { this->send(); });
        m_eStreamStatus = E_STREAM_SENDING;
    }
    else if (m_eStreamStatus == E_STREAM_PAUSE)
    {
        m_eStreamStatus = E_STREAM_SENDING;
    }
}

void CFileReader::Stop()
{
    m_eStreamStatus = E_STREAM_STOP;
}

void CFileReader::Pause()
{
    if (m_eStreamStatus == E_STREAM_SENDING)
    {
        m_eStreamStatus = E_STREAM_PAUSE;
    }
}

void CFileReader::append(const char *buff, std::size_t size, bool isVideo)
{
    // do nothing
}

void CFileReader::Seek(double pos)
{
    m_u64SeekPts = pos * ((double)(m_tVideoInfo.timebase.den) / m_tVideoInfo.timebase.num);
    // av_seek_frame(m_pAV_format_context, m_tVideoInfo.streamIndex, , AVSEEK_FLAG_FRAME);
    // avformat_seek_file(m_pAV_format_context, m_tVideoInfo.streamIndex, int64_t min_ts, int64_t ts, int64_t max_ts, AVSEEK_FLAG_FRAME);
}


void CFileReader::UpdateClientPort(uint16_t port)
{
    for (auto &sender: m_vSenders)
    {
        sender->UpdateRemotePort(port);
    }
}

void CFileReader::read()
{
    unsigned int tatalF = 0;
    AVPacket pkt;
    while(1)
    {
        if (m_eStreamStatus == E_STREAM_STOP)
        {
            LOGE("Stop Read \n");
            break;
        }

#ifndef READ_ALL
        if (isFrameFull() || m_eStreamStatus == E_STREAM_PAUSE)
#else
        if (m_eStreamStatus == E_STREAM_PAUSE)
#endif
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (m_u64SeekPts > 0)
        {
            clear();
            int ret = av_seek_frame(m_pAV_format_context, m_tVideoInfo.streamIndex, m_u64SeekPts, AVSEEK_FLAG_FRAME);
            if (ret < 0)
            {
                LOGE("seek frame error: %d %s, \n", ret, err2str(ret).c_str());
            }
            m_u64SeekPts = 0;
        }

        int ret = av_read_frame(m_pAV_format_context, &pkt);
        if (ret != 0)
        {
            m_bReadEOF.exchange(true);
            LOGE("read frame error: %d %s, \n", ret, err2str(ret).c_str());
            break;
        }
        ++tatalF;

        if (pkt.stream_index == m_tVideoInfo.streamIndex)
        {
            // cout << "video: " << pkt.pts << endl;;
            filter_stream(&pkt);
        }
        else
        {
            // cout << "audio: " << pkt.pts << endl;;
            insertFrame(&pkt);
        }

        av_packet_unref(&pkt);
    }
    cout << "tatal read frame: " << tatalF << endl;
}

void CFileReader::insertFrame(AVPacket *pkt)
{
    auto frame = std::make_shared<Frame>();
    if (pkt->stream_index == m_tVideoInfo.streamIndex)
    {
        frame->m_eMediaType = E_MEDIA_VIDEO;
        frame->m_eFrameType = (pkt->flags & AV_PKT_FLAG_KEY) ? E_FRAME_I : E_FRAME_P;
        if ((*m_vSenders.begin())->StreamType() == E_STREAM_PS)
        {
            frame->SetData(pkt->data, pkt->size + PES_HDR_LEN, PES_HDR_LEN);
        }
        else
        {
            // uint32_t extData = RTP_HDR_LEN + (m_tVideoInfo.codecID == AV_CODEC_ID_HEVC ? HEVC_NALU_SLICE_HEAD
            //                                                                                  : H264_NALU_SLICE_HEAD);
            uint32_t extData = GetExtHeaderLen(m_tVideoInfo.codecID);
            // 无需分片，所以无需额外分配分片nalu头
            if (pkt->size <= RTP_MAX_PACKET_BUFF)
            {
                extData = RTP_HDR_LEN;
            }

            frame->SetData(pkt->data, pkt->size + extData, extData);
        }
        frame->m_pts = pkt->pts;
        frame->m_ms = frame->m_pts * m_tVideoInfo.timebase.num * 1000 / m_tVideoInfo.timebase.den;
        frame->m_pt = GetMediaType(m_tVideoInfo.codecID);
        frame->m_tTimeBase.den = pkt->time_base.den;
        frame->m_tTimeBase.num = pkt->time_base.num;
    }
    else
    {
        frame->m_pt = GetMediaType(m_tAudioInfo.codecID);
        frame->m_eMediaType = E_MEDIA_AUDIO;
        frame->m_eFrameType = E_FRAME_I;

        if (m_tAudioInfo.codecID != AV_CODEC_ID_AAC)
        {
            // auto p = new uint8_t[pkt->size + sizeof(m_adts_hdr) + RTP_HDR_LEN]();
            auto extDataLen = GetExtHeaderLen(m_tAudioInfo.codecID);
            auto p = new uint8_t[pkt->size + extDataLen]();
            std::shared_ptr<uint8_t> data(p, [](uint8_t *p) { delete[] p; });

            int profile = 2;
            m_adts_hdr[0] = (char)0xFF; // 11111111     = syncword
            m_adts_hdr[1] = (char)0xF9; // 1111 1 00 1  = syncword MPEG-2 Layer CRC
            m_adts_hdr[2] = (char)(((profile - 1) << 6) + (m_nFreqIdxAAC << 2) + (m_tAudioInfo.nbChannels >> 2));
            m_adts_hdr[6] = (char)0xFC;
            m_adts_hdr[3] = (char)(((m_tAudioInfo.nbChannels & 3) << 6) + ((7 + pkt->size) >> 11));
            m_adts_hdr[4] = (char)(((pkt->size + 7) & 0x7FF) >> 3);
            m_adts_hdr[5] = (char)((((pkt->size + 7) & 7) << 5) + 0x1F);

            memcpy(data.get() + RTP_HDR_LEN, m_adts_hdr, 7);
            memcpy(data.get() + sizeof(m_adts_hdr) + RTP_HDR_LEN, pkt->data, pkt->size);

            frame->SetData(data, pkt->size + extDataLen, extDataLen);
            frame->m_pts = pkt->pts < 0 ? 0 : pkt->pts;
        }
        else
        {
            auto extDataLen = GetExtHeaderLen(m_tAudioInfo.codecID);
            auto p = new uint8_t[pkt->size + extDataLen]();
            std::shared_ptr<uint8_t> data(p, [](uint8_t *p) { delete[] p; });           
            memcpy(data.get() + extDataLen, pkt->data, pkt->size);

            frame->SetData(data, pkt->size + extDataLen, extDataLen);
            frame->m_pts = pkt->pts < 0 ? 0 : pkt->pts; 
            frame->m_ms = frame->m_pts * 1000 / m_tAudioInfo.sample_rate; 
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_listFrames.push_back(frame);
}

void CFileReader::insertFrame(std::shared_ptr<Frame> frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listFrames.push_back(frame);
}

void CFileReader::popFrame(std::shared_ptr<Frame> &frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_listFrames.empty())
        return;

    frame = m_listFrames.front();
    m_listFrames.pop_front();
    return;
}

bool CFileReader::isFrameFull()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_listFrames.size() > 1000;
}

void CFileReader::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listFrames.clear();
}

bool CFileReader::isFrameEmpty()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_listFrames.empty();
}

int CFileReader::open_bitstream_filter(AVStream *stream, const std::string& name)
{
    int ret = 0;
    const AVBitStreamFilter *filter = av_bsf_get_by_name(name.c_str());
    if (!filter)
    {
        ret = -1;
        LOGE("Unknow bitstream filter.\n");
    }
    if ((ret = av_bsf_alloc(filter, &m_bfsCtx) < 0))
    {
        LOGE("av_bsf_alloc failed\n");
        return ret;
    }
    if ((ret = avcodec_parameters_copy((m_bfsCtx)->par_in, stream->codecpar)) < 0)
    {
        LOGE("avcodec_parameters_copy failed, ret=%d\n", ret);
        return ret;
    }
    if ((ret = av_bsf_init(m_bfsCtx)) < 0)
    {
        LOGE("av_bsf_init failed, ret=%d\n", ret);
        return ret;
    }
    return ret;
}

int CFileReader::filter_stream(AVPacket *pkt)
{
    int ret = 0;
    ret = av_bsf_send_packet(m_bfsCtx, pkt);
    if (ret < 0)
    {
        LOGE("av_bsf_send_packet failed, ret=%d\n", ret);
        return ret;
    }

    while ((ret = av_bsf_receive_packet(m_bfsCtx, pkt) == 0))
    {
        insertFrame(pkt);
        av_packet_unref(pkt);
        if (ret < 0)
            return ret;
    }

    if (ret == AVERROR(EAGAIN))
        return 0;
    return ret;
}

int CFileReader::filter_stream2(AVPacket *pkt)
{
    uint32_t dwFrameSize = pkt->size;
    bool bNeedSps = false;

    auto frame = std::make_shared<Frame>();
    frame->m_eFrameType = (pkt->flags & AV_PKT_FLAG_KEY) ? E_FRAME_I : E_FRAME_P;
    frame->m_pts = pkt->dts;
    frame->m_eMediaType = E_MEDIA_VIDEO;

    if (frame->m_eFrameType == E_FRAME_I)
    {
        if (!HasH264SPSPPS(pkt->data, pkt->size))
        {
            dwFrameSize += m_nSpsPpsLen;
            bNeedSps = true;
        }
    }

    auto data = std::shared_ptr<uint8_t>(new uint8_t[dwFrameSize + RTP_HDR_LEN + H264_NALU_SLICE_HEAD],
                                         std::default_delete<uint8_t[]>());
    uint8_t *pData = data.get() + RTP_HDR_LEN + H264_NALU_SLICE_HEAD;

    if (bNeedSps)
    {
        memcpy(pData, m_pSpsPps, m_nSpsPpsLen);
        pData += m_nSpsPpsLen;
    }

    memcpy(pData, pkt->data, pkt->size);
    convertToVideoFrame(data.get() + RTP_HDR_LEN + H264_NALU_SLICE_HEAD, dwFrameSize);

    frame->SetData(data, dwFrameSize + RTP_HDR_LEN + H264_NALU_SLICE_HEAD, RTP_HDR_LEN + H264_NALU_SLICE_HEAD);

    insertFrame(frame);
    return 0;
}


void CFileReader::convertToVideoFrame(uint8_t *data, uint32_t len)
{
    const char start_code[4] = {0x00, 0x00, 0x00, 0x01};
    uint8_t* pData = data;
    uint32_t pos = 0;
    uint32_t nalu_len = 0;

    while(pos < len - 4 && len > 4)
    {
        nalu_len = GetBE32(pData + pos);
        memcpy(pData + pos, start_code, 4);
        pos += nalu_len + 4;
    }
}

void CFileReader::GetSPSPPS(uint8_t *pbyExtraData, uint32_t dwExtraDataSize)
{
    auto ext_data = pbyExtraData;
    auto ext_len = dwExtraDataSize;

    auto pos = 5u;

    m_nSpsPpsLen = 0;

    // sps
    auto num_sps = ext_data[pos++] & 0x1fu;
    cout << "num_sps " << num_sps << endl;
    while (num_sps > 0 && pos < ext_len)
    {
        auto sps_len = GetBE16(ext_data + pos);
        cout << "sps_len " << sps_len << endl;
        pos += 2;

        SetBE32(m_pSpsPps + m_nSpsPpsLen, sps_len);

        m_nSpsPpsLen += 4;

        memcpy(m_pSpsPps + m_nSpsPpsLen, ext_data + pos, sps_len);
        m_nSpsPpsLen += sps_len;
        pos += sps_len;

        num_sps--;
    }

    if (pos >= ext_len)
        return;

    // pps
    auto num_pps = ext_data[pos++];
    while (num_pps > 0 && pos < ext_len)
    {
        auto pps_len = GetBE16(ext_data + pos);
        pos += 2;

        SetBE32(m_pSpsPps + m_nSpsPpsLen, pps_len);
        m_nSpsPpsLen += 4;

        memcpy(m_pSpsPps + m_nSpsPpsLen, ext_data + pos, pps_len);
        m_nSpsPpsLen += pps_len;
        pos += pps_len;

        num_pps--;
    }

    cout << "Sps: " << endl;
    printfHex(m_pSpsPps, 20);
    cout << "Sps len :" << m_nSpsPpsLen << endl;
}

bool CFileReader::HasH264SPSPPS(uint8_t *data, int len)
{
    return (data[4] & 0x1f) == 0x07;
}

void CFileReader::send()
{
    while (true)
    {
        if (m_bReadEOF && isFrameEmpty())
        {
            break;
        }

        if (m_eStreamStatus == E_STREAM_STOP)
        {
            clear();
            break;
        }

        if (m_eStreamStatus == E_STREAM_PAUSE)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::shared_ptr<Frame> frame;
        popFrame(frame);
        if (!frame)
        {
            if (m_bReadEOF == true)
            {
                LOGE("Send over");
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        // m_rtpSend->Send(frame);
        (*m_vSenders.begin())->Send(frame);
        if (frame->m_eMediaType == E_MEDIA_VIDEO)
        {
            double fSleep = 1000000.0 * m_tVideoInfo.fps.den / m_tVideoInfo.fps.num;
            // 发快一点点防止卡顿
            std::this_thread::sleep_for(std::chrono::microseconds(uint64_t(fSleep) - 10000));
        }
    }
}