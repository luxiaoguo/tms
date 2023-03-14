/*
 **************************************************************************************
 *       Filename:  sdp.h
 *    Description:   header file
 *
 *        Version:  1.0
 *        Created:  2017-11-09 17:52:01
 *
 *       Revision:  initial draft;
 **************************************************************************************
 */

#ifndef SDP_H_INCLUDED
#define SDP_H_INCLUDED

#include <string>
#include <vector>
#include <iostream>

namespace sdp {

//======================================
//
//               enum(s)
//
//======================================
enum ENodeType {
    SDP_NODE_NONE = 0,
    SDP_NODE_SESSION,                // full sdp session
    SDP_NODE_VERSION,
    SDP_NODE_ORIGIN,
    SDP_NODE_SESSION_NAME,
    SDP_NODE_SESSION_INFORMATION,    // 5
    SDP_NODE_URI,
    SDP_NODE_EMAIL,
    SDP_NODE_PHONE,
    SDP_NODE_CONNECTION,
    SDP_NODE_BANDWIDTH,              // 10
    SDP_NODE_TZ,
    SDP_NODE_ENCRYPTION,
    SDP_NODE_TIME,
    SDP_NODE_REPEAT,
    SDP_NODE_MEDIA,                 // 15
    SDP_NODE_ATTRIBUTE, 
    SDP_NODE_GB_SSRC,
    SDP_NODE_GB_MEDIA_DESC, 
};
enum ENetType {
    SDP_NET_NONE = 0,
    SDP_NET_IN
};
enum EAddrType {
    SDP_ADDR_NONE = 0,
    SDP_ADDR_IP4,
    SDP_ADDR_IP6
};
enum EMediaType {
    SDP_MEDIA_NONE = 0,
    SDP_MEDIA_VIDEO,
    SDP_MEDIA_AUDIO,
    SDP_MEDIA_TEXT,
    SDP_MEDIA_APPLICATION,
    SDP_MEDIA_MESSAGE
};
enum EProtoType {
    SDP_PROTO_NONE = 0,
    SDP_PROTO_UDP,
    SDP_PROTO_RTP_AVP,
    SDP_PROTO_TCP_RTP_AVP,
    SDP_PROTO_RTP_SAVP,
    SDP_PROTO_RTP_SAVPF,
    SDP_PROTO_RTP_UDP2SAVPF
};
enum EAttrType {
    SDP_ATTR_NONE = 0,

    // rfc4566
    SDP_ATTR_CAT,
    SDP_ATTR_KEYWDS,
    SDP_ATTR_TOOL,
    SDP_ATTR_PTIME,
    SDP_ATTR_MAXPTIME,
    SDP_ATTR_RTPMAP,
    SDP_ATTR_RECVONLY,
    SDP_ATTR_SENDRECV,
    SDP_ATTR_SENDONLY,
    SDP_ATTR_INACTIVE,
    SDP_ATTR_ORIENT,
    SDP_ATTR_TYPE,
    SDP_ATTR_CHARSET,
    SDP_ATTR_SDPLANG,
    SDP_ATTR_LANG,
    SDP_ATTR_FRAMERATE,
    SDP_ATTR_QUALITY,
    SDP_ATTR_FMTP,
    SDP_ATTR_RANGE,
    SDP_ATTR_CONTROL,

    //rfc3605
    SDP_ATTR_RTCP,
    //rfc5245
    SDP_ATTR_CANDIDATE,
    SDP_ATTR_ICE_UFRAG,
    SDP_ATTR_ICE_PWD,
    SDP_ATTR_ICE_OPTIONS,
    SDP_ATTR_FINGERPRINT,
    SDP_ATTR_SETUP,
    SDP_ATTR_GROUP,
    SDP_ATTR_MID,
    SDP_ATTR_EXTMAP,
    SDP_ATTR_RTCPMUX,
    SDP_ATTR_RTCPRSIZE,
    SDP_ATTR_RTCPFB,
    SDP_ATTR_CRYPTO,
    SDP_ATTR_SSRC,
    SDP_ATTR_SSRC_GROUP,
    SDP_ATTR_MSID,
    SDP_ATTR_MSID_SEMANTIC,
    SDP_ATTR_SCTPMAP,

    // video resolution to be compatiable with helix server
    SDP_ATTR_CLIPRECT,
    // GB28181 f
    SDP_ATTR_GB_SETUP,
    SDP_ATTR_GB_CONNECTION,
    SDP_ATTR_GB_FILE_SIZE,
    SDP_ATTR_GB_DOWN_LOAD_SPEED,
    SDP_ATTR_GB_VKEK,
    // SDP_ATTR_GB_SSRC,
    // SDP_ATTR_GB_MEDIA_DESC,
};
enum ECandiType {
    SDP_CANDI_NONE,
    SDP_CANDI_HOST,
    SDP_CANDI_SRFLX,
    SDP_CANDI_PRFLX,
    SDP_CANDI_RELAY
};

enum EVideoEncodeType
{
    SDP_VIDEO_ENCODE_TYPE_MPEG4 = 1,
    SDP_VIDEO_ENCODE_TYPE_H264, 
    SDP_VIDEO_ENCODE_TYPE_SVAC, 
    SDP_VIDEO_ENCODE_TYPE_3GP,  
    SDP_VIDEO_ENCODE_TYPE_H265, 
};

enum EAudioEncodeType
{
    SDP_ADUIO_ENCODETYPE_G711      = 1,    /// G.711
    SDP_ADUIO_ENCODETYPE_G723,     /// 2   /// G.723.1
    SDP_ADUIO_ENCODETYPE_G729,     /// 3   /// G.729
    SDP_ADUIO_ENCODETYPE_G722,     /// 4   /// G.722.1
    SDP_ADUIO_ENCODETYPE_AACLC      = 11,   ///AAC-LC
    SDP_ADUIO_ENCODETYPE_ADPCM,    ///12    ///ADPCM
    SDP_ADUIO_ENCODETYPE_OPUS,  
};

enum EResolution
{
    SDP_RESOLUTION_QCIF  = 1,
    SDP_RESOLUTION_CIF,  
    SDP_RESOLUTION_4CIF, 
    SDP_RESOLUTION_D1, 
    SDP_RESOLUTION_720P, 
    SDP_RESOLUTION_1080P,
};

enum EVideoBitRateType
{
    SDP_VIDEO_BITRATE_TYPE_CBR = 1,
    SDP_VIDEO_BITRATE_TYPE_VBR,
};

enum EAudioBitRate
{
    SDP_AUDIO_BITRATE_FOR_G723_5_3Kbps   = 1,    /// 5.3 kbps （注：G.723.1中使用）
    SDP_AUDIO_BITRATE_FOR_G723_6_3Kbps,  /// 2   /// 6.3 kbps （注：G.723.1中使用）
    SDP_AUDIO_BITRATE_FOR_G729_8Kbps,    /// 3   /// 8 kbps （注：G.729中使用）
    SDP_AUDIO_BITRATE_FOR_G722_16Kbps,   /// 4   /// 16 kbps （注：G.722.1中使用）
    SDP_AUDIO_BITRATE_FOR_G722_24Kbps,   /// 5   /// 24 kbps （注：G.722.1中使用）
    SDP_AUDIO_BITRATE_FOR_G722_32Kbps,   /// 6   /// 32 kbps （注：G.722.1中使用）
    SDP_AUDIO_BITRATE_FOR_G722_48Kbps,   /// 7   /// 48 kbps （注：G.722.1中使用）
    SDP_AUDIO_BITRATE_FOR_G711_64Kbps,   /// 8   /// 64 kbps （注：G.711中使用）
    SDP_AUDIO_BITRATE_20Kbps             =21,    ///20 kbps
    SDP_AUDIO_BITRATE_128Kbps,
};

enum EAudioSampleRate
{
    SDP_AUDIO_SAMPLERATE_1_8kHz            = 1,    /// 1-8 kHz （注：G.711/ G.723.1/ G.729中使用）
    SDP_AUDIO_SAMPLERATE_FOR_G722_2_14kHz, /// 2   /// 2-14 kHz （注：G.722.1中使用）
    SDP_AUDIO_SAMPLERATE_FOR_G722_3_16kHz, /// 3   /// 3-16 kHz（注：G.722.1中使用）
    SDP_AUDIO_SAMPLERATE_FOR_G722_4_32kHz, /// 4   /// 4-32 kHz（注：G.722.1中使用）
    SDP_AUDIO_SAMPLERATE_11_44_1kHz =11, /// 11   /// 11-44.1 kHz
    SDP_AUDIO_SAMPLERATE_12_48kHz,       /// 12-48 kHz
};

//======================================
//
//               interfaces
//
//======================================
class SdpRoot;
class SdpNode;
class SdpWriter;
class SdpReader;
class SdpMedia;
class SdpAttr;

class SdpReader {
public:
    int parse(const std::string& s, SdpRoot& sdp);
};
class SdpWriter {
public:
    int write(std::string& s, SdpRoot& sdp);
};

class SdpNode {
public:
    SdpNode(ENodeType t) : nodeType(t) {}
    virtual ~SdpNode();
    virtual void dump();

    virtual SdpNode* clone() {return new SdpNode(SDP_NODE_NONE);}

    int add(SdpNode* n);
    int remove(SdpNode* n);

    int find(ENodeType t,  SdpNode*& n);
    int find(EAttrType t,  SdpAttr*& n);
    int find(EMediaType t, SdpMedia*& n);
    int find(ENodeType t,  std::vector<SdpNode*>& v);
    int find(EAttrType t,  std::vector<SdpAttr*>& v);
    int find(EMediaType t, std::vector<SdpMedia*>& v);
    virtual int parse(std::string& l);
    virtual int write(std::string& l);

public:
    ENodeType nodeType;
    std::vector<SdpNode*> children;
};
class SdpRoot : public SdpNode {
public:
    SdpRoot() : SdpNode(SDP_NODE_SESSION) {}
};
class SdpFactory {
public:
    static ENodeType getNodeType(std::string& l);
    static EAttrType getAttrType(std::string& l);

    static SdpNode* createNode(ENodeType type);
    static SdpAttr* createAttr(EAttrType type);
};

//======================================
//
// sub class used for reader and writer
//
//======================================
class SdpNone : public SdpNode {
public:
    SdpNone() : SdpNode(SDP_NODE_NONE) {}
    SdpNode* clone() {return new SdpNone; }
};
class SdpVersion : public SdpNode {
public:
    SdpVersion() : SdpNode(SDP_NODE_VERSION) {}
    SdpNode* clone() {return new SdpVersion;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    int version;
};
class SdpOrigin : public SdpNode {
public:
    SdpOrigin() : SdpNode(SDP_NODE_ORIGIN) {}
    SdpNode* clone() {return new SdpOrigin;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    std::string userName;
    std::string sid;
    uint64_t    sessVersion;
    ENetType    netType;
    EAddrType   addrType;
    std::string addr;
};
class SdpSessName : public SdpNode {
public:
    SdpSessName() : SdpNode(SDP_NODE_SESSION_NAME) {}
    SdpNode* clone() {return new SdpSessName;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    std::string name;
};
class SdpSessInfo : public SdpNode {
public:
    SdpSessInfo() : SdpNode(SDP_NODE_SESSION_INFORMATION) {}
    SdpNode* clone() {return new SdpSessInfo;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    std::string info;
};
class SdpUri : public SdpNode {
public:
    SdpUri() : SdpNode(SDP_NODE_URI) {}
    SdpNode* clone() {return new SdpUri;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    std::string uri;
};
class SdpEmail : public SdpNode {
public:
    SdpEmail() : SdpNode(SDP_NODE_EMAIL) {}
    SdpNode* clone() {return new SdpEmail;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    std::string email;
};
class SdpPhone : public SdpNode {
public:
    SdpPhone() : SdpNode(SDP_NODE_PHONE) {}
    SdpNode* clone() {return new SdpPhone;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    std::string phone;
};
class SdpTime : public SdpNode {
public:
    SdpTime() : SdpNode(SDP_NODE_TIME) {}
    SdpNode* clone() {return new SdpTime;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    uint64_t start;
    uint64_t stop;
};
class SdpConn : public SdpNode {
public:
    SdpConn() : SdpNode(SDP_NODE_CONNECTION) {}
    SdpNode* clone() {return new SdpConn;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    ENetType    netType;
    EAddrType   addrType;
    std::string addr;
};
class SdpMedia : public SdpNode {
public:
    SdpMedia() : SdpNode(SDP_NODE_MEDIA) {}
    SdpNode* clone() {return new SdpMedia;}

    int parse(std::string& l);
    int write(std::string& l);

    int filter(int pt);
    int filter(EAttrType aType);
    int reject();
    int inactive();
    int getPT(std::string& codec);
    uint32_t ssrc();
    uint32_t bandwidth();
    std::vector<uint32_t> ssrcGrp();
    int addCandidate(SdpNode* n);
    int updateIce(const std::string &ufrag, const std::string &pwd, const std::string &fp);
public:
    EMediaType       mediaType;
    uint16_t         port;
    EProtoType       proto;
    std::vector<int> supportedPTs;
};
class SdpBandWidth : public SdpNode {
public:
    SdpBandWidth() : SdpNode(SDP_NODE_BANDWIDTH) {}
    SdpNode* clone() {return new SdpBandWidth;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    std::string type;
    uint32_t    bw;
};
class SdpAttr : public SdpNode {
public:
    SdpAttr(EAttrType t) : SdpNode(SDP_NODE_ATTRIBUTE), attrType(t) {}
    SdpNode* clone() {return new SdpAttr(this->attrType);}
    int parse(std::string& l);
    int write(std::string& l);
public:
    EAttrType   attrType;
    std::string val;
};
class SdpAttrRtpMap : public SdpAttr {
public:
    SdpAttrRtpMap() : SdpAttr(SDP_ATTR_RTPMAP) {}
    SdpNode* clone() {return new SdpAttrRtpMap;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    int pt;
    std::string enc;
    std::string param;
};
class SdpAttrRTCPFB : public SdpAttr {
public:
    SdpAttrRTCPFB() : SdpAttr(SDP_ATTR_RTCPFB) {}
    SdpNode* clone() {return new SdpAttrRTCPFB;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    int pt;
    std::string param;
};
class SdpAttrFmtp : public SdpAttr {
public:
    SdpAttrFmtp() : SdpAttr(SDP_ATTR_FMTP) {}
    SdpNode* clone() {return new SdpAttrFmtp;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    int pt;
    std::string param;
};
class SdpAttrRange: public SdpAttr {
public:
    SdpAttrRange() : SdpAttr(SDP_ATTR_RANGE) {}
    SdpNode* clone() {return new SdpAttrRange;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    std::string start;
    std::string end;
};
class SdpAttrRTCP : public SdpAttr {
public:
    SdpAttrRTCP() : SdpAttr(SDP_ATTR_RTCP) {}
    SdpNode* clone() {return new SdpAttrRTCP;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    uint16_t    port;
    ENetType    netType;
    EAddrType   addrType;
    std::string addr;
};
class SdpAttrCandi : public SdpAttr {
public:
    SdpAttrCandi() : SdpAttr(SDP_ATTR_CANDIDATE) {}
    SdpNode* clone() {return new SdpAttrCandi;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    std::string foundation;
    uint64_t    compID;
    std::string transport = "udp";
    uint64_t    priority;
    std::string addr;
    uint16_t    port;
    ECandiType  candiType;
    std::string relAddr;
    uint16_t    relPort;
    std::string extName;
    std::string extVal;
};
class SdpAttrSsrc : public SdpAttr {
public:
    SdpAttrSsrc() : SdpAttr(SDP_ATTR_SSRC) {}
    SdpNode* clone() {return new SdpAttrSsrc;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    uint32_t ssrc;
    std::string attr;
    std::string val;
};
class SdpAttrSsrcGrp : public SdpAttr {
public:
    SdpAttrSsrcGrp() : SdpAttr(SDP_ATTR_SSRC_GROUP) {}
    SdpNode* clone() {return new SdpAttrSsrcGrp;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    std::string semantics;
    std::vector<uint32_t> ssrcs;
};
class SdpAttrClipRect : public SdpAttr {
public:
    SdpAttrClipRect() : SdpAttr(SDP_ATTR_CLIPRECT) {}
    SdpNode* clone() {return new SdpAttrClipRect;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
};

class SdpAttrGBDownLoadSpeed: public SdpAttr {
public:
    SdpAttrGBDownLoadSpeed() : SdpAttr(SDP_ATTR_GB_DOWN_LOAD_SPEED) {}
    SdpNode* clone() {return new SdpAttrGBDownLoadSpeed;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    uint64_t downLoadSpeed;
};

class SdpAttrGBFileSize: public SdpAttr {
public:
    SdpAttrGBFileSize() : SdpAttr(SDP_ATTR_GB_FILE_SIZE) {}
    SdpNode* clone() {return new SdpAttrGBFileSize;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    uint64_t fileSize;
};

class SdpAttrGBVkek: public SdpAttr {
public:
    SdpAttrGBVkek() : SdpAttr(SDP_ATTR_GB_VKEK) {}
    SdpNode* clone() {return new SdpAttrGBVkek;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    std::string version;
    std::string vkek;
    bool bUsebrackets = false;
};


class SdpGBSsrc: public SdpNode
{
public:
    SdpGBSsrc() : SdpNode(SDP_NODE_GB_SSRC) {}
    SdpNode* clone() {return new SdpGBSsrc;}
    int parse(std::string& l);
    int write(std::string& l);

public:
    std::string gbSsrc;
};

class SdpGBMediaDesc : public SdpNode {
public:
    SdpGBMediaDesc() : SdpNode(SDP_NODE_GB_MEDIA_DESC) {}
    SdpNode* clone() {return new SdpGBMediaDesc;}
    int parse(std::string& l);
    int write(std::string& l);
public:
    EVideoEncodeType videoEncType;
    EResolution resol;
    uint32_t frameRate;
    EVideoBitRateType videoBitRateType;
    uint32_t videoBitRate;
    EAudioEncodeType audioEncType;
    EAudioBitRate audioBitRate;
    EAudioSampleRate audioSampleRate;
};

std::string generateCname();
std::string generateLabel();
std::string generateMslabel();
std::string generateFoundation(const std::string& type, const std::string& protocol, const std::string& relay_protocol, const std::string& address);

}; //namespace sdp

#endif /*SDP_H_INCLUDED*/

/********************************** END **********************************************/
