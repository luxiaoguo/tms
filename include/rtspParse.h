#ifndef _RTSP_PARSE_H_
#define _RTSP_PARSE_H_

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

struct TRange
{
    double begin = 0.0;
    double end = -1;

    std::string to_string()
    {
        std::string str;
        str += "npt=";
        str += std::to_string(begin) + "-";
        if (end > 0)
        {
            str += std::to_string(end);
        }
        return str;
    }
};

struct TTransPort
{
    int clientPort = -1;
    int serverPort = -1;
    std::string protocol = "RTP/AVP/UDP";
    uint16_t ssrc = 0;
    bool IsUnicast = true;

    std::string to_string()
    {
      /*Transport: RTP/AVP/UDP;unicast;client_port=55656-55657;server_port=30000-30001;ssrc=00000002*/
        std::string str;
        str += protocol;
        str += ";";
        str += "unicast";
        if (clientPort > 0)
        {
            str += ";client_port=" + std::to_string(clientPort) + "-" + std::to_string(clientPort + 1);
        }

        if (serverPort> 0)
        {
            str += ";server_port=" + std::to_string(serverPort) + "-" + std::to_string(serverPort + 1);
        }

        if (ssrc > 0)
        {
            str += ";ssrc=" + std::to_string(ssrc);
        }

        return str;
    }
};

struct TRTPInfo
{
    std::string videoUrl;
    std::string audioUrl;
    uint16_t videoSeq = 0;
    uint16_t audioSeq = 0;
    uint16_t videoRtpTime = 0;
    uint16_t audioRtpTime = 0;

    std::string to_string()
    {
        std::string str;
        str += "url=" + videoUrl + ";seq=" + std::to_string(videoSeq) + ";rtptime=" + std::to_string(videoRtpTime);
        str += ",";
        str += "url=" + audioUrl + ";seq=" + std::to_string(audioSeq) + ";rtptime=" + std::to_string(audioRtpTime);
        return str;
    }
};

class RtspReq
{
  public:
    RtspReq(const std::string &src);
    RtspReq()
    {
    }
    ~RtspReq();

  public:
    std::string GetMod();
    std::string GetUrl();
    std::string GetUri();
    std::string GetVersion();
    std::string GetUserAgent();
    std::string GetAccept();
    TTransPort GetTransport();
    std::string GetSession();
    bool GetRange(TRange &tRange);
    uint32_t GetContentLength();
    std::string GetBody();
    int GetCSeq();

  private:
    std::string m_strSrc;
    std::map<std::string, std::string> m_header;
};

class RtspRsp
{
  public:
    RtspRsp();
    ~RtspRsp();

  public:
    void setStatus(int status);
    void setHeader(const std::string &key, const std::string &value);
    void setContent(const std::string &body, const char* content_type);
    std::string toString();

  private:
    std::map<std::string, std::string> m_header;
    std::string m_body;
    int m_status;
};

class RtspReq2Client
{
  public:
    RtspReq2Client();
    ~RtspReq2Client();

  public:
    void SetMod(const std::string mod, const std::string url);
    void SetHeader(const std::string &key, const std::string &value);
    std::string toString();

  private:
      std::string m_mod;
      std::map<std::string, std::string> m_header;
};

#endif