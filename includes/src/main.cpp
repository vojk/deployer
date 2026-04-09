#include "parser.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

static int runCommand(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to execute: " << cmd << std::endl;
        return 1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::cout << buffer;
        std::cout.flush();
    }

    int status = pclose(pipe);
    return WEXITSTATUS(status);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: deployer <yaml-file>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << argv[1] << std::endl;
        return 1;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    file.close();

    std::vector<Step> steps;
    try {
        steps = parseSteps(content);
    } catch (const std::runtime_error& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
        return 1;
    }

    for (const auto& step : steps) {
        std::cout << "=== Step: " << step.name << " ===" << std::endl;
        std::cout.flush();

        int exitCode = runCommand(step.run);
        if (exitCode != 0) {
            std::cerr << "Step \"" << step.name << "\" failed with exit code "
                      << exitCode << std::endl;
            return 1;
        }
    }

    std::cout << "All steps completed successfully." << std::endl;
    return 0;
}
