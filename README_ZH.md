## API端点

- `GET /plugins` - 列出所有插件
- `GET /plugins/{id}` - 获取特定插件信息
- `POST /plugins` - 从GitHub URL添加插件
- `DELETE /plugins/{id}` - 删除插件
- `POST /plugins/update` - 更新所有插件
## 插件开发

创建新插件：
```
./tools/plugin_ctl <plugin_name>
```

这将在plugins目录中创建一个新的插件目录和模板文件。
# MCP (Multi-Client Protocol) 插件系统

MCP插件系统是一个灵活的插件架构，允许通过动态加载的插件来扩展应用程序的功能。该系统提供了创建、管理和与插件通信的标准化接口。

## 功能特性

- 动态插件加载和卸载
- 基于JSON-RPC的通信协议
- 跨平台支持（Windows、Linux）
- 插件清单系统用于元数据管理
- 基于HTTP的插件管理接口
- 支持插件的流式响应
- GitHub集成用于插件分发
- 基于CPack的组件打包

## API接口

### 获取所有插件
```
GET /plugins
```

### 获取特定插件
```
GET /plugins/{plugin_id}
```

### 下载插件
```
GET /plugins/{plugin_id}/download
```

### 添加插件
```
POST /plugins
Content-Type: application/json

{
  "github_url": "https://github.com/owner/repo"
}
```

### 删除插件
```
DELETE /plugins/{plugin_id}
```

### 更新所有插件
```
POST /plugins/update
```

### 获取服务器自身信息
```
GET /self
```

### 检查并更新服务器自身版本
```
POST /self/update
```

## 构建

### 先决条件

- 支持C++17的编译器
- CMake 3.10或更高版本
- Git（用于克隆仓库）

### 构建步骤

1. 克隆仓库：
   ```
   git clone <repository-url>
   cd MCPPlugin
   ```

2. 创建构建目录：
   ```
   mkdir build
   cd build
   ```

3. 配置项目：
   ```
   cmake ..
   ```

4. 构建项目：
   ```
   cmake --build .
   ```

### 运行
```bash
./PluginServer [port]
./PluginServer --port 8080 --repo https://github.com/owner/repo
```
默认端口为6680。

命令行参数:
- `-p, --port <port>`: 指定服务器端口
- `-r, --repo <url>`: 指定自身仓库URL，用于获取release

## 打包

本项目使用CPack进行打包，将软件分为两个独立的组件：
1. 服务器组件：包含服务器可执行文件和plugin_ctl工具
2. 插件组件：包含插件动态库和对应的JSON配置文件

创建包：
```
cpack                 # 创建所有包
cpack -C server       # 仅创建服务器包
cpack -C plugins      # 仅创建插件包
```

有关详细打包说明，请参阅 [CPACK_USAGE_ZH.md](CPACK_USAGE_ZH.md)。
```

## 插件开发

插件需要实现以下接口：
- `get_tools_func`: 获取插件提供的工具列表
- `call_tool_func`: 调用具体工具
- `free_result_func`: 释放结果内存
- 对于流式工具，还需要实现流式处理接口

## 使用方法

1. 启动服务器：
   ```
   ./PluginServer
   ```

2. 使用插件管理API安装、删除和管理插件。

3. 插件会从plugins目录自动加载。