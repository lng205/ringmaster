# Ringmaster

[Ringmaster](https://github.com/microsoft/ringmaster) 是一个视频会议研究平台，由微软研究院开发。本项目在Ringmaster的基础上，实现了FEC（Forward Error Correction）方案。

## 使用方法

1. 安装依赖
```
sudo apt install libvpx-dev libsdl2-dev libjerasure-dev
```

2. 编译
```
cmake -S src -B build
cmake --build build
```

3. 运行
```
./build/sender 12345 ice_4cif_30fps.y4m
./build/receiver 127.0.0.1 12345 704 576 --fps 30 --cbr 500
```

这模拟了一次1:1的视频通话，发送端压缩视频帧并发送，接收端解码视频帧并显示。
输入的视频格式为y4m，分辨率为704x576，帧率为30fps。编码方式为VP9，码率为500kbps。

测试视频：[ice_4cif_30fps.y4m](https://media.xiph.org/video/derf/y4m/ice_4cif_30fps.y4m)
（该域名同时也提供其他测试视频）