#include "parser.h"
#include <yaml-cpp/yaml.h>
#include <stdexcept>

std::vector<Step> parseSteps(const std::string& yamlContent) {
    YAML::Node root = YAML::Load(yamlContent);

    if (!root["steps"]) {
        throw std::runtime_error("Missing 'steps' key in YAML");
    }

    const YAML::Node& steps = root["steps"];

    if (!steps.IsSequence()) {
        throw std::runtime_error("'steps' must be a sequence");
    }

    std::vector<Step> result;
    result.reserve(steps.size());

    for (std::size_t i = 0; i < steps.size(); ++i) {
        const YAML::Node& node = steps[i];

        Step step;
        step.name = node["name"] ? node["name"].as<std::string>() : "";

        if (!node["run"] || node["run"].as<std::string>().empty()) {
            throw std::runtime_error(
                "Step " + std::to_string(i) + " has an empty or missing 'run' field");
        }

        step.run = node["run"].as<std::string>();
        result.push_back(std::move(step));
    }

    return result;
}
