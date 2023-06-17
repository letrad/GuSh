#ifndef COMMANDS_H
#define COMMANDS_H

#include <vector>
#include <string>
#include <unordered_map>

namespace Commands {
    extern std::unordered_map<std::string, std::string> envVariables;
    extern std::unordered_map<std::string, std::string> aliasMap;  // for storing aliases

    void cmdPwd(const std::vector<std::string>& commands);
    void cmdCd(const std::vector<std::string>& commands);
    void cmdExport(const std::vector<std::string>& commands);
    void cmdAlias(const std::vector<std::string>& commands);
}

#endif
