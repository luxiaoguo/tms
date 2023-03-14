#ifndef _HLS_SENDER_H_
#define _HLS_SENDER_H_

#include "recver.hpp"
#include "sender.hpp"
#include "tsPacket.h"
#include <atomic>
#include <bits/types/FILE.h>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// TODO FIX ts文件实际durtion与写到m3u8的durtion不一致

struct CHlsConfig
{
  uint32_t m_maxSegNum = 99;
  uint32_t m_durtion = 10; 
};

class CM3U8
{
  public:
    CM3U8(const std::string &name, CHlsConfig *config);
    ~CM3U8();

  void Append(std::string segName, double durtion);
  std::vector<std::string> CleanList();
  void Flush();

private:
  struct segment
  {
      std::string name;
      double durtion;
  };

  private:
    std::string m_name;
    CHlsConfig *m_config;
    std::list<std::pair<std::string, std::string>> m_headers;
    std::list<segment> m_playList;
    std::vector<std::string> m_trash;
};


class CHlsWriter;
class CHlsSegment
{
  friend CHlsWriter;
private:
  // std::string newSegmentName();
  FILE* newSegmentName();
  std::string curSegmentName();

  private:
    std::string m_name;
    uint8_t m_index = 0;
    double m_durtion = 0;
    uint32_t m_firstPts = 0;
    bool m_recvFirstFrame = false;
    bool m_mkdir = false;
    FILE* m_file = nullptr;
};

class CHlsWriter: public CSender
{
  public:
    CHlsWriter(CRecver *m_recver, const std::string &filename);
    ~CHlsWriter();

    virtual void Send(const std::shared_ptr<Frame> &tFrame) override;
    virtual void Send(const uint8_t *buffer, size_t size, bool isVideo) override;
    virtual void Send1(Frame &tFrame);
    virtual E_STREAM_TYPE StreamType() override
    {
      return E_STREAM_TS;
    };

  private:
    void writeSpsPps(uint32_t dts);
    void writePatPmt(Frame &tFrame);
    void write(const uint8_t *buffer, size_t size, bool flush = false);
    void trySwitchSegment(uint32_t pts);

  public:
    CHlsConfig m_config;

  private:
    CM3U8 m_m3u8;
    CHlsSegment m_segment;
    FILE *m_pFileVideo = nullptr;
    std::mutex file_mutex;
    uint8_t continuity_counter = 0;
    uint32_t m_totalWrite = 0;
    uint32_t m_totalIn = 0;
    std::vector<uint8_t> m_fileBuffer;
};

#endif