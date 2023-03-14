#include "define.h"
#include <cstdint>
#include <iostream>

void Frame::SetData(uint8_t *pDate, size_t totalSize, size_t headsize)
{
    // 给pes头和startcode留点空间
    // if (!hasStartCode(pDate, size))
    // {
    //     m_data.reset(new uint8_t[size + 19 + 4], [](uint8_t *p) { delete[] p; });
    //     convertToVideoFrame(m_data.get() + 19, size);
    //     memcpy(m_data.get() + 19 + 4, pDate, size);
    //     m_size = size + 19 + 4;
    // }
    // else
    {
        auto p = new uint8_t[totalSize];
        m_data.reset(p, [](uint8_t *p) { delete[] p; });
        memcpy(m_data.get() + headsize, pDate, totalSize - headsize);
        m_size = totalSize;
        m_headerSize = headsize;
    }
}

void Frame::SetData(std::shared_ptr<uint8_t> data, size_t totalsize, size_t headSize)
{
    m_data = data;
    m_size = totalsize;
    m_headerSize = headSize;
}

void Frame::SetData(const DataList &&data)
{
    m_datas = std::move(data);
}

bool Frame::hasStartCode(uint8_t *data, int len)
{
    if (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1)
    {
        return true;
    }

    return false;
}

void Frame::convertToVideoFrame(uint8_t *data, uint32_t len)
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

std::shared_ptr<uint8_t> Frame::Data()
{
    return m_data;
}

const DataList& Frame::Datas()
{
    return m_datas;
}

uint8_t GetMediaType(uint32_t dwCodecId)
{
    switch (dwCodecId)
    {
    case AV_CODEC_ID_H264:
        return MEDIA_TYPE_H264;
        break;
    case AV_CODEC_ID_HEVC:
        return MEDIA_TYPE_H265;
        break;
    case AV_CODEC_ID_AAC:
        return MEDIA_TYPE_AACLC;
        break;
    case AV_CODEC_ID_PCM_ALAW:
        return MEDIA_TYPE_PCMA;
        break;
    case AV_CODEC_ID_PCM_MULAW:
        return MEDIA_TYPE_PCMU;
        break;
    // case AV_CODEC_ID_SVAC:
    // 	return MEDIA_TYPE_SVACV;
    // 	break;
    // case AV_CODEC_ID_SVAC2:
    // 	return MEDIA_TYPE_SVACV2;
    // 	break;
    case AV_CODEC_ID_OPUS:
        return MEDIA_TYPE_OPUS;
        break;
    case AV_CODEC_ID_MPEG4:
        return MEDIA_TYPE_MP4;
        break;
    default:
        break;
    }
    LOGE("[GetMediaType]can't get mediatype by codecId:%u.\n", dwCodecId);
    return MEDIA_TYPE_NULL;
}

AVCodecID GetCodecId(uint8_t byMediaType)
{
    switch (byMediaType)
    {
    case MEDIA_TYPE_H264:
        return AV_CODEC_ID_H264;
        break;
    case MEDIA_TYPE_H265:
        return AV_CODEC_ID_HEVC;
        break;
    case MEDIA_TYPE_AACLC:
        return AV_CODEC_ID_AAC;
        break;
    case MEDIA_TYPE_PCMA:
        return AV_CODEC_ID_PCM_ALAW;
        break;
    case MEDIA_TYPE_PCMU:
        return AV_CODEC_ID_PCM_MULAW;
        break;
    // case MEDIA_TYPE_SVACV:
    // 	return AV_CODEC_ID_SVAC;
    // 	break;
    // case MEDIA_TYPE_SVACV2:
    // 	return AV_CODEC_ID_SVAC2;
    // 	break;
    case MEDIA_TYPE_OPUS:
        return AV_CODEC_ID_OPUS;
        break;
    case MEDIA_TYPE_MP4:
        return AV_CODEC_ID_MPEG4;
        break;
    default:
        break;
    }
    LOGE("[GetCodeId]can't get codecId by mt:%d.\n", byMediaType);
    return AV_CODEC_ID_NONE;
}

uint32_t GetBitsPerSample(uint8_t byMediaType)
{
    switch (byMediaType)
    {
    case MEDIA_TYPE_AACLC:
        return 16;
        break;
    case MEDIA_TYPE_MP4:
    case MEDIA_TYPE_H264:
    case MEDIA_TYPE_H265:
    case MEDIA_TYPE_SVACV:
    case MEDIA_TYPE_SVACV2:
        return 24;
        break;
    case MEDIA_TYPE_PCMA:
    case MEDIA_TYPE_PCMU:
        return 8;
        break;
    case MEDIA_TYPE_OPUS:
        return 32;
        break;
    default:
        break;
    }
    return 24;
}

std::string GetCodecName(uint32_t dwCodecId)
{
    switch (dwCodecId)
    {
    case AV_CODEC_ID_H264:
        return "H264";
        break;
    case AV_CODEC_ID_HEVC:
        return "H265";
        break;
    case AV_CODEC_ID_AAC:
        return "mpeg4-generic";
        break;
    case AV_CODEC_ID_PCM_ALAW:
        return "PCMA";
        break;
    case AV_CODEC_ID_PCM_MULAW:
        return "PCMU";
        break;
    default:
        return "";
    }
}

AVCodecID GetCodecId(const std::string codecName)
{
    if (codecName == "H264")
        return AV_CODEC_ID_H264;
    else if (codecName == "HEVC" || codecName == "H265")
        return AV_CODEC_ID_HEVC;
    else if (codecName == "mpeg4-generic" || codecName == "MPEG4-GENERIC")
        return AV_CODEC_ID_AAC;
    else if (codecName == "PCMA")
        return AV_CODEC_ID_PCM_ALAW;
    else if (codecName == "PCMU")
        return AV_CODEC_ID_PCM_MULAW;
    else
     return AV_CODEC_ID_NONE;
}

uint32_t GetExtHeaderLen(uint32_t dwCodecId)
{
    switch (dwCodecId)
    {
    case AV_CODEC_ID_H264:
        return RTP_HDR_LEN + H264_NALU_SLICE_HEAD;
        break;
    case AV_CODEC_ID_HEVC:
        return RTP_HDR_LEN + HEVC_NALU_SLICE_HEAD;
        break;
    case AV_CODEC_ID_AAC:
        return RTP_HDR_LEN + 4;
        break;
    case AV_CODEC_ID_PCM_ALAW:
        return RTP_HDR_LEN + 7;
        break;
    default:
        return 0;
    }
}