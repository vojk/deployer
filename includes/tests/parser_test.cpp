#include "parser.h"
#include <gtest/gtest.h>

TEST(ParseSteps, ValidMultiStepYaml) {
    const std::string yaml = R"(
steps:
  - name: "Greet"
    run: "echo hello"
  - name: "List"
    run: "ls -la"
)";

    auto steps = parseSteps(yaml);
    ASSERT_EQ(steps.size(), 2);

    EXPECT_EQ(steps[0].name, "Greet");
    EXPECT_EQ(steps[0].run, "echo hello");

    EXPECT_EQ(steps[1].name, "List");
    EXPECT_EQ(steps[1].run, "ls -la");
}

TEST(ParseSteps, MissingStepsKey) {
    const std::string yaml = R"(
jobs:
  - name: "Greet"
    run: "echo hello"
)";

    EXPECT_THROW(parseSteps(yaml), std::runtime_error);
}

TEST(ParseSteps, StepsNotASequence) {
    const std::string yaml = R"(
steps: "not a sequence"
)";

    EXPECT_THROW(parseSteps(yaml), std::runtime_error);
}

TEST(ParseSteps, EmptyRunField) {
    const std::string yaml = R"(
steps:
  - name: "Bad step"
    run: ""
)";

    EXPECT_THROW(parseSteps(yaml), std::runtime_error);
}

TEST(ParseSteps, MissingRunField) {
    const std::string yaml = R"(
steps:
  - name: "No run"
)";

    EXPECT_THROW(parseSteps(yaml), std::runtime_error);
}

TEST(ParseSteps, EmptyStepsArray) {
    const std::string yaml = R"(
steps: []
)";

    auto steps = parseSteps(yaml);
    EXPECT_TRUE(steps.empty());
}

TEST(ParseSteps, MissingNameFieldDefaultsToEmpty) {
    const std::string yaml = R"(
steps:
  - run: "echo anonymous"
)";

    auto steps = parseSteps(yaml);
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0].name, "");
    EXPECT_EQ(steps[0].run, "echo anonymous");
}
