# MCP Plugin Server

This is a server for hosting and distributing MCP (Model Connector Plugin) plugins. The server can fetch release versions of plugins from GitHub repositories and provides APIs for clients to query and download plugins.

## Features

1. Add plugins from GitHub URL
2. Periodically update plugins
3. Provide RESTful API to query plugin information
4. Support plugin download
5. Support plugin management (add, remove, update)
6. Support server self-update from GitHub
7. Database-free design, using JSON file to store plugin information

## API Endpoints

### Get all plugins
```
GET /plugins
```

### Get a specific plugin
```
GET /plugins/{plugin_id}
```

### Download a plugin
```
GET /plugins/{plugin_id}/download
```

### Add a plugin
```
POST /plugins
Content-Type: application/json

{
  "github_url": "https://github.com/owner/repo"
}
```

### Remove a plugin
```
DELETE /plugins/{plugin_id}
```

### Update all plugins
```
POST /plugins/update
```

### Get server self information
```
GET /self
```

### Check and update server version
```
POST /self/update
```

## Build and Run

### Build
```bash
mkdir build
cd build
cmake ..
make
```

### Run
```bash
./PluginServer [port]
./PluginServer --port 8080 --repo https://github.com/owner/repo
```

The default port is 6680.

Command line arguments:
- `-p, --port <port>`: Specify server port
- `-r, --repo <url>`: Specify self repository URL for getting releases

## Plugin Format

Plugins need to follow a specific format, including tool definitions and implementations. Example plugin structure:

```json
{
  "tools": [
    {
      "name": "tool_name",
      "description": "Tool description",
      "parameters": {
        "type": "object",
        "properties": {
          "param1": {
            "type": "string",
            "description": "Parameter description"
          }
        },
        "required": ["param1"]
      }
    }
  ]
}
```

## Plugin Development

Plugins need to implement the following interfaces:
- `get_tools_func`: Get the list of tools provided by the plugin
- `call_tool_func`: Call a specific tool
- `free_result_func`: Free result memory
- For streaming tools, streaming processing interface also needs to be implemented

For detailed interface definitions, please refer to [plugin_manager.h](include/plugin_manager.h).

# MCP (Multi-Client Protocol) Plugin System

The MCP Plugin System is a flexible plugin architecture that allows extending the functionality of applications through dynamically loaded plugins. This system provides a standardized interface for creating, managing, and communicating with plugins.

## Features

- Dynamic plugin loading and unloading
- JSON-RPC based communication protocol
- Cross-platform support (Windows, Linux)
- Plugin manifest system for metadata management
- HTTP-based plugin management interface
- Support for streaming responses from plugins
- GitHub integration for plugin distribution
- CPack-based component packaging

## Building

### Prerequisites

- C++17 compatible compiler
- CMake 3.10 or higher
- Git (for cloning the repository)

### Build Steps

1. Clone the repository:
   ```
   git clone <repository-url>
   cd MCPPlugin
   ```

2. Create a build directory:
   ```
   mkdir build
   cd build
   ```

3. Configure the project:
   ```
   cmake ..
   ```

4. Build the project:
   ```
   cmake --build .
   ```

## Packaging

This project uses CPack for packaging, dividing the software into two separate components:
1. Server component: Contains the server executable and plugin_ctl tool
2. Plugins component: Contains plugin dynamic libraries and corresponding JSON configuration files

To create packages:
```
cpack                 # Create all packages
cpack -C server       # Create only server package
cpack -C plugins      # Create only plugins package
```

See [CPACK_USAGE.md](CPACK_USAGE.md) for detailed packaging instructions.

## Usage

1. Start the server:
   ```
   ./PluginServer
   ```

2. Use the plugin management API to install, remove, and manage plugins.

3. Plugins are automatically loaded from the plugins directory.

## Plugin Development

To create a new plugin:
```
./tools/plugin_ctl <plugin_name>
```

This will create a new plugin directory with template files.

## API Endpoints

- `GET /plugins` - List all plugins
- `GET /plugins/{id}` - Get specific plugin information
- `POST /plugins` - Add a plugin from GitHub URL
- `DELETE /plugins/{id}` - Remove a plugin
- `POST /plugins/update` - Update all plugins

## GitHub Workflow

The project includes GitHub Actions workflow for automated building and packaging on:
- Ubuntu 22.04
- Windows 2022

Packages are automatically created for each component and uploaded as artifacts.

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request.

## License

[License information to be added]
