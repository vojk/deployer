#include "parser.h"
#include <yaml-cpp/yaml.h>
#include <stdexcept>

std::vector<Variable> parseVariables(const std::string &yamlContent) {
    YAML::Node root = YAML::Load(yamlContent);
    if (!root["vars"]) {
        return {};
    }

    YAML::Node vars = root["vars"];
    if (!vars.IsSequence()) {
        throw std::runtime_error("'vars' must be a sequence");
    }

    std::vector<Variable> result;
    result.reserve(vars.size());

    for (std::size_t i = 0; i < vars.size(); ++i) {
        const YAML::Node &node = vars[i];
        Variable variable;
        variable.name = node["name"].as<std::string>();
        if (!node["value"] || node["value"].as<std::string>().empty()) {
            variable.value = "";
        }
        variable.value = node["value"].as<std::string>();

        result.push_back(variable);
    }

    return result;
}

std::vector<Step> parseSteps(const std::string &yamlContent, const std::vector<Variable> &variables) {
    YAML::Node root = YAML::Load(yamlContent);

    if (!root["steps"]) {
        throw std::runtime_error("Missing 'steps' key in YAML");
    }

    const YAML::Node &steps = root["steps"];

    if (!steps.IsSequence()) {
        throw std::runtime_error("'steps' must be a sequence");
    }

    std::vector<Step> result;
    result.reserve(steps.size());

    for (std::size_t i = 0; i < steps.size(); ++i) {
        const YAML::Node &node = steps[i];

        Step step;
        step.name = node["name"] ? node["name"].as<std::string>() : "";

        if (!node["run"] || node["run"].as<std::string>().empty()) {
            throw std::runtime_error(
                "Step " + std::to_string(i) + " has an empty or missing 'run' field");
        }

        step.run = node["run"].as<std::string>();

        if (!variables.empty()) {
            for (const auto &variable: variables) {
                std::string placeholder = "{{!" + variable.name + "!}}";
                std::size_t placeholderLen = placeholder.length();
                std::size_t index = 0;

                while ((index = step.run.find(placeholder, index)) != std::string::npos) {
                    step.run.replace(index, placeholderLen, variable.value);
                    index += variable.value.length();
                }
            }
        }

        result.push_back(std::move(step));
    }

    return result;
}
