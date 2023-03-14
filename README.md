# Tiny Media Server

#### 本项目实现了一个流媒体简易的流媒体直播、点播服务器，支持RTSP,HTTP-FLV,HTTP-HLS分发



## 编译及运行

```shell
mkdir build
cd build
cmake ..
make
```



## 依赖

- **FFMPEG**: 文件读取

- **cpp-httplib**: hls分发及flv分发



### 主要功能

#### RTSP收流

支持RTSP信令协商，接收客户端推流，解包组帧



#### 文件点播

支持rtsp点播mp4文件，目前支持H.264,H.265,AAC格式，支持暂停、Seek、倍速等功能



#### HLS分发

对收到的RTSP流进行处理，生成TS切片，进行HLS分发，可多路并发



#### FLV分发

对收到的RTSP流进行处理，生成FLV视频流，进行分发