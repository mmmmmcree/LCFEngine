#include <Runtime.h>
#include <gtest/gtest.h>

namespace {

namespace task = lcf::async::task;

struct TokenCase
{
    const char * m_name_p;
    bool m_void;
    bool m_junction;
};

using FailedAdditionalToken = testing::TestWithParam<TokenCase>;

/*
 * Objective: preserve the interpretation of an already failed extra token.
 * Input: a completed failed value or void token, used by a chain or junction link.
 * Procedure: run the source, then the consumer with a recovery handler.
 * Expected: a value token fails argument extraction with io_error before task/handler;
 *   a void token adds no argument, so the consumer succeeds with value 4.
 * Failure hints: inspect result_to_arguments and the additional-token path of link;
 *   merge inputs and additional link tokens intentionally have different contracts.
 */
void failed_additional_token_keeps_argument_semantics(const TokenCase & scenario);
TEST_P(FailedAdditionalToken, KeepsArgumentSemantics) { failed_additional_token_keeps_argument_semantics(GetParam()); }

const TokenCase k_cases[] {
    {"ChainValue", false, false}, {"ChainVoid", true, false},
    {"JunctionValue", false, true}, {"JunctionVoid", true, true}
};
INSTANTIATE_TEST_SUITE_P(ArgumentKinds, FailedAdditionalToken, testing::ValuesIn(k_cases),
    [](const testing::TestParamInfo<TokenCase> & info) { return info.param.m_name_p; });

void failed_additional_token_keeps_argument_semantics(const TokenCase & scenario)
{
    lcf::async::testing::Runtime runtime;
    unsigned user_runs = 0;
    unsigned handler_runs = 0;
    auto consume = [&](auto token) {
        auto seed = task::Chain {3, runtime.control()};
        auto function = [&](const int & input, const auto &...) { ++user_runs; return input + 1; };
        auto handler = task::on_error([&](const task::ErrorHandleContext &) { ++handler_runs; return task::use_value(99); });
        auto consumer = [&] {
            if (scenario.m_junction) {
                return task::merge(seed.getResultToken()).link(runtime.cpu(), function, std::move(handler), token);
            }
            return std::move(seed).link(runtime.cpu(), function, std::move(handler), token);
        }();
        const auto result_rh = runtime.run(std::move(consumer));
        if (scenario.m_void) {
            ASSERT_TRUE(*result_rh);
            EXPECT_EQ(result_rh->value(), 4);
            EXPECT_EQ(user_runs, 1u);
        } else {
            ASSERT_FALSE(*result_rh);
            EXPECT_EQ(result_rh->error(), std::errc::io_error);
            EXPECT_EQ(user_runs, 0u);
        }
        EXPECT_EQ(handler_runs, 0u);
    };
    if (scenario.m_void) {
        auto failed = task::Chain {runtime.cpu()}.link([]() -> task::Result<void> { return std::unexpected(std::error_code {}); });
        const auto token = failed.getResultToken();
        ASSERT_FALSE(*runtime.run(std::move(failed)));
        consume(token);
    } else {
        auto failed = task::Chain {runtime.cpu()}.link([]() -> task::Result<int> { return std::unexpected(std::error_code {}); });
        const auto token = failed.getResultToken();
        ASSERT_FALSE(*runtime.run(std::move(failed)));
        consume(token);
    }
}

}
