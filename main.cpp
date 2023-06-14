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
#include "Commands.h"

std::vector<std::string> splitCommand(const std::string& command) {
    std::vector<std::string> tokens;
    std::istringstream tokenStream(command);
    std::string token;
    while (tokenStream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void executeExternalCommand(const std::vector<std::string>& commands) {
    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "Fork failed!\n";
        return;
    } else if (pid == 0) {  // Child process
        signal(SIGINT, SIG_DFL);  // Reset SIGINT handler

        std::vector<char*> cstyle_commands;
        for (const auto& cmd : commands) {
            cstyle_commands.push_back(const_cast<char*>(cmd.c_str()));
        }
        cstyle_commands.push_back(nullptr);

        execvp(cstyle_commands[0], cstyle_commands.data());

        std::cerr << "Execvp failed!\n";
        return;
    } else {  // Parent process
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            std::cerr << "Child process error code: " << WEXITSTATUS(status) << "\n";
        } else if (WIFSIGNALED(status)) {
            std::cerr << "Child process killed by signal: " << WTERMSIG(status) << "\n";
        }
    }
}



std::vector<std::string> getCommandMatches(const char* text) {
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
    return matches;
}

typedef char** (*CompletionFunc)(const char*, int, int);

char** commandCompletion(const char* text, int start, int end) {
    if (start != 0) {
        return nullptr;
    }

    rl_attempted_completion_over = 1;
    std::vector<std::string> matches = getCommandMatches(text);

    if (matches.empty()) {
        return nullptr;
    }

    char** completionMatches = rl_completion_matches(text, [](const char* text, int state) -> char* {
        static size_t index = 0;
        if (state == 0) {
            index = 0;
        }

        std::vector<std::string> matches = getCommandMatches(text);

        if (index < matches.size()) {
            return strdup(matches[index++].c_str());
        }

        return nullptr;
    });

    return completionMatches;
}

int main() {
    // Just to stop the shell from being ^C-ed
    signal(SIGINT, SIG_IGN);
    try {
        rl_attempted_completion_function = reinterpret_cast<CompletionFunc>(commandCompletion);

        std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>> commandMap = {
                {"exit", [](const std::vector<std::string>&) {
                    exit(0);
                }},
                {"pwd", Commands::cmdPwd},
                {"cd", Commands::cmdCd},
        };

        char* input;
        while ((input = readline("> ")) != nullptr) {
            add_history(input);

            std::string command(input);
            std::vector<std::string> commands = splitCommand(command);

            const std::string& cmd = commands[0];
            auto it = commandMap
                    .find(cmd);
            if (it != commandMap.end()) {
                it->second(commands);
            } else {
                executeExternalCommand(commands);
            }

            free(input);
        }
        free(input); // In case the program exits normally, free the memory.
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
