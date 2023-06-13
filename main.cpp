#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <functional>
#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <filesystem>

std::vector<std::string> splitCommand(const std::string& command) {
    std::vector<std::string> tokens;
    std::istringstream tokenStream(command);
    std::string token;
    while (tokenStream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void executePwd(const std::vector<std::string>&) {
    char currentDir[256];
    if (getcwd(currentDir, sizeof(currentDir)) != nullptr) {
        std::cout << currentDir << std::endl;
    } else {
        std::cout << "Failed to get current directory" << std::endl;
    }
}

void executeCd(const std::vector<std::string>& commands) {
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

void executeExternalCommand(const std::vector<std::string>& commands) {
    std::string command;
    for (const auto& cmd : commands) {
        command += cmd + " ";
    }

    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        std::cout << "Failed to execute command" << std::endl;
        return;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::cout << buffer;
    }

    pclose(pipe);
}

typedef char** (*CompletionFunc)(const char*, int, int);

char** commandCompletion(const char* text, int start, int end) {
    if (start != 0) {
        return nullptr;
    }

    rl_attempted_completion_over = 1;

    std::vector<std::string> matches;

    // Add system commands to matches
    std::string pathEnv = std::getenv("PATH");
    std::istringstream pathStream(pathEnv);
    std::string path;
    while (getline(pathStream, path, ':')) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                const std::string& filename = entry.path().filename().string();
                if (filename.find(text) == 0) {
                    matches.push_back(filename);
                }
            }
        } catch (const std::filesystem::filesystem_error&) {
            // Handle any filesystem errors and continue to the next path
        }
    }

    if (matches.empty()) {
        return nullptr;
    }

    char** completionMatches = rl_completion_matches(text, [](const char* text, int state) -> char* {
        static size_t index = 0;
        if (state == 0) {
            index = 0;
        }

        std::vector<std::string> matches;

        std::string pathEnv = std::getenv("PATH");
        std::istringstream pathStream(pathEnv);
        std::string path;
        while (getline(pathStream, path, ':')) {
            try {
                for (const auto& entry : std::filesystem::directory_iterator(path)) {
                    const std::string& filename = entry.path().filename().string();
                    if (filename.find(text) == 0) {
                        matches.push_back(filename);
                    }
                }
            } catch (const std::filesystem::filesystem_error&) {
                // Handle any filesystem errors and continue to the next path
            }
        }

        if (index < matches.size()) {
            return strdup(matches[index++].c_str());
        }

        return nullptr;
    });

    return completionMatches;
}

int main() {
    rl_attempted_completion_function = reinterpret_cast<CompletionFunc>(commandCompletion);

    std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>> commandMap = {
            {"exit", [](const std::vector<std::string>&) {
                exit(0);
            }},
            {"pwd", executePwd},
            {"cd", executeCd},
            // Add more commands to the map here
    };

    char* input;
    while ((input = readline("> ")) != nullptr) {
        add_history(input);

        std::string command(input);
        std::vector<std::string> commands = splitCommand(command);

        const std::string& cmd = commands[0];
        auto it = commandMap.find(cmd);
        if (it != commandMap.end()) {
            it->second(commands);
        } else {
            executeExternalCommand(commands);
        }

        free(input);
    }

    return 0;
}
