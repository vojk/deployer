#pragma once

#include <string>
#include <vector>

struct Step {
    std::string name;
    std::string run;
};

struct Variable {
    std::string name;
    std::string value;
};

// Parse a YAML string and extract the "steps" sequence.
// Throws std::runtime_error if:
//   - the "steps" key is missing or not a sequence
//   - any step has an empty "run" field
std::vector<Step> parseSteps(const std::string& yamlContent, const std::vector<Variable>& variables);

std::vector<Variable> parseVariables(const std::string& yamlContent);
