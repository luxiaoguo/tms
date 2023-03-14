#include "Utils/Log.h"
#include "define.h"
#include "httplib.hpp"
#include "mediaServer.h"
#include "mp4Reader.h"
#include "netTcp.h"
#include "netbase.h"
#include "rtpSendPs.h"
#include "rtspServer.h"
#include "tsPacket.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

int G_LOG_LEV = log_info;

int main(int argc, char *argv[])
{
    CMediaServer media;
    media.Start();

    CRtspServer server(5554);
    server.Start();

    httplib::Server httpServer;
    httpServer.set_mount_point("/", "./");

    httpServer.Get("/live/(.+)", [](const httplib::Request &req, httplib::Response &res) {
        std::string urn = req.matches[1];
        auto cacheLock = std::make_shared<std::mutex>();
        auto cache = std::make_shared<std::vector<uint8_t>>();
        auto write = std::make_shared<CWriter>([=](const uint8_t *pData, uint32_t size) {
            std::lock_guard<std::mutex> lock(*cacheLock);
            cache->insert(cache->end(), pData, pData + size);
        });
        res.set_header("Content-Length", std::to_string(0));
        if (!CMediaServer::Inst().CreateFlvSender(urn, write))
        {
            res.status = 404;
            return;
        }
        res.set_chunked_content_provider("application/octet-stream", [=](uint64_t offset, httplib::DataSink &sink) {
            if (!sink.is_writable)
            {
                // to release
                write->isWriteable = false;
                return false;
            }
            cacheLock->lock();
            if (cache->empty())
            {
                // LOGD("Sleep ZzzzzZ...");
                cacheLock->unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return true;
            }
            // Send the next chunk of data
            auto p = ((char *)(&(*cache)[0]));
            sink.write(p, cache->size());
            LOGD("Read %lu", cache->size());
            cache->clear();
            cacheLock->unlock();
            return true;
        });
    });
    httpServer.listen("0.0.0.0", 8090);

    return 0;
}