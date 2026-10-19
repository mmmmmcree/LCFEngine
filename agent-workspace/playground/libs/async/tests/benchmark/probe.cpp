#include "Workloads.h"
#include "allocation_probe.h"
#include <fstream>
#include <iostream>

int main(int argc, char ** argv)
{
    namespace comparison = lcf::async::comparison;
    if (argc != 2) { std::cerr << "Usage: async_probe_<backend> output.csv\n"; return 1; }
    try {
        std::ofstream output {argv[1]};
        output.exceptions(std::ios::badbit | std::ios::failbit);
#if LCF_ASYNC_MEASURE_ALLOCATIONS
        output << "backend,case,size,new_calls,new_requested_bytes,new_peak_live_bytes,new_remaining_bytes\n";
#else
        output << "backend,case,size,callbacks,poll_p50_us,poll_p99_us,poll_max_us,max_steps_per_poll,timer_p99_lateness_us\n";
#endif
        for (const auto test_case : comparison::k_cases) {
            comparison::run_case(test_case);
#if LCF_ASYNC_MEASURE_ALLOCATIONS
            comparison::allocation::begin();
            comparison::run_case(test_case);
            const auto memory = comparison::allocation::end();
            output << LCF_ASYNC_COMPARISON_BACKEND << ',' << test_case.m_name << ',' << test_case.m_size
                << ',' << memory.m_allocations << ',' << memory.m_bytes << ',' << memory.m_peak_bytes << ',' << memory.m_live_bytes << '\n';
#else
            comparison::Samples samples;
            samples.m_poll_us.reserve(test_case.m_size * 32);
            samples.m_timer_lateness_us.reserve(test_case.m_size);
            for (unsigned repeat = 0; repeat < 8; ++repeat) { comparison::run_case(test_case, &samples); }
            output << LCF_ASYNC_COMPARISON_BACKEND << ',' << test_case.m_name << ',' << test_case.m_size << ',' << samples.m_callbacks / 8.0
                << ',' << comparison::percentile(samples.m_poll_us, 0.5) << ',' << comparison::percentile(samples.m_poll_us, 0.99)
                << ',' << comparison::percentile(samples.m_poll_us, 1.0) << ',' << samples.m_max_steps_per_poll
                << ',' << comparison::percentile(samples.m_timer_lateness_us, 0.99) << '\n';
#endif
        }
    } catch (const std::exception & error) {
#if LCF_ASYNC_MEASURE_ALLOCATIONS
        comparison::allocation::end();
#endif
        std::cerr << error.what() << '\n';
        return 1;
    }
}
