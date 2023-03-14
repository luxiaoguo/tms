#ifndef _MP4_READER_H_
#define _MP4_READER_H_

#include "recver.hpp"
#include "rtspSender.h"
#include "rtpSendPs.h"
#include "define.h"
#include "sender.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <string>

extern "C"
{
#include "libavcodec/packet.h"
#include "libavformat/avformat.h"
#include "libavcodec/bsf.h"
#include "libavutil/error.h"
}

class CFileReader:public CRecver
{

  public:
    CFileReader(const std::string fileName, const std::string &host, uint32_t serverPort, uint32_t localPort, E_STREAM_TYPE eStreamType);
    ~CFileReader();

  public:
    enum E_STREAM_STATUS
    {
        E_STREAM_INIT,
        E_STREAM_SENDING,
        E_STREAM_PAUSE,
        E_STREAM_STOP
    };

  public:
    std::string Urn() override;
    std::vector<std::shared_ptr<CSender>> GetSenders() override;

    bool Open();
    void Start();
    void Stop();
    void Pause();
    void Seek(double pos);
    void UpdateClientPort(uint16_t port);
    TMediaInfo GetProperty();

  private:
    void append(const char *buff, std::size_t size, bool isVideo) override;
private:
    void read();
    void send();
    void insertFrame(AVPacket *pAVPacket);
    void insertFrame(std::shared_ptr<Frame> frame);
    void popFrame(std::shared_ptr<Frame>&);
    bool isFrameFull();
    bool isFrameEmpty();
    void clear();
    int open_bitstream_filter(AVStream *stream, const std::string& name);

    /**
     * @brief  mp4的帧是以4字节(nalu长度)+nalu的形式保存的，没有startcode
     *         使用ffmpeg filter把mp4文件里的帧转换成annexb形式
     */
    int filter_stream(AVPacket *pkt);
    /**
     * @brief 自行把mp4文件里的帧转换成annexb形式
     * 
     */
    int filter_stream2(AVPacket *pkt);
    void convertToVideoFrame(uint8_t *data, uint32_t len);
    void GetSPSPPS(uint8_t *pbyExtraData, uint32_t dwExtraDataSize);
    bool HasH264SPSPPS(uint8_t *data, int len);

    inline std::string err2str(int code)
    {
        char str[512] = {0};
        av_make_error_string(str, 512, code);
        return std::string(str);
    }

  private:
    std::string m_strFileName;
    std::future<void> m_sendThread;
    std::future<void> m_readThread;

    AVFormatContext *m_pAV_format_context = nullptr;
    int m_nSpsPpsLen = 0;
    uint8_t *m_pSpsPps;
    // ffmpeg filter 
    AVBSFContext *m_bfsCtx = nullptr;

    // adts head
    char m_adts_hdr[7];
    uint32_t m_nFreqIdxAAC;

    // TMediaInfo m_tMediaInfo;

    /*tmp cache 400 frames*/
    std::list<std::shared_ptr<Frame>> m_listFrames;
    std::mutex m_mutex;
    std::atomic_bool m_bReadEOF{false};
    // std::atomic_bool m_bStop{false};
    // CSender *m_rtpSend;
    std::atomic<E_STREAM_STATUS> m_eStreamStatus{E_STREAM_INIT};

    // for seek
    uint64_t m_u64SeekPts = 0;
};

#endif