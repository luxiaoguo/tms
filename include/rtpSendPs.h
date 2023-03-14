//
// Created by bxc on 2022/12/15.
//

#ifndef BXC_GB28181CLIENT_RTPSENDPS_H
#define BXC_GB28181CLIENT_RTPSENDPS_H
#include "sender.hpp"
#include <cstdint>
#include <fcntl.h>
#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32 // Linux系统
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#endif // !WIN32

#include "define.h"
#include <thread>
/***
 *@remark:  讲传入的数据按位一个一个的压入数据
 *@param :  buffer   [in]  压入数据的buffer
 *          count    [in]  需要压入数据占的位数
 *          bits     [in]  压入的数值
 */


class RtpSendPs:public CSender
{
  public:
    explicit RtpSendPs(const char *rtpServerIp, int rtpServerPort, int localRtpPort);
    RtpSendPs() = delete;
    ~RtpSendPs();

  public:
    void start();
    void stop();

    virtual void Send(const std::shared_ptr<Frame> &tFrame) override;
    virtual void Send(const uint8_t *buffer, size_t size, bool isVideo) override;
    virtual void UpdateRemotePort(uint16_t port) override
    {
        mRtpServerPort = port;
    }
    virtual E_STREAM_TYPE StreamType() override
    {
        return E_STREAM_PS;
    }

  private:
    void sendPkt(char *pData, size_t size, uint64_t lPts, bool bIsAudio);
    int findStartCode(unsigned char *buf, int zeros_in_startcode);
    int getNextNalu(FILE *inpf, unsigned char *buf);

    int gb28181_streampackageForH264(char *pData, int nFrameLen, Data_Info_s *pPacker, int stream_type);
    int gb28181_make_ps_header(char *pData, unsigned long long s64Scr);
    int gb28181_make_sys_header(char *pData);
    int gb28181_make_psm_header(char *pData);
    int gb28181_make_pes_header(char *pData, int stream_id, int payload_len, unsigned long long pts,
                                unsigned long long dts);
    int gb28181_send_rtp_pack(char *databuff, int nDataLen, int mark_flag, Data_Info_s *pPacker);
    int gb28181_make_rtp_header(char *pData, int marker_flag, unsigned short cseq, long long curpts, unsigned int ssrc);
    int SendDataBuff(const char *buff, int size);
    bool isSpite(char *buff);

    static void SendDataThread(void *arg);

  private:
    int mSockFd = -1;
    const char *mRtpServerIp;
    int mRtpServerPort = 0;
    int mLocalRtpPort = 0;

    std::thread *mThread = nullptr;
    bool mQuit;
};
#endif 