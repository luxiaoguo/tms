#include "rtspParse.h"
#include <cstdint>
#include <iostream>
#include <regex>
#include <string>
#include <utility>
#include <vector>

std::string RTSP_MOD[11] = {"OPTIONS", "DESCRIBE", "ANNOUNCE", "GET_PARAMETER", "PAUSE",   "PLAY",
                            "RECORD", "REDIRECT", "SETUP",    "SET_PARAMETER", "TEARDOWN"};
const char *status_message(int status)
{
    switch (status)
    {
    case 100:
        return "Continue";
    case 101:
        return "Switching Protocol";
    case 102:
        return "Processing";
    case 103:
        return "Early Hints";
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 202:
        return "Accepted";
    case 203:
        return "Non-Authoritative Information";
    case 204:
        return "No Content";
    case 205:
        return "Reset Content";
    case 206:
        return "Partial Content";
    case 207:
        return "Multi-Status";
    case 208:
        return "Already Reported";
    case 226:
        return "IM Used";
    case 300:
        return "Multiple Choice";
    case 301:
        return "Moved Permanently";
    case 302:
        return "Found";
    case 303:
        return "See Other";
    case 304:
        return "Not Modified";
    case 305:
        return "Use Proxy";
    case 306:
        return "unused";
    case 307:
        return "Temporary Redirect";
    case 308:
        return "Permanent Redirect";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 402:
        return "Payment Required";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 406:
        return "Not Acceptable";
    case 407:
        return "Proxy Authentication Required";
    case 408:
        return "Request Timeout";
    case 409:
        return "Conflict";
    case 410:
        return "Gone";
    case 411:
        return "Length Required";
    case 412:
        return "Precondition Failed";
    case 413:
        return "Payload Too Large";
    case 414:
        return "URI Too Long";
    case 415:
        return "Unsupported Media Type";
    case 416:
        return "Range Not Satisfiable";
    case 417:
        return "Expectation Failed";
    case 418:
        return "I'm a teapot";
    case 421:
        return "Misdirected Request";
    case 422:
        return "Unprocessable Entity";
    case 423:
        return "Locked";
    case 424:
        return "Failed Dependency";
    case 425:
        return "Too Early";
    case 426:
        return "Upgrade Required";
    case 428:
        return "Precondition Required";
    case 429:
        return "Too Many Requests";
    case 431:
        return "Request Header Fields Too Large";
    case 451:
        return "Unavailable For Legal Reasons";
    case 501:
        return "Not Implemented";
    case 502:
        return "Bad Gateway";
    case 503:
        return "Service Unavailable";
    case 504:
        return "Gateway Timeout";
    case 505:
        return "HTTP Version Not Supported";
    case 506:
        return "Variant Also Negotiates";
    case 507:
        return "Insufficient Storage";
    case 508:
        return "Loop Detected";
    case 510:
        return "Not Extended";
    case 511:
        return "Network Authentication Required";

    default:
    case 500:
        return "Internal Server Error";
    }
}

RtspReq::RtspReq(const std::string &src):m_strSrc(src)
{
}

RtspReq::~RtspReq()
{
}

std::string RtspReq::GetMod()
{
    for (auto it : RTSP_MOD)
    {
        if (m_strSrc.find(it) != std::string::npos)
            return it;
    }

    return std::string();
}

std::string RtspReq::GetUrl()
{
    std::smatch out;
    std::regex url("(rtsp://.+) ");
    if (!std::regex_search(m_strSrc, out, url))
    {
        return std::string();
    }

    return out[1].str();
}

std::string RtspReq::GetUri() 
{
    std::smatch out;
    std::regex uri("(rtsp://.+):\\d+/(.*) ");

    if (!std::regex_search(m_strSrc, out, uri))
    {
        return std::string();
    }

    return out[2].str();
}


std::string RtspReq::GetVersion()
{
    std::smatch out;
    std::regex version("(RTSP/)([0-9]\\.[0-9])");

    if (!std::regex_search(m_strSrc, out, version))
    {
        return std::string();
    }

    return out[2].str();
}

/**
 * @brief
 * 则表达式内每个括号代表一个部分 out[0]代表完整匹配，out[1]代表第一个小括号表示的部分，也就是CSeq: ,
 * out[2]代表[0-9]*匹配到的部分, 所以这里直接返回out[2]就是cseq的数值
 *
 */
int RtspReq::GetCSeq()
{
    std::smatch out;
    std::regex value("(CSeq: )([0-9]*)");
    if (!std::regex_search(m_strSrc, out, value))
    {
        return -1;
    }

    return stoi(out[2].str());
}

std::string RtspReq::GetUserAgent()
{
    std::smatch out;
    std::regex value("(User-Agent: )(.*)\r\n");
    if (!std::regex_search(m_strSrc, out, value))
    {
        return std::string();
    }

    return out[2].str();
}

std::string RtspReq::GetAccept()
{
    std::smatch out;
    std::regex value("(Accept: )(.*)\r\n");
    if (!std::regex_search(m_strSrc, out, value))
    {
        return std::string();
    }

    return out[2].str();
}

TTransPort RtspReq::GetTransport()
{
    TTransPort tTransPort;
    std::smatch out;
    std::regex value("(Transport: )(.*);(.*);client_port=(\\d+)-(\\d+)");
    if (!std::regex_search(m_strSrc, out, value))
    {
        return tTransPort;
    }

    tTransPort.protocol = out[2].str();
    tTransPort.IsUnicast = out[3].str() == "unicast";
    tTransPort.clientPort= out[4].str().empty() ? -1 : std::stoi(out[4].str());

    return tTransPort;
}

std::string RtspReq::GetSession()
{
    std::smatch out;
    std::regex value("(Session: )(.*)\r\n");
    if (!std::regex_search(m_strSrc, out, value))
    {
        return std::string();
    }

    return out[2].str();
}

bool RtspReq::GetRange(TRange &tRange)
{
    std::smatch out;
    std::regex value("(Range: )npt=(.*)-(.*)\r\n");
    if (!std::regex_search(m_strSrc, out, value))
    {
        return false;
    }

    std::string start = out[2].str();
    std::string end = out[3].str();

    tRange.begin = start.empty() ? -1 : std::stof(start);
    tRange.end = end.empty() ? -1 : std::stof(end);

    return true;
}

uint32_t RtspReq::GetContentLength()
{
    uint32_t length = 0;
    std::smatch out;
    std::regex value("Content-Length: (\\d+)");
    if (!std::regex_search(m_strSrc, out, value))
    {
        return 0;
    }

    length = stoi(out[1].str());
    return length;
}

std::string RtspReq::GetBody()
{
    std::smatch out;
    std::regex value("\r\n\r\n(.*)");
    if (!std::regex_search(m_strSrc, out, value))
    {
        return std::string();
    }

    return out[1].str();
}

/**
 * @brief Construct a new Rtsp Rsp:: Rtsp Rsp object
 * rtsp 响应消息
 */
RtspRsp::RtspRsp()
{

}

RtspRsp::~RtspRsp()
{
}

void RtspRsp::setStatus(int status)
{
    m_status = status;
}

void RtspRsp::setHeader(const std::string &key, const std::string &value)
{
    m_header.insert(std::make_pair(key, value));
}

void RtspRsp::setContent(const std::string &body, const char *content_type)
{
    if (body.empty())
        return;
    setHeader("Content-Length", std::to_string(body.size()));
    setHeader("Content-Type", content_type);

    m_body = body;
}

std::string RtspRsp::toString()
{
    std::string str;
    str = "RTSP/1.0 " + std::to_string(m_status) + " " + std::string(status_message(m_status)) + std::string("\r\n");

    for (const auto &it : m_header)
    {
        str = str + it.first + ": " + it.second + "\r\n";
    }

    str += "\r\n";
    if (!m_body.empty())
    {
        str += m_body;
    }

    return str;
}

RtspReq2Client::RtspReq2Client()
{
}

RtspReq2Client::~RtspReq2Client()
{
}

void RtspReq2Client::SetMod(const std::string mod, const std::string url)
{
    m_mod = mod + " " + url + " " + "RTSP/1.0";
}

void RtspReq2Client::SetHeader(const std::string &key, const std::string &value)
{
    m_header.insert(std::make_pair(key, value));
}

std::string RtspReq2Client::toString()
{
    std::string str;
    str += m_mod + "\r\n";

    for (const auto &it : m_header)
    {
        str = str + it.first + ": " + it.second + "\r\n";
    }
    str = str + "\r\n";

    return str;
}