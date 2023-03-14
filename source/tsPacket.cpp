#include "tsPacket.h"
#include "Utils/Log.h"
#include "Utils/Utils.h"
#include "httplib.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <type_traits>
#include <zlib.h>

CPesPacket::CPesPacket(CTsPacket *tsPacket) : m_tsPacket(tsPacket)
{
}

CPesPacket::~CPesPacket()
{
}

void CPesPacket::encode(uint8_t *pData)
{
    uint16_t pos = 0;

    // 先写入一个 0, 有填充字段，或者音频流时不需要额外写0
    if (m_tsPacket->payload_unit_start_indicator && !m_tsPacket->adaptation_field && stream_id == 0xe0)
    {
        pData[pos++] = 0x00;
    }

    // 3byte start code
    pData[pos++] = 0;
    pData[pos++] = 0;
    pData[pos++] = 1;

    // 1byte v:e0 a:c0
    pData[pos++] = stream_id;

    pes_header_data_length = 0;        
    if (pes_pts_dts_flags == 0x02)
        pes_header_data_length += 5;
    if (pes_pts_dts_flags == 0x03)
        pes_header_data_length += 5;

    // 2 byte
    pes_packet_length = 3 + pes_header_data_length  + payload_len;
    // SetBE16(&pData[pos], pes_packet_length);
    SetBE16(&pData[pos], 0);
    pos += 2;

    // 1byte
    pData[pos] |= 0x02 << 6; // const '10'
    pData[pos] |= pes_scrambling_control << 4;
    pData[pos] |= pes_priority << 3;
    pData[pos] |= data_alignment_indicator << 2;
    pData[pos] |= pes_copyright << 1;
    pData[pos++] = pes_ori_or_copy;

    // 1byte
    pData[pos] |= pes_pts_dts_flags << 6;
    pData[pos] |= pes_escr_flag << 5;
    pData[pos] |= pes_es_rate_flag << 4;
    pData[pos] |= pes_dsm_trick_mode_flag << 3;
    pData[pos] |= pes_crc_flag << 2;
    pData[pos++] |= pes_extension_flag << 1;

    // 1byte
    pData[pos++] = pes_header_data_length;

    // 5 byte
    if (pes_pts_dts_flags == 0x2)
    {
        // 若有pts，则按以下规则组成，占用5byte
        // [0011] pts[32-30] [1] pts[29-22] pts[21-15] [1] pts[14-7] pts[6-0] [1]
        pData[pos] |= 0x03 << 4;
        pData[pos] |= (pes_pts >> 30) << 1;
        pData[pos++] |= 1;
        pData[pos++] |= pes_pts >> 22 & 0xff;
        pData[pos] |= (pes_pts >> 15 & 0x7f) << 1;
        pData[pos++] |= 1;
        pData[pos++] |= pes_pts >> 7 & 0xff;
        pData[pos] |= (pes_pts & 0x7f) << 1;
        pData[pos++] |= 1;
    }

    // 5 byte
    if (pes_pts_dts_flags == 0x03)
    {
        // 若有dts，则按以下规则组成，占用5byte
        // [0001] dts[32-30] [1] pts[29-15] [1] pts[14-0] [1]
        pData[pos] |= 0x01 << 4;
        pData[pos] |= (pes_dts >> 30) << 1;
        pData[pos++] |= 1;
        pData[pos++] |= pes_dts >> 22 & 0xff;
        pData[pos] |= pes_dts >> 15 & 0x7f << 1;
        pData[pos++] |= 1;
        pData[pos++] |= pes_dts >> 7 & 0xff;
        pData[pos] |= pes_dts & 0x7f << 1;
        pData[pos++] |= 1;
    }
}

void CPesPacket::decode(uint8_t *pData, uint32_t payload_len)
{
    // todo
}

uint32_t CPesPacket::header_size()
{
    uint32_t sz = 9;
    if (pes_pts_dts_flags == 0x02)
        sz += 5;
    if (pes_pts_dts_flags == 0x03)
        sz += 5;

    if (m_tsPacket->payload_unit_start_indicator && !m_tsPacket->adaptation_field && stream_id == 0xe0)
        sz += 1;

    return sz;
}

uint32_t CPesPacket::size()
{
    pes_header_data_length = payload_len;        
    if (pes_pts_dts_flags == 0x02)
        pes_header_data_length += 5;
    if (pes_pts_dts_flags == 0x03)
        pes_header_data_length += 5;

    uint32_t sz = 9 + pes_header_data_length;
    if (m_tsPacket->payload_unit_start_indicator && !m_tsPacket->adaptation_field && stream_id == 0xe0)
        sz += 1;

    return sz;
}

void CTsPayloadPATProgram::encode(uint8_t *pData)
{
    uint32_t pos = 0;
    SetBE16(pData, program_number);
    pos += 2;

    // 3bit reverser
    pData[pos] |= 0xe0;
    // 13bit pid
    pData[pos++] |= (pid >> 8) & 0x1f;
    pData[pos] = pid;
}

void CTsPayloadPATProgram::decode(uint8_t *pData, uint32_t payload_len)
{
    // todo
}

uint32_t CTsPayloadPATProgram::size()
{
    return 4;
}

void CTsPayloadPMTESInfo::encode(uint8_t *pData)
{
    uint16_t pos = 0;
    uint32_t ES_info_length = ES_info.size();

    pData[pos++] = stream_type;
    pData[pos] |= const1_value0 << 5;
    pData[pos++] |= elementary_PID >> 8;
    pData[pos++] = elementary_PID;
    pData[pos] |= const1_value1 << 4;

    pData[pos++] |= ES_info_length >> 8;
    pData[pos++] = ES_info_length;

    if (ES_info_length)
    {
        strncpy((char *)&pData[pos], ES_info.c_str(), ES_info_length);
    }
}

void CTsPayloadPMTESInfo::decode(uint8_t *pData, uint32_t payload_len)
{

}

uint32_t CTsPayloadPMTESInfo::size()
{
    return 5 + ES_info.size();
}

void CTsPayloadPAT::encode(uint8_t *pData)
{
    uint32_t pos = 0;
    section_length = 9 + pat_programs.size() * 4;

    // 先写入一个 0
    if (m_tsPacket->payload_unit_start_indicator)
    {
        pData[pos++] = 0x00;
    }

    // 1byte
    pData[pos++] = table_id;

    // 2byte
    pData[pos] = section_syntax_indicator << 7;
    pData[pos] |= reserved_1 << 4;

    pData[pos++] |= (section_length >> 8) & 0x0f;
    pData[pos++] = section_length;

    // 2byte
    pData[pos++] = transport_stream_id >> 8;
    pData[pos++] = transport_stream_id;

    // 1byte
    pData[pos] |= reserved_2 << 6;
    pData[pos] |= version_number << 1;
    pData[pos++] |= current_next_indicator;

    // 2byte
    pData[pos++] = section_number;
    pData[pos++] = last_section_number;

    for (const auto &it : pat_programs)
    {
        it->encode(&pData[pos]);
        pos += it->size();
    }

    CRC_32 = crc32(pData, pos);
    if (m_tsPacket->payload_unit_start_indicator)
    {
        CRC_32 = crc32(pData + 1, 12);
    }
    // 4byte
    uint8_t aCrc[4] = {0x2A, 0xB1, 0x04, 0xB2};
    memcpy(pData + pos, aCrc, 4);
    // SetBE32(&pData[pos], CRC_32);
}

void CTsPayloadPAT::decode(uint8_t *pData, uint32_t payload_len)
{
    // todo
}

uint32_t CTsPayloadPAT::size()
{
    uint32_t size = 12;
    if (m_tsPacket->payload_unit_start_indicator)
        size += 1;

    for (const auto &it : pat_programs)
    {
        size += it->size();
    }

    return size;
}

void CTsPayloadPMT::encode(uint8_t *pData)
{
    uint8_t pos = 0;

    /* @todo set section_length */
    section_length = 13;
    for (auto &it : stream_info)
    {
        section_length += it.size();
    }

    // 先写入一个 0
    if (m_tsPacket->payload_unit_start_indicator)
    {
        pData[pos++] = 0x00;
    }

    // 1byte
    pData[pos++] = table_id;

    // 2byte
    pData[pos] |= section_syntax_indicator << 7;
    pData[pos] |= reserved_1 << 4;
    pData[pos++] |= section_length >> 8;
    pData[pos++] = section_length;

    // 2byte
    pData[pos++] = Program_number >> 8;
    pData[pos++] = Program_number;

    // 1byte
    pData[pos] |= reserved_2 << 6;
    pData[pos] |= version_number << 1;
    pData[pos++] |= current_next_indicator;

    // 2byte
    pData[pos++] = section_number;
    pData[pos++] = last_section_number;

    // 2byte
    pData[pos] = reserved_3 << 5;
    pData[pos++] |= PCR_PID >> 8;
    pData[pos++] |= PCR_PID;

    // 2byte
    pData[pos] = reserved_4 << 4;
    pData[pos++] |= program_info_length >> 8;
    pData[pos++] = program_info_length;

    for (auto &it : stream_info)
    {
        it.encode(&pData[pos]);
        pos += it.size();
    }

    uint8_t aCrc[4] = {0x15, 0xBD, 0x4D, 0x56};
    memcpy(pData + pos, aCrc, 4);
    // CRC32 =  crc32(pData, pos);
    // SetBE32(&pData[pos], CRC32);
}

void CTsPayloadPMT::decode(uint8_t *pData, uint32_t payload_len)
{

}

uint32_t CTsPayloadPMT::size()
{
    uint32_t sz = 3 + section_length;
    if (m_tsPacket->payload_unit_start_indicator)
    {
        sz += 1;
    }

    return sz;
}

void CTsAdaptationField::encode(uint8_t *pData)
{
    adaption_field_length = 1 + stuffingNum;
    if (PCR_flag)
        adaption_field_length += 6;

    // 此长度需要计算,本结构负责将ts包填满188
    uint32_t pos = 0;
    pData[pos++] = adaption_field_length;
    pData[pos] |= discontinuity_indicator << 7;
    pData[pos] |= random_access_indicator << 6;
    pData[pos] |= elementary_stream_priority_indicator << 5;
    pData[pos] |= PCR_flag << 4;
    pData[pos] |= OPCR_flag << 3;
    pData[pos] |= splicing_point_flag << 2;
    pData[pos] |= transport_private_data_flag << 1;
    pData[pos++] |= adaptation_field_extension_flag << 1;

    if (PCR_flag)
    {
        char *pp = NULL;
        int64_t pcrv = program_clock_reference_extension & 0x1ff;
        pcrv |= (const1_value1 << 9) & 0x7E00;
        pcrv |= (program_clock_reference_base << 15) & 0xFFFFFFFF8000LL;

        pp = (char*)&pcrv;
        pData[pos++] = pp[5];
        pData[pos++] = pp[4];
        pData[pos++] = pp[3];
        pData[pos++] = pp[2];
        pData[pos++] = pp[1];
        pData[pos++] = pp[0];
    }

    if (stuffingNum)
    {
        memset(&pData[pos], 0xff, stuffingNum);
    }
}

void CTsAdaptationField::decode(uint8_t *pData, uint32_t payload_len)
{
    // todo
}

void CTsAdaptationField::SetStuffingNum(uint8_t num)
{
    stuffingNum = num;
}

uint8_t CTsAdaptationField::GetStuffingNum()
{
    return stuffingNum;
}

uint32_t CTsAdaptationField::size()
{
    adaption_field_length = 2 + stuffingNum;
    if (PCR_flag)
        adaption_field_length += 6;

    return adaption_field_length;
}

uint32_t CTsAdaptationField::sizeWithoutStuffing()
{
    uint32_t size = 2;
    if (PCR_flag)
        size += 6;

    return size;
}

CTsPacket::CTsPacket()
{
}
CTsPacket::~CTsPacket()
{
    if (payload)
    {
        delete payload;
    }

    if (adaptation_field)
    {
        delete adaptation_field;
    }
}

CTsPacket* CTsPacket::CreatePATPaket(uint16_t program_number, uint16_t pid)
{
    auto pTsPacket = new CTsPacket();
    pTsPacket->payload_unit_start_indicator = 1;
    pTsPacket->pid = 0x00;
    pTsPacket->adaptation_field_control = 0x01;
    pTsPacket->continuity_counter = 0x00;

    // new 
    pTsPacket->payload = new CTsPayloadPAT(pTsPacket);
    auto payload = static_cast<CTsPayloadPAT *>(pTsPacket->payload);
    payload->transport_stream_id = 1;

    auto pat_program = new CTsPayloadPATProgram;
    pat_program->program_number = program_number;
    pat_program->pid = pid;

    payload->pat_programs.push_back(pat_program);
    return pTsPacket;
}

CTsPacket* CTsPacket::CreatePMTPaket(uint8_t vst, uint16_t vpid, uint8_t ast, uint16_t apid)
{
    auto pTsPacket = new CTsPacket();
    pTsPacket->payload_unit_start_indicator = 1;
    pTsPacket->pid = TS_PMT_PID;
    pTsPacket->adaptation_field_control = 0x01;
    pTsPacket->continuity_counter = 0x00;

    auto pmtPacket = new CTsPayloadPMT(pTsPacket);
    pTsPacket->payload = pmtPacket;
    pmtPacket->PCR_PID = vpid;
    pmtPacket->stream_info.push_back(CTsPayloadPMTESInfo(ast, apid));
    pmtPacket->stream_info.push_back(CTsPayloadPMTESInfo(vst, vpid));

    return pTsPacket;
}

// payload_len 实际要传送的消息长度
CTsPacket* CTsPacket::CreatePesFirstPacket(bool isVideo, uint32_t payload_len, uint32_t pts, uint8_t continuity_counter)
{
    auto pTsPacket = new CTsPacket();
    pTsPacket->payload_unit_start_indicator = 1;
    pTsPacket->pid = isVideo ? TS_VIDEO_AVC_PID : TS_AUDIO_AAC_PID;
    pTsPacket->adaptation_field_control = 0x03;
    pTsPacket->continuity_counter = continuity_counter;

    auto pesPkt = new CPesPacket(pTsPacket);
    pTsPacket->payload = pesPkt;
    pesPkt->stream_id = isVideo ? 0xe0 : 0xc0;
    pesPkt->pes_pts_dts_flags = 0x02;
    pesPkt->pes_pts = pts;
    pesPkt->pes_crc_flag = 0x00;

    uint16_t byteCanTrans = 0;
    auto af = new CTsAdaptationField;;
    pTsPacket->adaptation_field = af;

    if (isVideo)
    {
        af->PCR_flag = 0x01;
        af->program_clock_reference_base = pts;
    }
    else
    {
        af->PCR_flag = 0;
        af->random_access_indicator = 0x01;
    }

    // pes_body   = ts_len - ts_header - af_header - af_pcr - pes_header
    byteCanTrans = 188 - 4 - af->sizeWithoutStuffing() - pesPkt->header_size();
    uint8_t stuffNum = 0;
    // 要传输的负载比实际能传输的负载小, 则需要填充
    if (payload_len < byteCanTrans)
    {
        pTsPacket->realTransBytes = payload_len;
        pesPkt->payload_len = payload_len;

        if (isVideo)
        {
            stuffNum = byteCanTrans - payload_len;
            af->SetStuffingNum(stuffNum);
        }
        else
        {
            if (byteCanTrans - payload_len == 1)
            {
                pTsPacket->adaptation_field_one_byte = true;
            }
            else
            {
                //  -2 减掉CTsAdaptationField的头
                stuffNum = byteCanTrans - payload_len - 2;
                af->SetStuffingNum(stuffNum);
                pTsPacket->adaptation_field_control = 0x03;
            }
        }
    }
    else
    {
        pesPkt->payload_len = payload_len;
        pTsPacket->realTransBytes = byteCanTrans;
    }


    return pTsPacket;
}

std::shared_ptr<CTsPacket> CTsPacket::CreatePesFollowPacket(bool isVideo, uint32_t payload_len, uint32_t pts, uint8_t continuity_counter)
{
    LOGD("%ld time in pes", Now);
    auto pTsPacket = std::make_shared<CTsPacket>();
    LOGD("%ld time after make", Now);
    pTsPacket->payload_unit_start_indicator = 0;
    pTsPacket->pid = isVideo ? TS_VIDEO_AVC_PID : TS_AUDIO_AAC_PID;
    pTsPacket->adaptation_field_control = 0x01;
    pTsPacket->continuity_counter = continuity_counter;

    uint16_t byteCanTrans = 0;
    uint8_t stuffNum = 0;

    byteCanTrans = 188 - 4; /* - pesPkt->header_size();*/
    CTsAdaptationField* af = nullptr;
    // 要传输的负载比实际能传输的负载小, 则需要填充
    if (payload_len < byteCanTrans)
    {
        pTsPacket->realTransBytes = payload_len;
        if (byteCanTrans - payload_len == 1)
        {
            pTsPacket->adaptation_field_one_byte = true;
        }
        else
        {
            //  -2 减掉CTsAdaptationField的头
            stuffNum = byteCanTrans - payload_len - 2;
            af = new CTsAdaptationField();
            af->SetStuffingNum(stuffNum);
            pTsPacket->adaptation_field_control = 0x03;
        }
    }
    else
    {
        // pesPkt->payload_len = byteCanTrans;
        pTsPacket->realTransBytes = byteCanTrans;
    }

    pTsPacket->adaptation_field = af;

    LOGD("%ld time in pes func end", Now);
    return pTsPacket;
}

CTsPacket* CTsPacket::CreatePesFollowPacket2(bool isVideo, uint32_t payload_len, uint32_t pts, uint8_t continuity_counter)
{
    LOGD("%ld time in pes", Now);
    auto pTsPacket = new CTsPacket;
    LOGD("%ld time after make", Now);
    pTsPacket->payload_unit_start_indicator = 0;
    pTsPacket->pid = isVideo ? TS_VIDEO_AVC_PID : TS_AUDIO_AAC_PID;
    pTsPacket->adaptation_field_control = 0x01;
    pTsPacket->continuity_counter = continuity_counter;

    uint16_t byteCanTrans = 0;
    uint8_t stuffNum = 0;

    byteCanTrans = 188 - 4; /* - pesPkt->header_size();*/
    CTsAdaptationField* af = nullptr;
    // 要传输的负载比实际能传输的负载小, 则需要填充
    if (payload_len < byteCanTrans)
    {
        pTsPacket->realTransBytes = payload_len;
        if (byteCanTrans - payload_len == 1)
        {
            pTsPacket->adaptation_field_one_byte = true;
        }
        else
        {
            //  -2 减掉CTsAdaptationField的头
            stuffNum = byteCanTrans - payload_len - 2;
            af = new CTsAdaptationField();
            af->SetStuffingNum(stuffNum);
            pTsPacket->adaptation_field_control = 0x03;
        }
    }
    else
    {
        // pesPkt->payload_len = byteCanTrans;
        pTsPacket->realTransBytes = byteCanTrans;
    }

    pTsPacket->adaptation_field = af;
    if (pTsPacket->realTransBytes == 0)
    {
        LOGD("pause");
        LOGD("pause");
        LOGD("pause");
    }

    LOGD("%ld time in pes func end", Now);
    return pTsPacket;
}

uint16_t CTsPacket::encode(uint8_t *pData)
{
    uint32_t pos = 0;
    // 1byte
    pData[pos++] = sync_byte;

    // 2byte
    pData[pos] |= transport_error_indicator << 7;
    pData[pos] |= payload_unit_start_indicator << 6;
    pData[pos] |= transport_priority << 5;
    pData[pos++] |= ((pid >> 8) & 0x1f) ;
    pData[pos++] = pid;

    // 1byte
    pData[pos] = transport_scrambling_control << 6;
    pData[pos] |= adaptation_field_control << 4;
    pData[pos++] |= continuity_counter & 0x0f;

    if (adaptation_field_one_byte)
    {
        pData[pos++] = 0;
    }
    else if ((adaptation_field_control & 0x02) && adaptation_field)
    {
        adaptation_field->encode(&pData[pos]);
        pos += adaptation_field->size();
    }

    if ((adaptation_field_control & 0x01) && payload)
    {
        payload->encode(&pData[pos]);
        pos += payload->size();
    }

    return realTransBytes;
}

void CTsPacket::decode(uint8_t *pData, uint32_t payload_len)
{

}

uint32_t CTsPacket::size()
{
    uint32_t sz = 4;
    if ((adaptation_field_control & 0x10) && adaptation_field)
    {
        sz += adaptation_field->size();
    }

    if (adaptation_field_control & 0x01 && payload)
    {
        sz += payload->size();
    }

    return sz;
}

void CTsHub::OnVideo(uint8_t *pData, size_t size)
{
    uint8_t tmp[1024];
    memset(tmp, 0, 1024);
    // auto patPkt = CTsPacket::CreatePATPaket(TS_PMT_NUMBER, TS_PMT_PID);
    // tsPkt->encode(&tmp[0]);

    auto pmtPkt = CTsPacket::CreatePMTPaket(TsStreamVideoH264, TS_VIDEO_AVC_PID, TsStreamAudioAAC, TS_AUDIO_AAC_PID);
    pmtPkt->encode(&tmp[0]);

    std::cout << "1" << std::endl;
    std::cout << "1" << std::endl;
}

void CTsHub::OnAudio(uint8_t *pData, size_t size)
{
}

void CTsHub::demux(uint8_t *data, size_t size)
{
}