// Tests that show HLO Module conversion to PlaidML Program.

#include <algorithm>
#include <string>
#include <map>
#include <variant>

#include <gtest/gtest.h>

#include "absl/strings/str_cat.h"
#include "tensorflow/compiler/xla/service/plaidml/compiler.h"
#include "tensorflow/compiler/xla/service/plaidml/tests/plaidml_codegen_test.h"
#include "tensorflow/compiler/xla/service/hlo_computation.h"
#include "tensorflow/compiler/xla/service/hlo_evaluator.h"
#include "tensorflow/compiler/xla/tests/filecheck.h"
#include "tensorflow/compiler/xla/tests/test_utils.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/core/platform/test.h"
#include "plaidml/testenv.h"

using ::plaidml::edsl::TensorBuffers;

namespace xla {
namespace plaidml {
namespace {

using TestCaseVal = std::vector<std::vector<int>>;
using TestCasePairs = std::map<TestCaseVal, TestCaseVal>;

struct EltwiseTestSpec {
  PrimitiveType primitive_type;
  string filecheck_lines;
};

string EltwiseTestSpecToString(const ::testing::TestParamInfo<EltwiseTestSpec>& info) {
  return PrimitiveType_Name(info.param.primitive_type);
}

class PlaidMLEltwiseOperationTest
    : public PlaidMLCodegenTest,
      public ::testing::WithParamInterface<EltwiseTestSpec> {
 protected:
  Status CompileAndCheck(std::unique_ptr<HloComputation> entry_computation,
                         const string& filecheck_lines,
                         const TestCasePairs& testcase_pairs) {

    HloModuleConfig cfg;

    std::unique_ptr<HloModule> hlo_module = absl::make_unique<HloModule>("module", cfg);
    hlo_module->AddEntryComputation(std::move(entry_computation));

    auto program = CompileToProgram(std::move(hlo_module));

    VLOG(2) << "Program:\n" << program->str();

    StatusOr<bool> fc_result = RunFileCheck(program->str(), filecheck_lines);

    //TF_ASSERT_OK(fc_result.status());
    EXPECT_TRUE(fc_result.ValueOrDie());

    VLOG(2) << "Evaluating results";

    for (auto pair : testcase_pairs) {

      TensorBuffers inp;
      TensorBuffers exp;

      auto program_inputs = program->inputs();

      for (auto i = 0; i < program_inputs.size(); i++) {
        inp.insert(std::make_pair(program_inputs[i].tensor, pair.first[i]));
      }

      auto program_outputs = program->outputs();

      for (auto i = 0; i < program_outputs.size(); i++) {
        exp.insert(std::make_pair(program_outputs[i].tensor, pair.second[i]));
      }

      checkProgram(*program, inp, exp);

    }

    return Status::OK();

  }
};

TEST_P(PlaidMLEltwiseOperationTest, EltwiseAndOp) {
  std::vector<int> input_A = {0, 0, 1, 1, 0, 0, 1, 1, 0};
  std::vector<int> input_B = {1, 0, 1, 0, 1, 0, 1, 0, 1};
  std::vector<int> output_C = {0, 0, 1, 0, 0, 0, 1, 0, 0};

  TestCaseVal inputs = {input_A, input_B};
  TestCaseVal results = {output_C};
  TestCasePairs testcase_pairs = {{inputs, results}};

  HloComputation::Builder builder("EltwiseAndOp");
  EltwiseTestSpec spec = GetParam();

  auto fcheck_lines = spec.filecheck_lines;
  fcheck_lines.insert(4,"CHECK: func @hlo_module(%arg0: tensor<3x3xsi32>, %arg1: tensor<3x3xsi32>) -> tensor<3x3xsi32>\n");

  auto param_shape = ShapeUtil::MakeShape(spec.primitive_type, {3, 3});

  HloInstruction* lhs = builder.AddInstruction(
      HloInstruction::CreateParameter(0, param_shape, "input"));
  HloInstruction* rhs = builder.AddInstruction(
      HloInstruction::CreateParameter(1, param_shape, "input"));

  builder.AddInstruction(HloInstruction::CreateBinary(param_shape, HloOpcode::kAnd, lhs, rhs));
  CompileAndCheck(builder.Build(), fcheck_lines, testcase_pairs);
}

TEST_P(PlaidMLEltwiseOperationTest, EltwiseNotOp) {
  std::vector<int> input_A = {0, 0, 1, 1, 0, 0, 1, 1, 0};
  std::vector<int> output_C = {1, 1, 0, 0, 1, 1, 1, 1, 0};

  TestCaseVal inputs = {input_A};
  TestCaseVal results = {output_C};
  TestCasePairs testcase_pairs = {{inputs, results}};

  HloComputation::Builder builder("EltwiseNotOp");
  EltwiseTestSpec spec = GetParam();

  auto fcheck_lines = spec.filecheck_lines;
  fcheck_lines.insert(4,"CHECK: func @hlo_module(%arg0: tensor<3x3xsi32>) -> tensor<3x3xsi32>\n");

  auto param_shape = ShapeUtil::MakeShape(spec.primitive_type, {3, 3});

  HloInstruction* lhs = builder.AddInstruction(
      HloInstruction::CreateParameter(0, param_shape, "input"));

  builder.AddInstruction(HloInstruction::CreateUnary(param_shape, HloOpcode::kNot, lhs));
  CompileAndCheck(builder.Build(), fcheck_lines, testcase_pairs);
}

TEST_P(PlaidMLEltwiseOperationTest, EltwiseOrOp) {
  std::vector<int> input_A = {0, 0, 1, 1, 0, 0, 1, 1, 0};
  std::vector<int> input_B = {1, 0, 1, 0, 1, 0, 1, 0, 1};
  std::vector<int> output_C = {1, 0, 1, 1, 1, 0, 1, 1, 1};

  TestCaseVal inputs = {input_A, input_B};
  TestCaseVal results = {output_C};
  TestCasePairs testcase_pairs = {{inputs, results}};

  HloComputation::Builder builder("EltwiseOrOp");
  EltwiseTestSpec spec = GetParam();

  auto fcheck_lines = spec.filecheck_lines;
  fcheck_lines.insert(4,"CHECK: func @hlo_module(%arg0: tensor<3x3xsi32>, %arg1: tensor<3x3xsi32>) -> tensor<3x3xsi32>\n");

  auto param_shape = ShapeUtil::MakeShape(spec.primitive_type, {3, 3});

  HloInstruction* lhs = builder.AddInstruction(
      HloInstruction::CreateParameter(0, param_shape, "input"));
  HloInstruction* rhs = builder.AddInstruction(
      HloInstruction::CreateParameter(1, param_shape, "input"));

  builder.AddInstruction(HloInstruction::CreateBinary(param_shape, HloOpcode::kOr, lhs, rhs));
  CompileAndCheck(builder.Build(), fcheck_lines, testcase_pairs);
}

TEST_P(PlaidMLEltwiseOperationTest, EltwiseXorOp) {
  std::vector<int> input_A = {0, 0, 1, 1, 0, 0, 1, 1, 0};
  std::vector<int> input_B = {1, 0, 1, 0, 1, 0, 1, 0, 1};
  std::vector<int> output_C = {1, 0, 0, 1, 1, 0, 0, 1, 1};

  TestCaseVal inputs = {input_A, input_B};
  TestCaseVal results = {output_C};
  TestCasePairs testcase_pairs = {{inputs, results}};

  HloComputation::Builder builder("EltwiseXorOp");
  EltwiseTestSpec spec = GetParam();

  auto fcheck_lines = spec.filecheck_lines;
  fcheck_lines.insert(4,"CHECK: func @hlo_module(%arg0: tensor<3x3xsi32>, %arg1: tensor<3x3xsi32>) -> tensor<3x3xsi32>\n");

  auto param_shape = ShapeUtil::MakeShape(spec.primitive_type, {3, 3});

  HloInstruction* lhs = builder.AddInstruction(
      HloInstruction::CreateParameter(0, param_shape, "input"));
  HloInstruction* rhs = builder.AddInstruction(
      HloInstruction::CreateParameter(1, param_shape, "input"));

  builder.AddInstruction(HloInstruction::CreateBinary(param_shape, HloOpcode::kXor, lhs, rhs));
  CompileAndCheck(builder.Build(), fcheck_lines, testcase_pairs);
}

std::vector<EltwiseTestSpec> GetEltwiseTestCases() {
  std::vector<EltwiseTestSpec> result;
  result.push_back(
      {S32, R"#(
        CHECK: return %{{.*}} : tensor<3x3xsi32>
        )#"});
  // TODO: Determine issue with si64 testing
  return result;
}



/**/
// TODO: INSTANTIATE_TEST_CASE_P was deprecated in favor for INSTANTIATE_TEST_SUITE_P, but the version of gtest that bazel links in is looking for INSTANTIATE_TEST_CASE_P right now.
INSTANTIATE_TEST_CASE_P(EltwiseAndOp,
                         PlaidMLEltwiseOperationTest,
                         ::testing::ValuesIn(GetEltwiseTestCases()),
                         EltwiseTestSpecToString);
/**/
}  // namespace
}  // namespace plaidml
}  // namespace xla
