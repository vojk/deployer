#include "parser.h"
#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// parseVariables
// ---------------------------------------------------------------------------

TEST(ParseVariables, NoVarsSection) {
    const std::string yaml = R"(
steps:
  - name: "Test"
    run: "echo hi"
)";

    auto vars = parseVariables(yaml);
    EXPECT_TRUE(vars.empty());
}

TEST(ParseVariables, ValidVars) {
    const std::string yaml = R"(
vars:
  - name: "FOO"
    value: "bar"
  - name: "NUM"
    value: "42"
)";

    auto vars = parseVariables(yaml);
    ASSERT_EQ(vars.size(), 2);
    EXPECT_EQ(vars[0].name, "FOO");
    EXPECT_EQ(vars[0].value, "bar");
    EXPECT_EQ(vars[1].name, "NUM");
    EXPECT_EQ(vars[1].value, "42");
}

TEST(ParseVariables, VarsNotASequence) {
    const std::string yaml = R"(
vars: "not a sequence"
)";

    EXPECT_THROW(parseVariables(yaml), std::runtime_error);
}

TEST(ParseVariables, EmptyVarsArray) {
    const std::string yaml = R"(
vars: []
)";

    auto vars = parseVariables(yaml);
    EXPECT_TRUE(vars.empty());
}

TEST(ParseVariables, VarWithEmptyValue) {
    const std::string yaml = R"(
vars:
  - name: "EMPTY"
    value: ""
)";

    auto vars = parseVariables(yaml);
    ASSERT_EQ(vars.size(), 1);
    EXPECT_EQ(vars[0].name, "EMPTY");
    EXPECT_EQ(vars[0].value, "");
}

TEST(ParseVariables, VarWithSpecialCharacters) {
    const std::string yaml = R"(
vars:
  - name: "URL"
    value: "https://example.com/path?q=1&x=2"
)";

    auto vars = parseVariables(yaml);
    ASSERT_EQ(vars.size(), 1);
    EXPECT_EQ(vars[0].value, "https://example.com/path?q=1&x=2");
}

// ---------------------------------------------------------------------------
// parseSteps – basic
// ---------------------------------------------------------------------------

TEST(ParseSteps, ValidMultiStepYaml) {
    const std::string yaml = R"(
vars:
  - name: "TEST"
    value: "imagine"
  - name: "AHOJ"
    value: "televize"

steps:
  - name: "Greet"
    run: "echo hello"
  - name: "List"
    run: "ls -la"
  - name: "Trala"
    run: "test {{!TEST!}}"
  - name: "Trala"
    run: "test {{!AHOJ!}}"
)";

    auto vars = parseVariables(yaml);
    ASSERT_EQ(vars.size(), 2);

    auto steps = parseSteps(yaml, vars);
    ASSERT_EQ(steps.size(), 4);

    EXPECT_EQ(steps[0].run, "echo hello");
    EXPECT_EQ(steps[1].run, "ls -la");
    EXPECT_EQ(steps[2].run, "test imagine");
    EXPECT_EQ(steps[3].run, "test televize");
}

TEST(ParseSteps, MissingStepsKey) {
    const std::string yaml = R"(
jobs:
  - name: "Greet"
    run: "echo hello"
)";

    EXPECT_THROW(parseSteps(yaml, {}), std::runtime_error);
}

TEST(ParseSteps, StepsNotASequence) {
    const std::string yaml = R"(
steps: "not a sequence"
)";

    EXPECT_THROW(parseSteps(yaml, {}), std::runtime_error);
}

TEST(ParseSteps, EmptyRunField) {
    const std::string yaml = R"(
steps:
  - name: "Bad step"
    run: ""
)";

    EXPECT_THROW(parseSteps(yaml, {}), std::runtime_error);
}

TEST(ParseSteps, MissingRunField) {
    const std::string yaml = R"(
steps:
  - name: "No run"
)";

    EXPECT_THROW(parseSteps(yaml, {}), std::runtime_error);
}

TEST(ParseSteps, EmptyStepsArray) {
    const std::string yaml = R"(
steps: []
)";

    auto steps = parseSteps(yaml, {});
    EXPECT_TRUE(steps.empty());
}

TEST(ParseSteps, MissingNameFieldDefaultsToEmpty) {
    const std::string yaml = R"(
steps:
  - run: "echo anonymous"
)";

    auto steps = parseSteps(yaml, {});
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0].name, "");
    EXPECT_EQ(steps[0].run, "echo anonymous");
}

// ---------------------------------------------------------------------------
// parseSteps – variable substitution
// ---------------------------------------------------------------------------

TEST(ParseSteps, SubstitutionNoVars) {
    const std::string yaml = R"(
steps:
  - name: "plain"
    run: "echo hello"
)";

    auto steps = parseSteps(yaml, {});
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0].run, "echo hello");
}

TEST(ParseSteps, SubstitutionUnknownPlaceholderUntouched) {
    const std::string yaml = R"(
steps:
  - name: "unknown"
    run: "echo {{!MISSING!}}"
)";

    auto steps = parseSteps(yaml, {});
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0].run, "echo {{!MISSING!}}");
}

TEST(ParseSteps, SubstitutionMultipleOccurrencesOfSameVar) {
    const std::string yaml = R"(
steps:
  - name: "double"
    run: "cp {{!DIR!}}/a {{!DIR!}}/b"
)";

    std::vector<Variable> vars = {{"DIR", "/tmp/deploy"}};
    auto steps = parseSteps(yaml, vars);
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0].run, "cp /tmp/deploy/a /tmp/deploy/b");
}

TEST(ParseSteps, SubstitutionMultipleDifferentVars) {
    const std::string yaml = R"(
steps:
  - name: "multi"
    run: "git clone {{!REPO!}} {{!DIR!}}"
)";

    std::vector<Variable> vars = {
        {"REPO", "https://github.com/user/repo.git"},
        {"DIR",  "/opt/app"},
    };
    auto steps = parseSteps(yaml, vars);
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0].run, "git clone https://github.com/user/repo.git /opt/app");
}

TEST(ParseSteps, SubstitutionWithEmptyValue) {
    const std::string yaml = R"(
steps:
  - name: "empty val"
    run: "echo '{{!X!}}' done"
)";

    std::vector<Variable> vars = {{"X", ""}};
    auto steps = parseSteps(yaml, vars);
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0].run, "echo '' done");
}

TEST(ParseSteps, SubstitutionOnlyAffectsRunField) {
    const std::string yaml = R"(
steps:
  - name: "{{!TAG!}}"
    run: "deploy {{!TAG!}}"
)";

    std::vector<Variable> vars = {{"TAG", "v1.0"}};
    auto steps = parseSteps(yaml, vars);
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0].name, "{{!TAG!}}");
    EXPECT_EQ(steps[0].run, "deploy v1.0");
}

TEST(ParseSteps, SubstitutionValueContainingPlaceholderSyntax) {
    const std::string yaml = R"(
steps:
  - name: "tricky"
    run: "echo {{!A!}}"
)";

    std::vector<Variable> vars = {{"A", "{{!B!}}"}};
    auto steps = parseSteps(yaml, vars);
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0].run, "echo {{!B!}}");
}

TEST(ParseSteps, SubstitutionAcrossMultipleSteps) {
    const std::string yaml = R"(
steps:
  - name: "first"
    run: "cd {{!DIR!}}"
  - name: "second"
    run: "ls {{!DIR!}}"
)";

    std::vector<Variable> vars = {{"DIR", "/srv/app"}};
    auto steps = parseSteps(yaml, vars);
    ASSERT_EQ(steps.size(), 2);
    EXPECT_EQ(steps[0].run, "cd /srv/app");
    EXPECT_EQ(steps[1].run, "ls /srv/app");
}

// ---------------------------------------------------------------------------
// End-to-end: parseVariables + parseSteps together
// ---------------------------------------------------------------------------

TEST(EndToEnd, FullYamlWithVarsAndSteps) {
    const std::string yaml = R"(
vars:
  - name: "REPO"
    value: "https://github.com/user/repo.git"
  - name: "BRANCH"
    value: "main"
  - name: "APP_DIR"
    value: "/home/deploy/app"

steps:
  - name: "Clone"
    run: "git clone -b {{!BRANCH!}} {{!REPO!}} {{!APP_DIR!}}"
  - name: "Build"
    run: "cd {{!APP_DIR!}} && make"
  - name: "Info"
    run: "echo deployed {{!REPO!}} on {{!BRANCH!}}"
)";

    auto vars = parseVariables(yaml);
    ASSERT_EQ(vars.size(), 3);

    auto steps = parseSteps(yaml, vars);
    ASSERT_EQ(steps.size(), 3);

    EXPECT_EQ(steps[0].run,
              "git clone -b main https://github.com/user/repo.git /home/deploy/app");
    EXPECT_EQ(steps[1].run,
              "cd /home/deploy/app && make");
    EXPECT_EQ(steps[2].run,
              "echo deployed https://github.com/user/repo.git on main");
}
