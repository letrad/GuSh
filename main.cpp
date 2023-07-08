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
#include <regex>
#include <limits.h>
#include <pwd.h>
#include <filesystem>
#include <fstream>
#include <signal.h>
#include "Commands.h"
#include "Lexer.h"
#include <glob.h>
#include <algorithm>
#include <memory>
#include <utility>
#include <mutex>
#include <unistd.h>

const char *getHomeDirectory() {
    return getpwuid(getuid())->pw_dir;
}

std::string shortenPath(const std::string &path) {
    std::string shortenedPath = path;
    const char *homedir = getHomeDirectory();

    if (path.find(homedir) == 0) {
        shortenedPath.replace(0, std::string(homedir).length(), "~");
    }

    return shortenedPath;
}

void handleToken(const std::string &token, std::vector<std::string> &tokens) {
    if (!token.empty()) {
        if (token[0] == '$') {
            auto it = Commands::envVariables.find(token.substr(1));
            if (it != Commands::envVariables.end()) {
                tokens.push_back(it->second);
            }
        } else if (token == "~") {
            tokens.push_back(getHomeDirectory());
        } else {
            tokens.push_back(token);
        }
    }
}

std::string exSubstitute(const std::string &command) {
    char buffer[128];
    std::string result = "";

    // Open pipe to file
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "popen failed!";
    }

    // Read till end of process:
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }

    pclose(pipe);
    return result;
}

std::vector<std::vector<std::string>> splitCommand(const std::string &command) {
    std::vector<std::vector<std::string>> commands;
    std::vector<std::string> tokens, expanded;
    std::string token; //See Lexer.cpp
    size_t idx = 0;

    std::string commandWithAliasesReplaced = command;
    for (const auto &alias: Commands::aliasMap) {
        std::string pattern = "(\\b)" + alias.first + "(\\b)";  // \b represents a word boundary
        std::regex r(pattern);
        commandWithAliasesReplaced = std::regex_replace(commandWithAliasesReplaced, r, alias.second);
    }

    try {
        while (idx < command.size()) {
            token = Lexer::LexItem(command, idx);
            expanded = Lexer::ExpandToken(token);
            for (auto token: expanded) {
                if (token == "|") {
                    commands.push_back(tokens);
                    tokens.clear();
                } else {
                    handleToken(token, tokens);
                }
            }
        }
    } catch (Lexer::CLexerExcept lexx) {
        std::cerr << lexx.message << std::endl;
        commands.clear();
        return commands;
    }

    commands.push_back(tokens);

    return commands;
}

static int exWithEnv(const char *command, const std::vector<std::string> &cmds) {
    std::vector<char *> cstyle_commands;
    for (const auto &cmd: cmds) {
        cstyle_commands.push_back(const_cast<char *>(cmd.c_str()));
    }
    cstyle_commands.push_back(nullptr);

    //Make enviroment array
    std::vector<char *> cstyle_env;
    //We will need to allocate the tuples in the format "VAR=value"
    std::vector<std::string> alloced_var_tuples;
#if defined (__linux__)
    //Here's the deal const char **environ; contains "var=val" stuff,I will make a "clone"
    //of existing ones,then I will add our stuff from Commands::envVariables
    //man 7 environ
    extern char **environ;
    for (size_t i = 0; environ[i]; i++)
        cstyle_env.push_back(environ[i]);
    for (const auto &it: Commands::envVariables) {
        std::string dummy = it.first;
        dummy += "=";
        dummy += it.second;
        alloced_var_tuples.push_back(dummy);
        cstyle_env.push_back(const_cast<char *>(dummy.c_str()));
    }
    cstyle_env.push_back(nullptr);

    return execvpe(command, cstyle_commands.data(), cstyle_env.data());
#elif defined (__FreeBSD__)
    for(const auto& it: Commands::envVariables) {
        const char *exist;
        if((exist=getenv(it.first.c_str()))) {
            if(strcmp(exist,it.second.c_str()))
                goto set;
        } else {
set:
            setenv(it.first.c_str(),it.second.c_str(),1);
        }
    }
    return execvp(command, cstyle_commands.data());
#else
#error "Platform not supported"
#endif
}

void exExternal(const std::vector<std::string> &commands) {
    if (commands.empty()) {
        std::cerr << "GuSH: No command provided\n";
        return;
    }

    // Check if the command is an alias
    std::vector<std::string> finalCommands;
    auto aliasIter = Commands::aliasMap.find(commands[0]);
    if (aliasIter != Commands::aliasMap.end()) {
        // Command is an alias, replace it with the actual command
        std::istringstream ss(aliasIter->second);
        std::string token;
        while (std::getline(ss, token, ' ')) {
            finalCommands.push_back(token);
        }
        // Add any arguments
        for (size_t i = 1; i < commands.size(); i++) {
            finalCommands.push_back(commands[i]);
        }
    } else {
        // Command is not an alias, execute as normal
        finalCommands = commands;
    }

    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "Fork failed!\n";
        return;
    } else if (pid == 0) {  // Child process
        signal(SIGINT, SIG_DFL);  // Reset SIGINT handler

        exWithEnv(const_cast<const char *>(commands[0].c_str()), finalCommands);

        // If execvp returns, an error occurred.
        std::cerr << "GuSH: The specified command could not be found: " << finalCommands[0] << "\n";
        exit(EXIT_FAILURE);  // End the child process.
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


void exPipedCommands(std::vector<std::vector<std::string>> &commands) {
    int pipefds[2 * commands.size() - 2];
    for (int i = 0; i < commands.size() - 1; ++i) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("couldn't pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < commands.size(); ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("error in fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // If not last command
            if (i < commands.size() - 1) {
                if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            // If not first command && j!= 2*numPipes
            if (i != 0) {
                if (dup2(pipefds[i * 2 - 2], STDIN_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            for (int j = 0; j < 2 * commands.size() - 2; ++j)
                close(pipefds[j]);

            if (exWithEnv(const_cast<const char *>(commands[i][0].c_str()), commands[i]) < 0) {
                perror(commands[i][0].c_str());
                exit(EXIT_FAILURE);
            }
        }
    }

    for (int i = 0; i < 2 * commands.size() - 2; ++i)
        close(pipefds[i]);

    for (int i = 0; i < commands.size(); ++i)
        wait(NULL);
}


std::vector<std::string> getCommandMatches(const char *text) {
    std::vector<std::string> matches;
    if (!strlen(text)) return matches;
    std::string pathEnv = std::getenv("PATH");
    std::istringstream pathStream(pathEnv);
    std::string path;
    while (getline(pathStream, path, ':')) {
        try {
            if (std::filesystem::exists(path))
                for (const auto &entry: std::filesystem::directory_iterator(path)) {
                    //Here's the deal,a user may have a invalid path in their directory,do ignore it
                    const std::string &filename = entry.path().filename().string();
                    if (filename.find(text) == 0) {
                        matches.push_back(filename);
                    }
                }
        } catch (const std::filesystem::filesystem_error &e) {
            std::cerr << "GuSH: Filesystem error: " << e.what() << std::endl;
        }
    }
    return matches;
}

std::vector<std::string> getDirMatches(const char *path) {
    size_t cnt, dir_sz;
    bool expanded_home_dir = false;
    std::vector<std::string> matches;
    std::string pat = path;
    if (!pat.size()) return matches;
    if (0 == pat.find("~/")) {
        expanded_home_dir = true;
        dir_sz = std::string(getHomeDirectory()).size();
        pat.replace(0, 1, getHomeDirectory());
    }
    if (std::filesystem::is_directory(pat)) {
        if (pat.back() == '/')
            pat += "*";
    } else if (pat.back() != '*')
        pat += "*";
    glob_t gl;
    gl.gl_offs = 0;
    glob(pat.c_str(), GLOB_DOOFFS, NULL, &gl);
    if (gl.gl_pathv)
        for (cnt = 0; gl.gl_pathv[cnt]; cnt++) {
            matches.push_back(gl.gl_pathv[cnt]);
            if (expanded_home_dir)
                matches.back().replace(0, dir_sz, "~");
        }
    globfree(&gl);
    return matches;
}

typedef char **(*CompletionFunc)(const char *, int, int);

//Force a "C" ABI for libreadline api
extern "C" {
char **gushCompletion(const char *text, int start, int end) {
    if (start != 0 && !strlen(text)) {
        return nullptr;
    }

    rl_attempted_completion_over = 1;
    std::vector<std::string> pmatches = getCommandMatches(text);
    std::vector<std::string> dmatches = getDirMatches(text);
    pmatches.insert(pmatches.end(), dmatches.begin(), dmatches.end());

    std::sort(pmatches.begin(), pmatches.end());

    if (pmatches.empty()) {
        return nullptr;
    }

    static std::shared_ptr<std::vector<std::string>> precomputed = nullptr;
    precomputed = std::make_shared<std::vector<std::string>>(pmatches);

    char **completionMatches = rl_completion_matches(text, [](const char *text, int state) -> char * {
        static size_t index = 0;
        if (state == 0) {
            index = 0;
        }

        if (index < precomputed->size()) {
            return strdup(precomputed->at(index++).c_str());
        }

        return nullptr;
    });

    precomputed.reset();

    return completionMatches;
}
};

std::unordered_map<std::string, std::function<void(const std::vector<std::string> &)>> commandMap = {
        {"exit",   [](const std::vector<std::string> &) {
            exit(0);
        }},
        {"pwd",    Commands::cmdPwd},
        {"cd",     Commands::cmdCd},
        {"export", Commands::cmdExport},
        {"alias",  Commands::cmdAlias}
};

std::string exCmd(const std::string &command) {
    std::string modifiedCommand = command;

    // New code to handle command substitution
    std::size_t pos = command.find("$(");
    while (pos != std::string::npos) {
        std::size_t endPos = command.find(")", pos);
        if (endPos != std::string::npos) {
            std::string innerCommand = command.substr(pos + 2, endPos - pos - 2);
            std::string substitution = exSubstitute(innerCommand);
            modifiedCommand.replace(pos, endPos - pos + 1, substitution);
        }
        pos = command.find("$(", pos + 1);
    }

    std::vector<std::vector<std::string>> commands = splitCommand(modifiedCommand);

    if (commands.size() > 1) {
        exPipedCommands(commands);
    } else if (commands.size()) {
        if (!commands[0].empty()) {
            const std::string &cmd = commands[0][0];
            auto it = commandMap.find(cmd);
            if (it != commandMap.end()) {
                //First item in commands is the command name,so i will remove it as I only want the arguments
                commands[0].erase(commands[0].begin());
                it->second(commands[0]);
            } else {
                exExternal(commands[0]);
            }
        }
    }

    return "";
}

void exConfig() {
    std::string configFilePath = std::string(getHomeDirectory()) + "/.config.gush";
    std::ifstream configFile(configFilePath);

    if (configFile.is_open()) {
        std::string line;
        while (std::getline(configFile, line)) {
            exCmd(line);
        }
        configFile.close();
    } else {
        std::cerr << "Unable to open .config.gush file" << std::endl;
    }
}

int main() {
    // Just to stop the shell from being ^C-ed
    signal(SIGINT, SIG_IGN);
    try {
        const char *homedir = getHomeDirectory();
        std::string historyPath = std::string(homedir) + "/.gushHis";

        rl_attempted_completion_function = gushCompletion;

        if (read_history(historyPath.c_str()) != 0) {
            if (errno != ENOENT) {
                std::cerr << "Error reading history file: " << historyPath << "\n";
            }
        }
        exConfig();
        char *input;
        while (true) {
            char currentDir[PATH_MAX];
            if (getcwd(currentDir, sizeof(currentDir)) != nullptr) {
                // Just make it look like robbyrussel while I work on things
                std::string prompt =
                        "\u001b[1m\u001b[96m" + std::string(shortenPath(currentDir)) + " \u001b[32m➜\033[0m ";
                input = readline(prompt.c_str());
            } else {
                std::cerr << "Failed to get current directory" << std::endl;
                break;
            }

            add_history(input);

            if (write_history(historyPath.c_str()) != 0) {
                std::cerr << "Error writing history file: " << historyPath << "\n";
            }

            exCmd(input);
            free(input);
        }

        free(input); // In case the program exits normally, free the memory.
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
