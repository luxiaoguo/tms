#include "hlsWriter.h"
#include "Utils/Log.h"
#include "Utils/Utils.h"
#include "define.h"
#include "recver.hpp"
#include "tsPacket.h"
#include <atomic>
#include <bits/types/FILE.h>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

/*
 * #EXTM3U
 * #EXT-X-VERSION:3
 * #EXT-X-MEDIA-SEQUENCE:0
 * #EXT-X-ALLOW-CACHE:YES
 * #EXT-X-TARGETDURATION:13
 */

CM3U8::CM3U8(const std::string &fileName, CHlsConfig *config) : m_name(fileName), m_config(config)
{
    // m_headers.push_back({"#EXTM3U",""});
    m_headers.push_back({"#EXT-X-VERSION","3"});
    m_headers.push_back({"#EXT-X-MEDIA-SEQUENCE","0"});
    m_headers.push_back({"#EXT-X-ALLOW-CACHE","YES"});
    m_headers.push_back({"#EXT-X-TARGETDURATION",std::to_string(config->m_durtion)});
}

CM3U8::~CM3U8()
{

}

void CM3U8::Append(std::string segName, double durtion)
{
    m_playList.push_back({segName, durtion});
    while (m_playList.size() > m_config->m_maxSegNum)
    {
        m_trash.push_back(m_playList.front().name);
        m_playList.pop_front();
    }
    
    Flush();
}

void CM3U8::Flush()
{
    std::ofstream of(std::string("live/" + m_name + ".m3u8"));
    if (of.is_open())
    {
        of.clear();
        of << "#EXTM3U" << std::endl;;
        for (auto &head: m_headers)
        {
            of << head.first << ":" << head.second << std::endl;
        }

        for (auto &seg : m_playList)
        {
            of << "#EXTINF:" << seg.durtion << "," << std::endl;
            of << seg.name << std::endl;
        }

        of.flush();
        of.close();
    }

}

// std::string CHlsSegment::newSegmentName()
// {
//     // std::stringstream name;
//     // name << m_name << "_" << std::setfill('0') << std::setw(4) << m_index++ << ".ts";
//     // return name.str();

//     char name[64] = {0};
//     if (!m_mkdir)
//     {
//         mkdir("live", S_IRWXU | S_IRWXG | S_IRWXO);
//         mkdir(std::string("live/" + m_name).c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
//     }
//     sprintf(name, "live/%s/%s_%04d.ts", m_name.c_str(), m_name.c_str(), m_index++);
//     return std::string(name);
// }

FILE* CHlsSegment::newSegmentName()
{
    char name[64] = {0};
    if (!m_mkdir)
    {
        mkdir("live", S_IRWXU | S_IRWXG | S_IRWXO);
        mkdir(std::string("live/" + m_name).c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    }
    sprintf(name, "live/%s/%s_%04d.ts", m_name.c_str(), m_name.c_str(), m_index++);
    if (m_file)
    {
        fflush(m_file);
        fclose(m_file);
        m_file = nullptr;
    }
    m_file = fopen(name, "wb");

    return m_file;
}

std::string CHlsSegment::curSegmentName()
{
    // std::stringstream name;
    // name << m_name << "_" << std::setfill('0') << std::setw(4) << m_index - 1 << ".ts";
    // return name.str();

    char name[64] = {0};
    // sprintf(name, "live/%s/%s_%04d.ts", m_name.c_str(), m_name.c_str(), m_index - 1);
    sprintf(name, "%s/%s_%04d.ts", m_name.c_str(), m_name.c_str(), m_index - 1);
    return std::string(name);
}

CHlsWriter::CHlsWriter(CRecver *pRecver, const std::string &filename) : m_m3u8(filename, &m_config)
{
    m_pRecver = pRecver;
    m_segment.m_name = filename;
    m_pFileVideo = m_segment.newSegmentName();
}

CHlsWriter::~CHlsWriter()
{
}

void CHlsWriter::Send1(Frame &tFrame)
{
    m_totalWrite += tFrame.BodySize();
    static int i = 0;
    if (i == 0)
    {
        write(&m_pRecver->VideoInfo().vps[0], m_pRecver->VideoInfo().vps.size());
        write(&m_pRecver->VideoInfo().sps[0], m_pRecver->VideoInfo().sps.size());
        write(&m_pRecver->VideoInfo().pps[0], m_pRecver->VideoInfo().pps.size());
        i++;
    }

    write(tFrame.Data().get(), tFrame.BodySize());
    write(nullptr, 0, true);
    LOGD("total write %u", m_totalWrite);
}


void CHlsWriter::Send(const std::shared_ptr<Frame> &tFrame)
{
    m_fileBuffer.clear();
    m_fileBuffer.reserve((tFrame->BodySize() * 1.5));

    uint64_t starttime = Now;
    LOGD("%ld time in", starttime);
    
    writePatPmt(*tFrame);

    uint32_t frameLens = tFrame->BodySize();
    // if key frame, add sps and pps on the header
    std::shared_ptr<uint8_t> data = tFrame->Data();
    if (tFrame->m_eFrameType == E_FRAME_I && tFrame->m_eMediaType == E_MEDIA_VIDEO)
    {
        writeSpsPps(tFrame->m_pts);
    }

    m_totalIn += frameLens;
    auto totalWrite = 0u;
    auto realWrite = 0u;
    uint8_t buf[256];
    memset(buf, 0, 256);

    CTsPacket* pkt = nullptr;
    bool bVideo = tFrame->m_eMediaType == E_MEDIA_VIDEO;

    pkt = CTsPacket::CreatePesFirstPacket(bVideo, frameLens, tFrame->m_pts, continuity_counter++);
    realWrite = pkt->encode(buf);
    write(buf, 188 - realWrite);
    write(data.get(), realWrite);
    totalWrite += realWrite;
    delete pkt;

    if (totalWrite >= frameLens)
    {
        return;
    }
    LOGD("%ld time after first pes", Now);

    while (1)
    {
        memset(buf, 0, 256);
        LOGD("%ld time after memset", Now);
        auto pkt = CTsPacket::CreatePesFollowPacket2(bVideo, frameLens - totalWrite, tFrame->m_pts, continuity_counter++);
        LOGD("%ld time after createPes", Now);
        realWrite = pkt->encode(buf);
        LOGD("%ld time after encode", Now);
        write(buf, 188 - realWrite);
        LOGD("%ld time after header", Now);
        // 写入payload
        write(data.get() + totalWrite, realWrite);
        LOGD("%ld time after body", Now);
        totalWrite += realWrite;
        if (totalWrite >= frameLens)
        {
            break;
        }
        delete pkt;
    }
    // write to file
    write(nullptr, 0, true);

    m_totalWrite += totalWrite;
    LOGD("total write: %d, total in %d, this frame %d send take %lu", m_totalWrite, m_totalIn, frameLens, Now - starttime);
}

void CHlsWriter::Send(const uint8_t *buffer, size_t size, bool isVideo)
{
}

void CHlsWriter::writePatPmt(Frame &tFrame)
{
    if (tFrame.m_eMediaType == E_MEDIA_VIDEO && tFrame.m_eFrameType == E_FRAME_I)
    {
        trySwitchSegment(tFrame.m_pts);
    }

    if (!m_segment.m_recvFirstFrame)
    {
        m_segment.m_recvFirstFrame = true;
        uint8_t buf[512];
        memset(buf, 0, 512);

        std::shared_ptr<CTsPacket> patPacket(CTsPacket::CreatePATPaket(TS_PMT_NUMBER, TS_PMT_PID));
        patPacket->encode(buf);
        memset(buf + patPacket->size(), 0xff, 188 - patPacket->size());
        write(buf, 188);

        TsStream videoStream = TsStreamVideoH264;
        if (m_pRecver->VideoInfo().codecID == AV_CODEC_ID_HEVC)
        {
            videoStream = TsStreamVideoHEVC;
        }
        auto pmtPacket = CTsPacket::CreatePMTPaket(videoStream, TS_VIDEO_AVC_PID, TsStreamAudioAAC, TS_AUDIO_AAC_PID);
        memset(buf, 0, 512);
        pmtPacket->encode(buf);
        memset(buf + pmtPacket->size(), 0xff, 188 - pmtPacket->size());
        write(buf, 188);
    }
}

void CHlsWriter::trySwitchSegment(uint32_t pts)
{
    if (!m_segment.m_firstPts)
    {
        m_segment.m_firstPts = pts;
    }
    else
    {
        if (pts > m_segment.m_firstPts)
        {
            uint32_t ptsDur = pts - m_segment.m_firstPts;
            uint32_t tsDur =
                m_config.m_durtion * m_pRecver->VideoInfo().timebase.den / m_pRecver->VideoInfo().timebase.num;
            if (ptsDur > tsDur)
            {
                m_segment.m_durtion = (double)ptsDur * m_pRecver->VideoInfo().timebase.num / m_pRecver->VideoInfo().timebase.den;
                LOGI("time to switch segment, durtion %f", m_segment.m_durtion);
                m_segment.m_firstPts = pts;
                m_segment.m_recvFirstFrame = false;
                if (m_pFileVideo)
                {
                    // fclose(m_pFileVideo);
                    // m_pFileVideo = fopen(m_segment.newSegmentName().c_str(), "wb");
                    m_pFileVideo = m_segment.newSegmentName();
                    LOGI("switch file %s", m_segment.curSegmentName().c_str());
                    m_m3u8.Append(m_segment.curSegmentName(), m_segment.m_durtion);
                }
            }
        }
    }
}

void CHlsWriter::writeSpsPps(uint32_t pts)
{
    std::vector<uint8_t> extraData = m_pRecver->VideoInfo().vps;
    extraData.insert(extraData.end(), m_pRecver->VideoInfo().sps.begin(), m_pRecver->VideoInfo().sps.end());
    extraData.insert(extraData.end(), m_pRecver->VideoInfo().pps.begin(), m_pRecver->VideoInfo().pps.end());

    uint8_t buf[256];
    memset(buf, 0, 256);
    auto pkt = CTsPacket::CreatePesFirstPacket(true, extraData.size(), pts, continuity_counter++);
    auto realWrite = pkt->encode(buf);

    write(buf, 188 - realWrite);
    // 写入payload
    write(&extraData[0], realWrite);
}

void CHlsWriter::write(const uint8_t *buffer, size_t size, bool flush)
{
    if (!m_pFileVideo)
    {
        LOGE("open segment fail");
        return;
    }

    if (!flush)
    {
        m_fileBuffer.insert(m_fileBuffer.end(), buffer, buffer + size);
        return;
    }

    std::lock_guard<std::mutex> lock(file_mutex);
    fwrite(&m_fileBuffer[0], m_fileBuffer.size(), 1, m_pFileVideo);
    fflush(m_pFileVideo);
    m_fileBuffer.clear();
}