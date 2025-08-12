# CPack 打包使用说明

## 概述

本项目使用CPack进行打包，将软件分为两个独立的组件：

1. **plugins组件**：仅包含插件动态库文件（.dll或.so）和对应的JSON配置文件
2. **server组件**：包含服务器可执行文件和plugin_ctl工具

## 构建和打包步骤

1. 创建构建目录并进入：
   ```bash
   mkdir build
   cd build
   ```

2. 配置项目：
   ```bash
   cmake ..
   ```

3. 构建项目：
   ```bash
   cmake --build .
   ```

4. 打包所有组件为一个包：
   ```bash
   cpack
   ```

5. 或者分别打包各个组件：
   ```bash
   cpack -C plugins    # 只打包插件组件
   cpack -C server     # 只打包服务器组件
   ```

## 生成的包文件

- `MCPPlugin-1.0.0-<platform>.zip` - 完整包（包含所有组件）
- `MCPPlugin-1.0.0-<platform>-plugins.zip` - 插件组件包
- `MCPPlugin-1.0.0-<platform>-server.zip` - 服务器组件包

## 包内容说明

### plugins组件包
```
bin/
└── plugins/
    ├── example_plugin.dll (或 example_plugin.so)
    └── configs/
        └── example_plugin_tools.json
```

### server组件包
```
bin/
├── PluginServer.exe (或 PluginServer)
├── plugin_ctl.exe (或 plugin_ctl)
└── configs/
    └── example_plugin_tools.json
```

## GitHub工作流

项目配置了GitHub工作流，会在以下情况下自动构建和打包：
1. 推送到main/master分支
2. 创建Pull Request到main/master分支
3. 创建以"v"开头的标签（如v1.0.0）

工作流会在两个平台上进行构建和打包：
- Ubuntu 22.04
- Windows 2022

每个平台都会生成两个独立的包：
1. 服务器包：包含服务器可执行文件和plugin_ctl工具
2. 插件包：包含插件动态库和对应的JSON配置文件

构建产物可以通过GitHub Actions Artifacts下载。

### Release发布

当创建以"v"开头的标签时（如`v1.0.0`），工作流会自动创建Release，并将所有打包好的组件上传到Release中。

## 注意事项

1. 插件组件包只包含插件的动态库文件和对应的JSON配置文件，不包含服务器和控制工具
2. 服务器组件包包含运行服务器所需的所有文件，但不包含插件动态库文件
3. 两个组件可以独立安装和更新