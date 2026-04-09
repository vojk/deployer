#pragma once

#include <string>
#include <vector>

struct Step {
    std::string name;
    std::string run;
};

// Parse a YAML string and extract the "steps" sequence.
// Throws std::runtime_error if:
//   - the "steps" key is missing or not a sequence
//   - any step has an empty "run" field
std::vector<Step> parseSteps(const std::string& yamlContent);
