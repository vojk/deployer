#include "parser.h"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static std::string g_socketPath;

static void cleanup(int) {
    if (!g_socketPath.empty()) unlink(g_socketPath.c_str());
    _exit(0);
}

static std::string readAll(int fd) {
    std::string data;
    char buf[4096];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) data.append(buf, n);
        else break;
    }
    return data;
}

static void sendStr(int fd, const std::string& s) {
    const char* p = s.data();
    size_t rem = s.size();
    while (rem > 0) {
        ssize_t n = write(fd, p, rem);
        if (n <= 0) break;
        p += n;
        rem -= n;
    }
}

static void handleClient(int clientFd) {
    std::string yaml = readAll(clientFd);

    std::vector<Step> steps;
    try {
        steps = parseSteps(yaml);
    } catch (const std::runtime_error& e) {
        std::string msg = std::string("Parse error: ") + e.what() + "\n";
        sendStr(clientFd, msg);
        sendStr(clientFd, "\n[EXIT CODE: 1]\n");
        close(clientFd);
        return;
    }

    for (const auto& step : steps) {
        std::string header = "=== Step: " + step.name + " ===\n";
        sendStr(clientFd, header);

        FILE* pipe = popen(step.run.c_str(), "r");
        if (!pipe) {
            std::string err = "Failed to execute: " + step.run + "\n";
            sendStr(clientFd, err);
            sendStr(clientFd, "\n[EXIT CODE: 1]\n");
            close(clientFd);
            return;
        }

        char buf[256];
        while (fgets(buf, sizeof(buf), pipe) != nullptr) {
            sendStr(clientFd, std::string(buf));
        }

        int status = pclose(pipe);
        int exitCode = WEXITSTATUS(status);
        if (exitCode != 0) {
            std::string err = "Step \"" + step.name + "\" failed with exit code " +
                              std::to_string(exitCode) + "\n";
            sendStr(clientFd, err);
            sendStr(clientFd, "\n[EXIT CODE: " + std::to_string(exitCode) + "]\n");
            close(clientFd);
            return;
        }
    }

    sendStr(clientFd, "All steps completed successfully.\n");
    close(clientFd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: deployer <socket-path>" << std::endl;
        return 1;
    }

    g_socketPath = argv[1];

    unlink(g_socketPath.c_str());

    int serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd < 0) {
        std::cerr << "socket(): " << strerror(errno) << std::endl;
        return 1;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(serverFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind(): " << strerror(errno) << std::endl;
        close(serverFd);
        return 1;
    }

    if (listen(serverFd, 4) < 0) {
        std::cerr << "listen(): " << strerror(errno) << std::endl;
        close(serverFd);
        return 1;
    }

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    std::cout << "Deployer listening on " << g_socketPath << std::endl;

    for (;;) {
        int clientFd = accept(serverFd, nullptr, nullptr);
        if (clientFd < 0) {
            if (errno == EINTR) continue;
            std::cerr << "accept(): " << strerror(errno) << std::endl;
            continue;
        }
        handleClient(clientFd);
    }
}
