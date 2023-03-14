#include "flvSender.h"
#include "Utils/Log.h"
#include "Utils/Utils.h"
#include "define.h"
#include "httplib.hpp"
#include "mediaServer.h"
#include "recver.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <string>
#include <vector>

void CFlvOnMetaData::encode(uint8_t *pData)
{
    uint16_t pos = 0;
    pData[pos++] = 0x02; // type string
    SetBE16(pData + pos, 0x0a); // sizeof "onmetadata"
    pos += 2;
    strcpy((char *)(pData + pos), "onMetaData");
    pos += 0x0a;
    pData[pos++] = 0x03; // type object

    auto fwriteData32 = [](const std::string key, double value, uint8_t *pData, uint16_t &pos) {

        union D_U64 {
            double d;
            uint64_t u64;
        };

        D_U64 u_value;
        u_value.d = value;

        SetBE16(pData + pos, key.size()); // sizeof "width"
        pos += 2;
        strncpy((char *)pData + pos, key.c_str(), key.size());
        pos += key.size();
        pData[pos++] = 0x00; // type double
        SetBE64(pData + pos, u_value.u64);
        pos += 8;
    };

    auto fwriteDataStr = [](const std::string key, std::string value, uint8_t *pData, uint16_t &pos) {
        SetBE16(pData + pos, key.size()); // sizeof "width"
        pos += 2;
        strncpy((char *)pData + pos, key.c_str(), key.size());
        pos += key.size();

        pData[pos++] = 0x02; // type string
        SetBE16(pData + pos, value.size()); // sizeof value
        pos += 2;
        strncpy((char *)pData + pos, value.c_str(), value.size());
        pos += value.size();
    };

    if (width)
    {
        fwriteData32("width", width, pData, pos);
    }

    if (height)
    {
        fwriteData32("height", height, pData, pos);
    }

    if (videodatarate)
    {
        fwriteData32("videodatarate", videodatarate, pData, pos);
    }

    if (framerate)
    {
        fwriteData32("framerate", framerate, pData, pos);
    }

    if (videocodecid)
    {
        fwriteData32("videocodecid", videocodecid, pData, pos);
    }

    if (audiosamplerate)
    {
        fwriteData32("audiosamplerate", audiosamplerate, pData, pos);
    }

    if (true)
    {
        SetBE16(pData + pos, 6); 
        pos += 2;
        strcpy((char *)pData + pos, "stereo");
        pos += 6;
        pData[pos++] = 0x01; // type bool
        pData[pos++] = stereo;
    }

    if (audiosamplesize)    
    {
        fwriteData32("audiosamplesize", audiosamplesize, pData, pos);
    }

    if (audiocodecid)
    {
        fwriteData32("audiocodecid", audiocodecid, pData, pos);
    }

    if (!major_brand.empty())
    {
        fwriteDataStr("major_brand", major_brand, pData, pos);
    }

    if (!minor_version.empty())
    {
        fwriteDataStr("minor_version", minor_version, pData, pos);
    }

    if (!compatible_brands.empty())
    {
        fwriteDataStr("compatible_brands", compatible_brands, pData, pos);
    }

    if (!description.empty())
    {
        fwriteDataStr("description", description, pData, pos);
    }

    // obj end code 00 00 09
    pData[pos++] = 0x00;
    pData[pos++] = 0x00;
    pData[pos++] = 0x09;

    // // write previous tag len
    // SetBE32(pData + pos, pos);
    // pos += 4;
    m_size = pos;
}

uint32_t CFlvOnMetaData::size()
{
    return m_size;
}

CFlvSender::CFlvSender(CRecver *pRecver, const std::string &urn, std::shared_ptr<CWriter> writer)
    : m_writer(writer)
{
#ifdef FLV_DEBUG
    m_file = fopen("test.flv", "wb");
#endif
    m_pRecver = pRecver;
}

CFlvSender::~CFlvSender()
{
#ifdef FLV_DEBUG
    if (m_file)
    {
        fclose(m_file);
    }
#endif
}

void CFlvSender::Send(const std::shared_ptr<Frame> &tFrame)
{
    if (!m_writeHeader)
    {
        createHeaderAndOnMetaData();
        writeSpsPps();
        writeAudioExtData();
        m_writeHeader = true;
    }

    m_totalIn += tFrame->BodySize();
    if (m_fristVideoDts == 0 && tFrame->m_eMediaType == E_MEDIA_VIDEO)
    {
        m_fristVideoDts = tFrame->m_dts;
    }

    if (m_fristAudioPts == 0 && tFrame->m_eMediaType == E_MEDIA_AUDIO)
    {
        m_fristAudioPts = tFrame->m_pts;
    }

    CFlvTag cTag;
    if (tFrame->m_eMediaType == E_MEDIA_VIDEO)
    {
        createVideoTag(*tFrame, cTag);
    }
    else
    {
        createAudioTag(*tFrame, cTag);
    }
    writeTag(cTag);

    LOGD("total in %u, total write %u, offset %u, datalen %u, framelen %lu", m_totalIn, m_totalWrite, cTag.dataOffset, cTag.dataLen, tFrame->BodySize());
}

void CFlvSender::Send(const uint8_t *buffer, size_t size, bool isVideo)
{
    

}

void CFlvSender::createHeaderAndOnMetaData()
{
    uint8_t header[16] = {0};
    header[0] = 'F';
    header[1] = 'L';
    header[2] = 'V';
    header[3] = 0x01;
    header[4] = 0x05;
    header[8] = 0x09;
    // previous tag len 4B
    write(&header[0], 13);

    uint8_t AMFHeader[11] = {0};
    AMFHeader[0] = 0x12; // tag type

    uint8_t metaData[512] = {0};
    CFlvOnMetaData cOnMetaData;
    cOnMetaData.audiosamplerate = m_pRecver->AudioInfo().sample_rate;
    cOnMetaData.audiosamplesize = 16; 
    cOnMetaData.stereo = m_pRecver->AudioInfo().nbChannels > 1;
    cOnMetaData.videodatarate = m_pRecver->VideoInfo().timebase.den;
    cOnMetaData.height = m_pRecver->VideoInfo().h;
    cOnMetaData.width = m_pRecver->VideoInfo().w;
    cOnMetaData.encode(metaData);
    uint32_t tagBodySize = cOnMetaData.size();
    uint8_t *p = (uint8_t*)&tagBodySize;

    AMFHeader[1] = *(p + 2);
    AMFHeader[2] = *(p + 1);
    AMFHeader[3] = *(p + 0);

    write(AMFHeader, 11);
    write(metaData, tagBodySize);
    uint8_t pervious[4];
    SetBE32(pervious, 11 + tagBodySize);
    write(pervious, 4);
}

void CFlvSender::writeSpsPps()
{
    std::vector<uint8_t> extData;
    extData.push_back(0x01);
    extData.push_back(0x64);
    extData.push_back(0x00);
    extData.push_back(0x1F);
    extData.push_back(0xFF);
    extData.push_back(0xE1);
    uint16_t spsSize = m_pRecver->VideoInfo().sps.size() - 3;
    extData.push_back(spsSize >> 8);
    extData.push_back(spsSize);
    extData.insert(extData.end(), m_pRecver->VideoInfo().sps.begin() + 3, m_pRecver->VideoInfo().sps.end());
    extData.push_back(0x01);
    uint16_t ppsSize = m_pRecver->VideoInfo().pps.size() - 3;
    extData.push_back(ppsSize >> 8);
    extData.push_back(ppsSize);
    extData.insert(extData.end(), m_pRecver->VideoInfo().pps.begin() + 3, m_pRecver->VideoInfo().pps.end());

    auto payload = std::shared_ptr<uint8_t>(new uint8_t[extData.size()], std::default_delete<uint8_t[]>());
    memcpy(payload.get(), &extData[0], extData.size());

    Frame tFrame;
    tFrame.SetData(payload, extData.size());

    tFrame.m_eFrameType = E_FRAME_OTHER;
    tFrame.m_eMediaType = E_MEDIA_VIDEO;
    tFrame.m_dts = m_fristVideoDts;
    tFrame.m_pts = m_fristVideoDts;

    CFlvTag cTag;
    createVideoTag(tFrame, cTag);
    writeTag(cTag);
}

void CFlvSender::writeAudioExtData()
{
    if (m_pRecver->AudioInfo().audioSpecificConfig.empty())
    {
        LOGE("no audio ext data");
        return;
    }

    Frame tFrame;
    auto audioExt = std::shared_ptr<uint8_t>(new uint8_t[m_pRecver->AudioInfo().audioSpecificConfig.size()],
                                             std::default_delete<uint8_t[]>());
    memcpy(audioExt.get(), &(m_pRecver->AudioInfo().audioSpecificConfig[0]),
           m_pRecver->AudioInfo().audioSpecificConfig.size());
    tFrame.SetData(audioExt, m_pRecver->AudioInfo().audioSpecificConfig.size(), 0);
    tFrame.m_eFrameType = E_FRAME_OTHER;
    tFrame.m_eMediaType = E_MEDIA_AUDIO;
    tFrame.m_pts = m_fristAudioPts;
    tFrame.m_dts = m_fristAudioPts;

    CFlvTag cTag;
    createAudioTag(tFrame, cTag);
    writeTag(cTag);
}


void CFlvSender::createVideoTag(Frame &tFrame, CFlvTag &cTag)
{
    uint8_t *tagHeader = cTag.tagHeader;

    // tag type
    tagHeader[0] = 0x09;
    uint8_t *pData = tFrame.Data().get();

    size_t startCodeSize = 0;
    if (pData[0] == 0 && pData[2] == 0)
        startCodeSize = 4;
    else if (pData[0] == 0 && pData[2] == 1)
        startCodeSize = 3;

    cTag.dataOffset = startCodeSize;

    uint32_t time = (tFrame.m_dts - m_fristVideoDts) * 1000 /
                    (m_pRecver->VideoInfo().timebase.den * m_pRecver->VideoInfo().timebase.num);

    // time ms
    uint8_t *p = (uint8_t *)&time;
    tagHeader[4] = *(p + 2);
    tagHeader[5] = *(p + 1);
    tagHeader[6] = *(p + 0);
    LOGD("time: %d", time);

    // time ext
    tagHeader[7] = 0;

    // stream id
    tagHeader[8] = 0;
    tagHeader[9] = 0;
    tagHeader[10] = 0;

    // avc body header
    // frame type
    cTag.dataAttr |= ((tFrame.m_eFrameType == E_FRAME_I || tFrame.m_eFrameType == E_FRAME_OTHER) ? 1 : 2) << 4;
    cTag.dataAttr |= 0x07; // avc

    cTag.payloadHeader.resize(4);

    cTag.payloadHeader[0] = (tFrame.m_eFrameType == E_FRAME_OTHER) ? 0 : 1;
    uint32_t cts = (tFrame.m_pts - tFrame.m_dts) * 1000 /
                   (m_pRecver->VideoInfo().timebase.den * m_pRecver->VideoInfo().timebase.num);

    p = (uint8_t *)&cts;
    cTag.payloadHeader[1] = *(p + 2);
    cTag.payloadHeader[2] = *(p + 1);
    cTag.payloadHeader[3] = *(p + 0);

    if (tFrame.m_eFrameType != E_FRAME_OTHER)
    {
        uint8_t naluLen[4] = {0};
        SetBE32(naluLen, tFrame.BodySize() - startCodeSize);
        cTag.payloadHeader.insert(cTag.payloadHeader.end(), naluLen, naluLen + 4);
    }

    // tag payload size
    uint32_t dataSize = tFrame.BodySize() - startCodeSize + 1 + cTag.payloadHeader.size();
    p = (uint8_t *)&dataSize;
    tagHeader[1] = *(p + 2);
    tagHeader[2] = *(p + 1);
    tagHeader[3] = *(p + 0);

    LOGD("cts: %d", cts);
    cTag.data = tFrame.Data();
    cTag.dataLen = tFrame.BodySize();
}

void CFlvSender::createAudioTag(Frame &tFrame, CFlvTag &cTag)
{
    uint8_t *tagHeader = cTag.tagHeader;

    // tag type
    tagHeader[0] = 0x08;

    // TODO data size
    uint32_t time = (tFrame.m_dts - m_fristAudioPts) * 1000 / (m_pRecver->AudioInfo().sample_rate);
     // time ms
    uint8_t *p = (uint8_t *)&time;
    tagHeader[4] = *(p + 2);
    tagHeader[5] = *(p + 1);
    tagHeader[6] = *(p + 0);   

    // time ext
    tagHeader[7] = 0;

    // stream id
    tagHeader[8] = 0;
    tagHeader[9] = 0;
    tagHeader[10] = 0;

    // https://blog.csdn.net/Ritchie_Lin/article/details/121730749
    // TODO soundFromat + soundRate + sounSize + soundType
    cTag.dataAttr = 0xaf;
    
    // raw data
    if (tFrame.m_eFrameType == E_FRAME_I)
    {
        // seek after adts
        cTag.dataOffset = 7;
        cTag.payloadHeader.push_back(0x01);

    }
    // aac audio specific config. aslo call ext data. like sps pps
    else
    {
        cTag.dataOffset = 0;
        cTag.payloadHeader.push_back(0x00);
    }

    // tag payload size
    uint32_t dataSize = tFrame.BodySize() - cTag.dataOffset + 1 + cTag.payloadHeader.size();
    p = (uint8_t *)&dataSize;
    tagHeader[1] = *(p + 2);
    tagHeader[2] = *(p + 1);
    tagHeader[3] = *(p + 0);
    
    cTag.dataLen = tFrame.BodySize();
    cTag.data = tFrame.Data();
}


void CFlvSender::writeTag(CFlvTag &cTag)
{
    write(cTag.tagHeader, 11);
    write(&cTag.dataAttr, 1);
    write(&cTag.payloadHeader[0], cTag.payloadHeader.size());
    write(cTag.data.get() + cTag.dataOffset, cTag.dataLen - cTag.dataOffset);

    uint8_t pervious[4];
    SetBE32(pervious, 12 + cTag.payloadHeader.size() + cTag.dataLen - cTag.dataOffset);
    write(pervious, 4);
}

void CFlvSender::release()
{
    CMediaServer::Inst().UnBindSender2Recver(m_pRecver, this);
}

void CFlvSender::write(const uint8_t *buffer, size_t size)
{
#ifdef FLV_DEBUG
    fwrite(buffer, size, 1, m_file);
    fflush(m_file);
#endif
    if (m_writer->isWriteable)
    {
        (*m_writer)(buffer, size);
    }
    else
    {
        release();
    }

    m_totalWrite += size;
}
