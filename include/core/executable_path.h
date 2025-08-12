// src/core/executable_path.h

#ifndef AB58D638_1980_4178_A0CD_6372D207DBCB
#define AB58D638_1980_4178_A0CD_6372D207DBCB
#include <string>

namespace mcp::core {

    // get the full path of the currently running executable
    std::string getExecutablePath();

    // get the directory of the currently running executable
    std::string getExecutableDirectory();

}// namespace mcp::core


#endif /* AB58D638_1980_4178_A0CD_6372D207DBCB */
