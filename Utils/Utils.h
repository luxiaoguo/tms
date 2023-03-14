#ifndef _UTILS_H
#define _UTILS_H
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <regex>
#include <string>
#include <chrono>
#include <vector>
#include <sstream>
#include <string>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <zlib.h>


extern "C"
{
#include "libavcodec/packet.h"
#include "libavformat/avformat.h"
#include "libavcodec/bsf.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/codec.h"
#include "libavutil/error.h"
}


/*
static int64_t getCurTime()// 获取当前系统启动以来的毫秒数
{
#ifndef WIN32
    // Linux系统
    struct timespec now;// tv_sec (s) tv_nsec (ns-纳秒)
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1000 + now.tv_nsec / 1000000);
#else
    long long now = std::chrono::steady_clock::now().time_since_epoch().count();
    return now / 1000000;
#endif // !WIN32

}
static int64_t getCurTimestamp()// 获取毫秒级时间戳（13位）
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).
        count();

}
*/

inline std::string getCurFormatTime(const char *format = "%Y-%m-%d %H:%M:%S") { //

    time_t t = time(nullptr);
    char tc[64];
    strftime(tc, sizeof(tc), format, localtime(&t));

    std::string tcs = tc;
    return tcs;
}

/*
static int genRandomInt() {
    std::string numStr;

    numStr.append(std::to_string(rand()%9 + 1));
    numStr.append(std::to_string(rand() % 10));
    numStr.append(std::to_string(rand() % 10));
    numStr.append(std::to_string(rand() % 10));
    numStr.append(std::to_string(rand() % 10));
    numStr.append(std::to_string(rand() % 10));
    numStr.append(std::to_string(rand() % 10));
    numStr.append(std::to_string(rand() % 10));
    int num = stoi(numStr);

    return num;
}

static std::vector<std::string> split(std::string file,std::string pattern) {

    std::string::size_type pos;
    std::vector<std::string> result;
    file += pattern;//扩展字符串以方便操作
    int size = file.size();
    for (int i = 0; i < size; i++) {
        pos = file.find(pattern, i);
        if (pos < size) {
            std::string s = file.substr(i, pos - i);
            result.push_back(s);
            i = pos + pattern.size() - 1;
        }
    }
    return result;
}
*/

inline void printfHex(void *p, size_t size)
{
    unsigned char *data = (unsigned char *)p;
    std::stringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0');
    for (uint32_t index = 0; index < size; ++index)
    {
        ss << std::setw(2) << static_cast<unsigned>(data[index]);
        std::cout << ss.str() << " ";
        ss.clear();
        ss.str("");
    }
    std::cout << "\n";
}

inline void Set8(void* buf, const uint32_t offset, const uint8_t value)
{
    static_cast<uint8_t*>(buf)[offset] = value;
}

inline void SetBE16(void* buf, const uint16_t value)
{
    Set8(buf, 0, static_cast<uint8_t>(value >> 8));
    Set8(buf, 1, static_cast<uint8_t>(value));
}

inline void SetLE16(void* buf, const uint16_t value)
{
    memcpy(buf, &value, sizeof(uint16_t));
}

inline void SetBE32(void* buf, const uint32_t value)
{
    Set8(buf, 0, static_cast<uint8_t>(value >> 24));
    Set8(buf, 1, static_cast<uint8_t>(value >> 16));
    Set8(buf, 2, static_cast<uint8_t>(value >> 8));
    Set8(buf, 3, static_cast<uint8_t>(value));
}

inline void SetLE32(void* buf, const uint32_t value)
{
    memcpy(buf, &value, sizeof(uint32_t));
}

inline void SetBE64(void* buf, const uint64_t value)
{
    Set8(buf, 0, static_cast<uint8_t>(value >> 56));
    Set8(buf, 1, static_cast<uint8_t>(value >> 48));
    Set8(buf, 2, static_cast<uint8_t>(value >> 40));
    Set8(buf, 3, static_cast<uint8_t>(value >> 32));
    Set8(buf, 4, static_cast<uint8_t>(value >> 24));
    Set8(buf, 5, static_cast<uint8_t>(value >> 16));
    Set8(buf, 6, static_cast<uint8_t>(value >> 8));
    Set8(buf, 7, static_cast<uint8_t>(value));
}

inline void SetLE64(void* buf, const uint64_t value)
{
    memcpy(buf, &value, sizeof(uint64_t));
}

inline uint8_t Get8(const void* buf, const uint32_t offset)
{
    return static_cast<const uint8_t*>(buf)[offset];
}

inline uint16_t GetBE16(const void* buf)
{
    return static_cast<uint16_t>((Get8(buf, 0) << 8) | (Get8(buf, 1) << 0));
}

inline uint16_t GetLE16(const void* buf)
{
    return static_cast<const uint16_t*>(buf)[0];
}

inline uint32_t GetBE32(const void* buf)
{
    return (static_cast<uint32_t>(Get8(buf, 0)) << 24) |
           (static_cast<uint32_t>(Get8(buf, 1)) << 16) |
           (static_cast<uint32_t>(Get8(buf, 2)) << 8) |
           (static_cast<uint32_t>(Get8(buf, 3)));
}

inline uint32_t GetLE32(const void* buf)
{
    return static_cast<const uint32_t*>(buf)[0];
}

inline uint64_t GetBE64(const void* buf)
{
    return (static_cast<uint64_t>(Get8(buf, 0)) << 56) |
           (static_cast<uint64_t>(Get8(buf, 1)) << 48) |
           (static_cast<uint64_t>(Get8(buf, 2)) << 40) |
           (static_cast<uint64_t>(Get8(buf, 3)) << 32) |
           (static_cast<uint64_t>(Get8(buf, 4)) << 24) |
           (static_cast<uint64_t>(Get8(buf, 5)) << 16) |
           (static_cast<uint64_t>(Get8(buf, 6)) << 8) |
           (static_cast<uint64_t>(Get8(buf, 7)));

}

inline uint64_t GetLE64(const void* buf)
{
    return static_cast<const uint64_t*>(buf)[0];
}

inline std::string CreateSessionID(int num)
{
    srand((unsigned)time(NULL));
    static const char az[62] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
                          'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F',
                          'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
                          'W', 'X', 'Y', 'Z', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
    std::string strSessionID;

    for (int i = 0; i < num; ++i)
    {
        int j = rand() % 62;
        strSessionID.append(&az[j], 1);
    }

    return strSessionID;
}

const uint8_t start_code3[3] = {0, 0, 1};
const uint8_t start_code4[4] = {0, 0, 0, 1};

inline const uint8_t *StartCode(uint8_t len)
{
    if (len == 3)
    {
        return start_code3;
    }
    else if (len == 4)
    {
        return start_code4;
    }

    return nullptr;
}

inline uint32_t crc32(const unsigned char *pData, uint32_t size)
{
    uint32_t crc = crc32(0L, Z_NULL, 0);
    //   crc = crc32(crc, reinterpret_cast<const Bytef*>(str.c_str()), str.size());
    crc = crc32(crc, pData, size);
    return crc;
}

typedef unsigned char uchar;
static std::string base64_decode(const std::string &in) {

    std::string out;

    std::vector<int> T(256,-1);
    for (int i=0; i<64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

    int val=0, valb=-8;
    for (uchar c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val>>valb)&0xFF));
            valb -= 8;
        }
    }
    return out;
}

// 从 SDP 描述符中提取 SPS 和 PPS 参数
inline void extract_sps_pps(const std::string &sdp, std::vector<uint8_t> &sps, std::vector<uint8_t> &pps)
{
    std::smatch out;
    std::regex sps_pps("sprop-parameter-sets=(.*),(.*);");
    if (!std::regex_search(sdp, out, sps_pps))
    {
        return;
    }

    // insert start code
    sps.insert(sps.begin(), StartCode(3), StartCode(3) + 3);
    pps.insert(pps.begin(), StartCode(3), StartCode(3) + 3);

    auto tmp_sps = base64_decode(out[1].str());
    auto tmp_pps = base64_decode(out[2].str());

    sps.insert(sps.end(), tmp_sps.begin(), tmp_sps.end());
    pps.insert(pps.end(), tmp_pps.begin(), tmp_pps.end());

    printfHex(&sps[0], sps.size());
    printfHex(&pps[0], pps.size());
    printfHex(&pps[0], pps.size());
}

inline void extract_vps_sps_pps(const std::string &sdp, std::vector<uint8_t> &vps, std::vector<uint8_t> &sps, std::vector<uint8_t> &pps)
{
    std::smatch out;
    std::regex reVps("sprop-vps=(.*)(;|\r\n)");
    if (std::regex_search(sdp, out, reVps))
    {
        vps.insert(vps.begin(), StartCode(3), StartCode(3) + 3);
        auto tmp_vps = base64_decode(out[1].str());
        vps.insert(vps.end(), tmp_vps.begin(), tmp_vps.end());
    }

    std::regex reSps("sprop-sps=(.*)(;|\r\n)");
    if (std::regex_search(sdp, out, reSps))
    {
        sps.insert(sps.begin(), StartCode(3), StartCode(3) + 3);
        auto tmp_sps = base64_decode(out[1].str());
        sps.insert(sps.end(), tmp_sps.begin(), tmp_sps.end());
    }

    std::regex rePps("sprop-pps=(.*)(;|\r\n)");
    if (std::regex_search(sdp, out, rePps))
    {
        pps.insert(pps.begin(), StartCode(3), StartCode(3) + 3);
        auto tmp_pps = base64_decode(out[1].str());
        pps.insert(pps.end(), tmp_pps.begin(), tmp_pps.end());
    }
}

static uint32_t adts_samples[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};
inline uint8_t samples2adtsFreqIdx(uint16_t samples)
{
    for (int i = 0; i < 12; i++)
    {
        if (samples == adts_samples[i])
        {
            return i;
        }
    }

    return 0;
}

void getResolutionFromSps(const std::vector<uint8_t> &sps);

#endif