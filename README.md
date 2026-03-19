# 预编译v8

从这里整一个v8 https://github.com/puerts/backend-v8/releases/tag/V8_11.8.172__251225
然后解压到目录下，重命名目录为 v8


# 自编译v8

需要的基本目录结构

├── v8
│   └── Inc -> /home/panda/v8/v8/include
├── v8Debug
│   ├── Bin -> /home/panda/v8/v8/output/Debug/v8/Bin/
│   ├── Inc -> /home/panda/v8/v8/include/
│   └── Lib -> /home/panda/v8/v8/output/Debug/v8/Lib
└── v8Release
    ├── Bin -> /home/panda/v8/v8/output/Release/v8/Bin
    ├── Inc -> /home/panda/v8/v8/include/
    └── Lib -> /home/panda/v8/v8/output/Release/v8/Lib
