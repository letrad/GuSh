#include "Commands.h"
#include <iostream>
#include <unistd.h>

namespace Commands {
    std::unordered_map<std::string, std::string> envVariables;
    std::unordered_map<std::string, std::string> aliasMap;

    void cmdPwd(const std::vector<std::string>&) {
        char currentDir[256];
        if (getcwd(currentDir, sizeof(currentDir)) != nullptr) {
            std::cout << currentDir << std::endl;
        } else {
            std::cout << "Failed to get current directory" << std::endl;
        }
    }

    void cmdCd(const std::vector<std::string>& commands) {
        if (commands.size() != 1) {
            return;
        }

        const std::string& directory = commands[0];
        if (chdir(directory.c_str()) != 0) {
            std::cout << "cd: Failed to change directory to " << directory << std::endl;
        }
    }

    void cmdExport(const std::vector<std::string>& commands) {
        if (commands.size() != 3) {
            std::cout << "export: Missing variable assignment" << std::endl;
            return;
        }
        
        if (commands[1] != "=") {
            std::cout << "export: Invalid variable assignment" << std::endl;
            return;
        }

        std::string varName = commands[0];
        std::string varValue = commands[2];
        envVariables[varName] = varValue;
    }

    void cmdAlias(const std::vector<std::string>& commands) {
        if (commands.size() != 3) {
            std::cerr << "Usage: alias [alias_name]=[command]\n";
            return;
        }
        if (commands[1]!="=") {
            std::cout << "Usage: alias [alias_name]=[command]" << std::endl;
            return;
        }
        std::string varName = commands[0];
        std::string varValue = commands[2];

        aliasMap[varName] = varValue;
    }
}
