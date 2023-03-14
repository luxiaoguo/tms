#ifndef _DEFINE_H_
#define _DEFINE_H_

#include "Utils/Log.h"
#include <atomic>
#include <cmath>
// #include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

extern "C"
{
#include "libavcodec/codec_par.h"
}

#define bits_write(buffer, count, bits)                                                                                \
    {                                                                                                                  \
        bits_buffer_s *p_buffer = (buffer);                                                                            \
        int i_count = (count);                                                                                         \
        uint64_t i_bits = (bits);                                                                                      \
        while (i_count > 0)                                                                                            \
        {                                                                                                              \
            i_count--;                                                                                                 \
            if ((i_bits >> i_count) & 0x01)                                                                            \
            {                                                                                                          \
                p_buffer->p_data[p_buffer->i_data] |= p_buffer->i_mask;                                                \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                p_buffer->p_data[p_buffer->i_data] &= ~p_buffer->i_mask;                                               \
            }                                                                                                          \
            p_buffer->i_mask >>= 1;    /*操作完一个字节第一位后，操作第二位*/                         \
            if (p_buffer->i_mask == 0) /*循环完一个字节的8位后，重新开始下一位*/                     \
            {                                                                                                          \
                p_buffer->i_data++;                                                                                    \
                p_buffer->i_mask = 0x80;                                                                               \
            }                                                                                                          \
        }                                                                                                              \
    }

#define PS_HDR_LEN 14             // ps header 字节长度
#define SYS_HDR_LEN 18            // ps system header 字节长度
#define PSM_HDR_LEN 24            // ps system map    字节长度
#define PES_HDR_LEN 19            // ps pes header    字节长度
#define RTP_HDR_LEN 12            // rtp header       字节长度
#define RTP_VERSION 2             // rtp 版本号
#define RTP_MAX_PACKET_BUFF 1400  // rtp传输时的最大包长 body
#define PS_PES_PAYLOAD_SIZE 65522 // 分片进循发送的最大长度上限
#define H264_NALU_SLICE_HEAD 2         // 分片nalu的额外长度, 每一个rtp分片都要带上分片nalu
#define HEVC_NALU_SLICE_HEAD 3         // 分片nalu的额外长度, 每一个rtp分片都要带上分片nalu

enum EH264NaluType
{
    H264_NIDR = 1,    // 非IDR片
    H264_IDR = 5,     // IDR片
    H264_SEI = 6,     // 图像增强信息
    H264_SPS = 7,     // 序列参数集
    H264_PPS = 8,     // 图像参数集
    H264_AUD = 9,     // 分界符
    H264_STAP_A = 24, // 时间分片A
    H264_STAP_B = 25, // 时间分片B
    H264_FU_A = 28,   // 碎片头A
    H264_FU_B = 29,   // 碎片头B
};

enum EH265NaluType
{
    H265_TRAIL_N = 0,
    H265_NIDR = 1,    // 非IDR片
    H265_IDR = 19,    // IDR片
    H265_VPS = 32,    // VPS
    H265_SPS = 33,    // 序列参数集
    H265_PPS = 34,    // 图像参数集
    H265_SEI = 39,    // 图像增强信息
    H265_STAP_A = 48, // 集合头A
    H265_FU_A = 49,   // 碎片头A
};

union LESize {
    unsigned short int length;
    unsigned char byte[2];
};

struct bits_buffer_s
{
    unsigned char *p_data;
    unsigned char i_mask;
    int i_size;
    int i_data;
};

struct Data_Info_s
{
    uint64_t s64CurPts = 0;
    int IFrame = 1;
    uint16_t u16CSeq;
    uint32_t u32Ssrc = (uint32_t)1234567890123;
    ;
    char szBuff[RTP_MAX_PACKET_BUFF];
};

enum EMediaType : unsigned char
{

    /*媒体类型无效值定义*/
    MEDIA_TYPE_NULL = 255, /*媒体类型为空*/

    /*音频*/
    MEDIA_TYPE_PCMU = 0,   /*G.711 ulaw  mode 6*/
    MEDIA_TYPE_G721 = 2,   /*G.721*/
    MEDIA_TYPE_G7231 = 4,  /*G.7231*/
    MEDIA_TYPE_ADPCM = 5,  /*DVI4 ADPCM*/
    MEDIA_TYPE_PCMA = 8,   /*G.711 Alaw  mode 5*/
    MEDIA_TYPE_G722 = 9,   /*G.722*/
    MEDIA_TYPE_G7221 = 13, /*G.7221*/
    MEDIA_TYPE_MP2 = 14,   //(音频)
    MEDIA_TYPE_G728 = 15,  /*G.728*/
    MEDIA_TYPE_G729 = 18,  /*G.729*/
    MEDIA_TYPE_SVACA = 20,
    MEDIA_TYPE_MPEG2 = 95,  //(视频)
    MEDIA_TYPE_G7221C = 98, /*G722.1.C Siren14*/
    MEDIA_TYPE_MP3 = 99,    /*mp3 mode 0-4*/
    MEDIA_TYPE_H224 = 100,
    MEDIA_TYPE_AACLC = 102,
    MEDIA_TYPE_AACLD = 103, /*AAC LD*/
    MEDIA_TYPE_AACLC_PCM = 104,
    MEDIA_TYPE_AMR = 105,
    MEDIA_TYPE_G726_16 = 112,
    MEDIA_TYPE_G726_24 = 113,
    MEDIA_TYPE_G726_32 = 114,
    MEDIA_TYPE_G726_40 = 115,
    MEDIA_TYPE_OPUS = 117,
    MEDIA_TYPE_AMBE = 118,
    MEDIA_TYPE_DTMF =
        101, // 与MEDIA_TYPE_H263PLUS冲突，但MEDIA_TYPE_H263PLUS目前不用，且DTMF与音频混流，修改payload有问题

    /*视频*/
    MEDIA_TYPE_MJPEG = 26,
    MEDIA_TYPE_H261 = 31, /*H.261*/
    MEDIA_TYPE_H262 = 33, /*H.262 (MPEG-2)*/
    MEDIA_TYPE_H263 = 34, /*H.263*/

    MEDIA_TYPE_PS = 96,        /*ps流*/
    MEDIA_TYPE_MP4 = 97,       /*MPEG-4*/
    MEDIA_TYPE_H263PLUS = 101, /*H.263+*/
    MEDIA_TYPE_H264 = 106,     /*H.264*/
    MEDIA_TYPE_H265 = 111,     /*H.265*/
    MEDIA_TYPE_SVACV = 107,
    MEDIA_TYPE_SVACV2 = 119,
};

enum E_STREAM_TYPE
{
    E_STREAM_PS = 0,
    E_STREAM_ES = 1,
    E_STREAM_TS = 2,
    E_STREAM_FLV = 3
};

enum E_MEDIA_TYPE
{
    E_MEDIA_VIDEO = 0,
    E_MEDIA_AUDIO
};

enum E_FRAME_TYPE
{
    E_FRAME_P = 0,
    E_FRAME_I = 1,
    E_FRAME_B = 2,
    E_FRAME_OTHER = 3
};

inline E_FRAME_TYPE GetFrameType(uint8_t naluType)
{
    switch (naluType)
    {
    /*
    case H265_NIDR:*/
    case H265_TRAIL_N:
    case H264_NIDR: 
        return E_FRAME_P;
    case H264_IDR:
    case H265_IDR:
    case H265_SEI:
    case H264_SEI:
    case H264_AUD:
        return E_FRAME_I;
    case H265_VPS:
    case H265_SPS:
    case H265_PPS:
    case H264_SPS:
    case H264_PPS:
        return E_FRAME_OTHER;
    case H265_FU_A:
    case H264_STAP_A:
    case H264_STAP_B:
    case H264_FU_A:
    case H264_FU_B:
        return E_FRAME_OTHER;
    }
    return E_FRAME_OTHER;
}

struct TTimebase
{
    int32_t num; ///< Numerator
    int32_t den; ///< Denominator
};

struct TFps
{
    int32_t num; ///< Numerator
    int32_t den; ///< Denominator
};

struct TVideoInfo
{
    uint32_t h = 0;
    uint32_t w = 0;
    uint32_t codecID = 0;
    uint8_t streamIndex = 0;
    uint32_t durtion = 0;
    TFps fps;
    TTimebase timebase;
    std::vector<uint8_t> vps;
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
};

struct TAudioInfo
{
    uint32_t sample_rate = 0;
    uint32_t nbChannels = 0;
    uint32_t codecID = 0;
    uint32_t profile = 0;
    uint32_t freqInx = 0;
    std::vector<uint8_t> audioSpecificConfig;
};

struct TMediaInfo
{
    TVideoInfo video;    
    TAudioInfo audio;
};

struct TSdpInfo
{
    uint32_t height;
    uint32_t width;
    std::string vFtm;
    std::string aFtm;
    uint16_t vPt;
    uint16_t aPt;
    std::string vCodecName;
    std::string aCodecName;
    uint32_t vSampleRate;
    uint32_t aSampleRate;
    uint32_t aChannelNum;
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
    std::vector<uint8_t> vps;
    std::vector<uint8_t> asc; // audioSpecificConfig
};

using DataList = std::list<std::pair<uint32_t, std::shared_ptr<uint8_t>>>;

class Frame
{
  public:
    void SetData(uint8_t *pDate, size_t totalSize, size_t headerSize);
    void SetData(std::shared_ptr<uint8_t> data, size_t totalSize, size_t headerSize = 0);
    //for stap-a
    void SetData(const DataList &&data);
    size_t TotalSize()
    {
        return m_size;
    }
    size_t HeaderSize()
    {
        return m_headerSize;
    }
    size_t BodySize()
    {
        return m_size - m_headerSize;
    }
    std::shared_ptr<uint8_t> Data();
    const DataList &Datas();

    E_MEDIA_TYPE m_eMediaType;
    E_FRAME_TYPE m_eFrameType;
    uint32_t m_pts = 0;
    uint32_t m_dts = 0;
    TTimebase m_tTimeBase;
    uint32_t m_pt = 0;
    uint32_t m_ms = 0;
    uint32_t m_seq = 0;

  public:
    bool operator<(const Frame &frame)
    {
        // return this->m_seq < frame.m_seq;
        return this->m_dts < frame.m_dts;
    }

  private:
    bool hasStartCode(uint8_t *data, int len);
    void convertToVideoFrame(uint8_t *data, uint32_t len);

  private:
    std::shared_ptr<uint8_t> m_data;
    DataList m_datas;
    std::size_t m_size = 0;
    std::size_t m_headerSize = 0;
};

template <class T>
struct SharedPtrComp
{
    bool operator()(const std::shared_ptr<T> &lhs, const std::shared_ptr<T> &rhs) const
    {
        return lhs < rhs;
    }
};

struct TAudioCodecInfo
{
public:
    // The audio codec id; for FLV, it's SoundFormat.
    uint8_t codecID;
    // The audio sample rate; for FLV, it's SoundRate.
    uint32_t sample_rate;
    // The audio sample size, such as 16 bits; for FLV, it's SoundSize.
    uint8_t bit_size;
    // The audio number of channels; for FLV, it's SoundType.
    // TODO: FIXME: Rename to sound_channels.
    uint8_t channels;
    int bps; // in bps
    // In AAC specification.
public:
    /**
     * audio specified
     * audioObjectType, in 1.6.2.1 AudioSpecificConfig, page 33,
     * 1.5.1.1 Audio object type definition, page 23,
     *           in ISO_IEC_14496-3-AAC-2001.pdf.
     */
    // SrsAacObjectType aac_object;
    /**
     * samplingFrequencyIndex
     */
    uint8_t aac_sample_rate;
    /**
     * channelConfiguration
     */
    uint8_t aac_channels;

};

struct TVideoCodecInfo
{
public:
    uint8_t codecID;
    uint32_t bps; // in bps
    TFps frame_rate;
    double duration;
    uint32_t width;
    uint32_t height;
};

class CWriter
{
  public:
    CWriter(std::function<void(const uint8_t *pData, uint32_t)> func) : m_func(func)
    {
    }
    CWriter(const CWriter &in)
    {
        isWriteable.store(in.isWriteable.load());
        m_func = in.m_func;
    }
    ~CWriter()
    {
    }

    void operator() (const uint8_t* pData, uint32_t size)
    {
        m_func(pData, size);
    }

    std::atomic_bool isWriteable{true};
    private:
      std::function<void(const uint8_t *pData, uint32_t)> m_func;
};


uint8_t GetMediaType(uint32_t dwCodecId);
AVCodecID GetCodecId(uint8_t byMediaType);
uint32_t GetBitsPerSample(uint8_t byMediaType);
std::string GetCodecName(uint32_t dwCodecId);
AVCodecID GetCodecId(const std::string codecName);
uint32_t GetExtHeaderLen(uint32_t dwCodecId);

#endif