# CPack Packaging Instructions

## Overview

This project uses CPack for packaging, dividing the software into two separate components:

1. **plugins component**: Contains only plugin dynamic library files (.dll or .so) and corresponding JSON configuration files
2. **server component**: Contains the server executable and the plugin_ctl tool

## Build and Packaging Steps

1. Create a build directory and enter it:
   ```bash
   mkdir build
   cd build
   ```

2. Configure the project:
   ```bash
   cmake ..
   ```

3. Build the project:
   ```bash
   cmake --build .
   ```

4. Package all components into one package:
   ```bash
   cpack
   ```

5. Or package each component separately:
   ```bash
   cpack -C plugins    # Package only the plugins component
   cpack -C server     # Package only the server component
   ```

## Generated Package Files

- `MCPPlugin-1.0.0-<platform>.zip` - Complete package (contains all components)
- `MCPPlugin-1.0.0-<platform>-plugins.zip` - Plugins component package
- `MCPPlugin-1.0.0-<platform>-server.zip` - Server component package

## Package Contents

### plugins component package
```
bin/
└── plugins/
    ├── example_plugin.dll (or example_plugin.so)
    └── configs/
        └── example_plugin_tools.json
```

### server component package
```
bin/
├── PluginServer.exe (or PluginServer)
├── plugin_ctl.exe (or plugin_ctl)
└── configs/
    └── example_plugin_tools.json
```

## GitHub Workflow

The project is configured with a GitHub workflow that automatically builds and packages in the following cases:
1. Push to main/master branch
2. Create a Pull Request to main/master branch
3. Create a tag starting with "v" (e.g. v1.0.0)

The workflow performs builds and packaging on two platforms:
- Ubuntu 22.04
- Windows 2022

Each platform generates two separate packages:
1. Server package: Contains the server executable and plugin_ctl tool
2. Plugins package: Contains plugin dynamic libraries and corresponding JSON configuration files

Build artifacts can be downloaded through GitHub Actions Artifacts.

### Release Publishing

When a tag starting with "v" is created (e.g. `v1.0.0`), the workflow automatically creates a Release and uploads all packaged components to the Release.

## Notes

1. The plugins component package contains only the plugin dynamic library files and corresponding JSON configuration files, without the server and control tools
2. The server component package contains all files needed to run the server, but does not contain plugin dynamic library files
3. The two components can be installed and updated independently