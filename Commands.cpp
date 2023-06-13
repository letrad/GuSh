#include "Commands.h"
#include <iostream>
#include <unistd.h>

namespace Commands {
    void cmdPwd(const std::vector<std::string>&) {
        char currentDir[256];
        if (getcwd(currentDir, sizeof(currentDir)) != nullptr) {
            std::cout << currentDir << std::endl;
        } else {
            std::cout << "Failed to get current directory" << std::endl;
        }
    }

    void cmdCd(const std::vector<std::string>& commands) {
        if (commands.size() < 2) {
            std::cout << "cd: Missing directory argument" << std::endl;
            return;
        }

        const std::string& directory = commands[1];
        if (chdir(directory.c_str()) == 0) {
        } else {
            std::cout << "cd: Failed to change directory to " << directory << std::endl;
        }
    }
}
