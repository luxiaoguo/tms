#ifndef _FLV_SENDER_H_
#define _FLV_SENDER_H_

#include "define.h"
#include "mediaServer.h"
#include "sender.hpp"
#include <bits/types/FILE.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>


class CFlvOnMetaData
{
  public:
    double width = 0;
    double height = 0;
    uint32_t videodatarate = 0;
    uint8_t framerate = 0;
    uint8_t videocodecid = 0x07; // now avc only
    uint32_t audiosamplerate = 0;
    uint8_t stereo = 0;
    uint8_t audiosamplesize = 0;
    uint32_t audiocodecid = 0x0a; // now aac only
    std::string major_brand{"isom"};
    std::string minor_version{"512"};
    std::string compatible_brands{"isomiso2avc1mp41"};
    std::string description{"WH_TMS"};

    void encode(uint8_t *pData);
    void decode();
    uint32_t size();

  private:
    size_t m_size;
};

struct CFlvTag
{
    uint32_t size()
    {
        uint32_t sz = 11 + 1 + payloadHeader.size() + dataLen - dataOffset;
        return sz;
    }

    uint8_t tagHeader[11] = {0};
    uint8_t dataAttr = 0;
    std::vector<uint8_t> payloadHeader;
    uint8_t dataOffset = 0;
    uint32_t dataLen = 0;
    std::shared_ptr<uint8_t> data;
};

class CFlvSender : public CSender
{
  public:
    CFlvSender(CRecver *pRecver, const std::string &urn, std::shared_ptr<CWriter> writer);
    ~CFlvSender();

    void Send(const std::shared_ptr<Frame> &tFrame) override;
    void Send(const uint8_t *buffer, size_t size, bool isVideo) override;

    E_STREAM_TYPE StreamType() override
    {
        return E_STREAM_FLV;
    }

  private:
    void createHeaderAndOnMetaData();
    void writeSpsPps();
    void writeAudioExtData();
    void createVideoTag(Frame &tFrame, CFlvTag &cFlvTag);
    void createAudioTag(Frame &tFrame, CFlvTag &cFlvTag);
    void writeTag(CFlvTag &cTag);
    void write(const uint8_t *buffer, size_t size);
    void release();

  private:
    uint32_t m_totalIn = 0;
    uint32_t m_totalWrite = 0;
    uint32_t m_fristVideoDts = 0;
    uint32_t m_fristAudioPts = 0;
    bool m_writeHeader = false;
    std::shared_ptr<CWriter> m_writer;
#ifdef FLV_DEBUG
    FILE *m_file;
#endif
};

#endif