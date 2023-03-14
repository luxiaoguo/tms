#ifndef _TS_PACKET_H_
#define _TS_PACKET_H_

#include "sender.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#define TS_PMT_NUMBER 1
#define TS_PMT_PID 0x1000
#define TS_VIDEO_AVC_PID 0x100
#define TS_AUDIO_AAC_PID 0x101
#define TS_AUDIO_MP3_PID 0x102
#define Now                                                                                                            \
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()

// from Srs
enum TsStream
{
    // ITU-T | ISO/IEC Reserved
    TsStreamReserved = 0x00,
    TsStreamForbidden = 0xff,
    // ISO/IEC 11172 Video
    // ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream
    // ISO/IEC 11172 Audio
    TsStreamAudioMp3 = 0x03,
    // ISO/IEC 13818-3 Audio
    // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections
    // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES packets containing private data
    // ISO/IEC 13522 MHEG
    // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A DSM-CC
    // ITU-T Rec. H.222.1
    // ISO/IEC 13818-6 type A
    // ISO/IEC 13818-6 type B
    // ISO/IEC 13818-6 type C
    // ISO/IEC 13818-6 type D
    // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 auxiliary
    // ISO/IEC 13818-7 Audio with ADTS transport syntax
    TsStreamAudioAAC = 0x0f,
    // ISO/IEC 14496-2 Visual
    TsStreamVideoMpeg4 = 0x10,
    // ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3 / AMD 1
    TsStreamAudioMpeg4 = 0x11,
    // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets
    // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC14496_sections.
    // ISO/IEC 13818-6 Synchronized Download Protocol
    // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved
    // 0x15-0x7F
    TsStreamVideoH264 = 0x1b,
    // For HEVC(H.265).
    TsStreamVideoHEVC = 0x24,
    // User Private
    // 0x80-0xFF
    TsStreamAudioAC3 = 0x81,
    TsStreamAudioDTS = 0x8a,
};

class CTsPacket;

class CTsPayload
{
  public:
    CTsPayload(){}
    virtual ~CTsPayload() {}

    virtual void encode(uint8_t *pData) = 0;
    virtual void decode(uint8_t *pData, uint32_t payload_len) = 0;
    virtual uint32_t size() = 0;
};

class CPesPacket : public CTsPayload
{
  public:
    CPesPacket(CTsPacket *tsPacket);
    ~CPesPacket();

  public:
    uint32_t packet_start_code_prefix = 0x000001; // PES包起始码前缀，3byte，固定为0x000001
    uint8_t stream_id;                            // 流标识符，1byte，标识该PES包所包含的数据流类型
    uint16_t pes_packet_length;                   // PES包长度，2byte，包括PES头部和PES数据的长度
    uint8_t const2bits = 0x03;                    // 2bit '10'
    uint8_t pes_scrambling_control = 0x00;        // 加扰控制，2bit，标识PES包中的数据是否加密或乱序
    uint8_t pes_priority = 0x00;                  // 1bit，标识该PES包的重要性
    uint8_t data_alignment_indicator = 0x00;      // 1bit，标识PES包中的数据是否有对齐要求
    uint8_t pes_copyright = 0x00;                 // 1bit, 版权信息
    uint8_t pes_ori_or_copy = 0x00;               // 1bit, 1原始,0复制
    uint8_t pes_pts_dts_flags = 0x00; // 2bit，标识PES包中是否包含PTS和DTS时间戳 '10':pts '11':pts & dts '01':禁止出现
    uint8_t pes_escr_flag = 0x00;                 // 1bit，标识PES包中是否包含ESCR扩展时钟参考
    uint8_t pes_es_rate_flag = 0x00;              // 1bit，标识PES包中是否包含ES率
    uint8_t pes_dsm_trick_mode_flag = 0x00;       // 1bit，标识PES包中是否包含DSM技巧模式信息
    uint8_t pes_additional_copy_info_flag = 0x00; // 1bit，标识PES包中是否包含额外的拷贝信息
    uint8_t pes_crc_flag = 0x00;                  // 1bit，标识PES包中是否包含CRC校验码
    uint8_t pes_extension_flag = 0x00;            // 1bit，标识PES包中是否包含扩展信息
    uint8_t pes_header_data_length; // 1byte PES头部数据长度，指示PES头部中附加数据的长度
    uint64_t pes_pts;               // 5byte PTS时间戳，标识该PES包中的数据的显示时间
    uint64_t pes_dts;               // 5byte DTS时间戳，标识该PES包中的数据的解码时间
    uint32_t payload_len = 0;       // 负载长度

    void encode(uint8_t *pData);
    void decode(uint8_t *pData, uint32_t payload_len);
    uint32_t size();
    uint32_t header_size();

  private:
    CTsPacket *m_tsPacket; 
};

class CTsPayloadPATProgram
{
  public:
    uint16_t program_number; // 16bit
    uint8_t reverser;        // 3bit
    uint16_t pid;            // 13bit

  public:
    void encode(uint8_t *pData);
    void decode(uint8_t *pData, uint32_t payload_len);

    uint32_t size();
};

class CTsPayloadPMTESInfo
{
  public:
    uint8_t stream_type;          // 8bits
    uint8_t const1_value0 = 0x7;  // 3bit
    uint16_t elementary_PID;       // 13bit
    uint8_t const1_value1 = 0x0f; // 4bit
    // es_info_length             // 12bit
    std::string ES_info;

  public:
    void encode(uint8_t *pData);
    void decode(uint8_t *pData, uint32_t payload_len);

    uint32_t size();

  public:
    CTsPayloadPMTESInfo(uint8_t st, uint16_t elementary_PID) : stream_type(st), elementary_PID(elementary_PID)
    {
    }
    ~CTsPayloadPMTESInfo(){}
};

class CTsPayloadPAT :public CTsPayload
{
  public:
    CTsPayloadPAT(CTsPacket *tsPacket) : m_tsPacket(tsPacket)
    {
    }
    ~CTsPayloadPAT()
    {
      for (auto &it : pat_programs)
      {
          delete it;
      }
    }

  public:
    uint8_t table_id = 0x00; // 8bit const 0x00
    // 1bit 表示后面的表格数据采用什么语法格式。0表示采用普通语法格式，1表示采用ISO/IEC 13818-1的二进制语法格式
    uint8_t section_syntax_indicator = 1;
    uint8_t reserved_1 = 0x03;     // 3bit 0+两个保留位
    uint16_t section_length;       // 12bit 表示这个字节之后有用的字节数，包括CRC_32
    uint16_t transport_stream_id;  // 16bit 传输流的ID，区别于一个网络中其他多路复用的流
    uint8_t reserved_2 = 0x03;     // 2bit
    uint8_t version_number = 0x00; // 5bit
    // 1bit 表示发送的PAT是当前有效还是下一个有效，为1时代表当前有效
    // 当current_next_indicator值为0时，表示当前PAT表是最新版本，没有新版本可用。当值为1时，表示当前PAT表可能不是最新版本，有可能会有一个新版本的PAT表在之后的传输中出现。当新的PAT表版本出现时，接收设备会读取这个标志位来确定是否需要更新PAT表信息
    uint8_t current_next_indicator = 0x01;
    uint8_t section_number = 0x00;      // 8bit 如果PAT分段传输，那么此值每次递增1
    uint8_t last_section_number = 0x00; // 8bit  最后一个分段的号码

    std::vector<CTsPayloadPATProgram*> pat_programs;
    uint32_t CRC_32;  // crc

  public:
    void encode(uint8_t *pData);
    void decode(uint8_t *pData, uint32_t payload_len);
    uint32_t size();

  private:
    CTsPacket *m_tsPacket; 
};

class CTsPayloadPMT : public CTsPayload
{
  public:
    CTsPayloadPMT(CTsPacket *tsPacket) : m_tsPacket(tsPacket)
    {
    }
    ~CTsPayloadPMT()
    {
    }

  public:
    uint8_t table_id = 0x02;               // 8bit const 0x02
    uint8_t section_syntax_indicator = 1;  // 1bit
    uint8_t reserved_1 = 0x03;             // 3bit 0+两个保留位
    uint16_t section_length;               // 12bit 表示这个字节之后有用的字节数，包括CRC_32
    uint16_t Program_number = 1;               // 16bit //指出该节目的节目号，与PAT表对应
    uint8_t reserved_2 = 0x03;             // 2bit
    uint8_t version_number = 0;            // 5bit
    uint8_t current_next_indicator = 0x01; // 1bit
    uint8_t section_number = 0x00;         // 8bit 如果PAT分段传输，那么此值每次递增1
    uint8_t last_section_number = 0x00;    // 8bit  最后一个分段的号码
    uint8_t reserved_3 = 0x07;             // 3bit
    uint16_t PCR_PID;                      // 13bit 指示TS包的PCR值，该TS包含有PCR字段
    uint8_t reserved_4 = 0x0f;             // 4bit
    uint16_t program_info_length = 0;      // 12bit 该字段描述跟随其后对节目信息描述的字节数
    std::vector<CTsPayloadPMTESInfo> stream_info;
    uint32_t CRC32;

  public:
    void encode(uint8_t *pData);
    void decode(uint8_t *pData, uint32_t payload_len);
    uint32_t size();

  private:
    CTsPacket *m_tsPacket; 
};

// 对于pat和pmt包，直接在pat或者pmt包之后填充就行,不需要此结构进行调整
// 对于音视频包，如果不满188个字节，则需要次结构进行填充
class CTsAdaptationField
{
  public:
    uint8_t adaption_field_length;                    // 8bit 此字节之后的长度，不包含自身
    uint8_t discontinuity_indicator = 0;              // 1bit
    uint8_t random_access_indicator = 0;              // 1bit
    uint8_t elementary_stream_priority_indicator = 0; // 1bit
    uint8_t PCR_flag = 0;                             // 1bit
    uint8_t OPCR_flag = 0;                            // 1bit
    uint8_t splicing_point_flag = 0;                  // 1bit
    uint8_t transport_private_data_flag = 0;          // 1bit
    uint8_t adaptation_field_extension_flag = 0;      // 1bit

    uint64_t program_clock_reference_base;               // 33bit
    uint8_t const1_value1;                               // 6bit
    uint16_t program_clock_reference_extension = 0;          // 9bit
    uint64_t original_program_clock_refedrence_base;      // 33bit
    uint8_t const1_value2;                               // 6bit
    uint16_t original_program_clock_reference_extension; // 9bit
    // todo
    // 字段先支持这么多

    void encode(uint8_t *pData);
    void decode(uint8_t *pData, uint32_t payload_len);
    void SetStuffingNum(uint8_t num);
    uint8_t GetStuffingNum();
    uint32_t size();
    uint32_t sizeWithoutStuffing();
  private:
    uint8_t stuffingNum = 0;
};

class CTsPacket
{
  public:
    CTsPacket();
    ~CTsPacket();

  public:
    static CTsPacket* CreatePATPaket(uint16_t program_number, uint16_t pid);
    static CTsPacket* CreatePMTPaket(uint8_t vst, uint16_t vpid, uint8_t ast, uint16_t apid);
    static CTsPacket* CreatePesFirstPacket(bool isVideo, uint32_t payload_len, uint32_t pts, uint8_t continuity_counter);
    static std::shared_ptr<CTsPacket> CreatePesFollowPacket(bool isVideo, uint32_t payload_len, uint32_t pts, uint8_t continuity_counter);
    static CTsPacket* CreatePesFollowPacket2(bool isVideo, uint32_t payload_len, uint32_t pts, uint8_t continuity_counter);

  public:
    // Packet Header Fields
    uint8_t sync_byte = 0x47;                 // 8bit
    uint8_t transport_error_indicator = 0;       // 1biy
    uint8_t payload_unit_start_indicator = 1;    // 1bit 每帧的开始置为1
    uint8_t transport_priority = 0;              // 1bit
    uint16_t pid;                             // 13 bit
    uint8_t transport_scrambling_control = 0; // 2bit
    uint8_t adaptation_field_control;         // 2bit  01仅有负载 10仅有自适应区 11都有
    uint8_t continuity_counter;               // 4bit

    // std::shared_ptr<CTsPayload> payload = nullptr;
    // std::shared_ptr<CTsAdaptationField> adaptation_field = nullptr;
    CTsPayload  *payload = nullptr;
    CTsAdaptationField *adaptation_field = nullptr;

    uint16_t encode(uint8_t *pData);
    void decode(uint8_t *pData, uint32_t payload_len);
    uint32_t size();

  private:
    // 本包ts实际传输的码流字节数, pat,pmt包时忽略
    uint16_t realTransBytes = 0;
    bool adaptation_field_one_byte = false;
};



class CTsHub
{
  public:
    void OnVideo(uint8_t *data, size_t size);
    void OnAudio(uint8_t *data, size_t size);

  private:
    void demux(uint8_t *data, size_t size);
    void mux();

  private:
    std::vector<std::shared_ptr<CSender>> m_Seners;
    std::vector<std::shared_ptr<CTsPacket>> m_Pkts;
};

#endif