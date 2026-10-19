#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Fields = std::vector<std::string>;
using Row = std::map<std::string, std::string>;
using Rows = std::vector<Row>;

struct Comparison
{
    std::string m_name;
    std::string m_size;
    std::array<std::optional<Row>, 2> m_rows;
};
using Comparisons = std::vector<Comparison>;

Fields parse_csv_line(std::string_view line);
Rows read_csv(const std::filesystem::path & path);
std::ofstream open_output(const std::filesystem::path & path);
void write_csv_row(std::ostream & output, const Fields & fields);
Comparisons join_rows(const Rows & asio, const Rows & stdexec);
double number(const std::string & text);
std::optional<double> time_us(const std::optional<Row> & row_opt);
std::string_view time_winner(std::optional<double> asio_time_opt, std::optional<double> stdexec_time_opt) noexcept;
std::string format(std::optional<double> value_opt, int precision = 3);
std::string metric(const std::optional<Row> & row_opt, const std::string & name, double scale = 1.0);
std::string display_name(const Comparison & comparison, std::string_view suffix = {});
void write_metric_csv(const std::filesystem::path & path, const Comparisons & comparisons, const Fields & metrics);
bool write_timing(const std::filesystem::path & directory, std::ostream & summary, const Comparisons & comparisons);
void write_diagnostics(const std::filesystem::path & directory, std::ostream & summary, const Comparisons & comparisons);
void write_memory(const std::filesystem::path & directory, std::ostream & summary, const Comparisons & comparisons);

}

int main(int argc, char ** argv)
{
    if (argc != 2) { std::cerr << "Usage: async_comparison_report results_directory\n"; return 1; }
    try {
        const std::filesystem::path directory {argv[1]};
        const auto timing_rows = read_csv(directory / "timing_raw.csv");
        Rows asio;
        Rows stdexec;
        for (const auto & row : timing_rows) {
            const auto & backend = row.at("backend");
            if (backend == "asio") { asio.push_back(row); }
            else if (backend == "stdexec") { stdexec.push_back(row); }
            else { throw std::runtime_error("Unknown timing backend: " + backend); }
        }
        auto summary = open_output(directory / "summary.md");
        summary << "# async benchmark comparison\n\n"
            << "Times are wall time per complete workload iteration, in microseconds (us). "
            << "Winner identifies the lower measured time for that case in this run; equal times are a tie. "
            << "Missing data is n/a; failed measurements are error, never zero.\n\n";
        const auto succeeded = write_timing(directory, summary, join_rows(asio, stdexec));
        write_diagnostics(directory, summary, join_rows(read_csv(directory / "asio_diagnostics.csv"), read_csv(directory / "stdexec_diagnostics.csv")));
        write_memory(directory, summary, join_rows(read_csv(directory / "asio_memory.csv"), read_csv(directory / "stdexec_memory.csv")));
        summary << "\n## Raw data\n\n"
            << "- [Asio benchmark JSON](asio.json), [stdexec benchmark JSON](stdexec.json)\n"
            << "- [Timing](timing.csv), [diagnostics](diagnostics.csv), [memory](memory.csv): adjacent backend columns\n"
            << "- [Unrounded timing input](timing_raw.csv); original per-backend diagnostics and memory CSVs are preserved.\n";
        std::cout << "\nReport: " << (directory / "summary.md").string() << '\n';
        return succeeded ? 0 : 1;
    } catch (const std::exception & error) {
        std::cerr << "Comparison report: " << error.what() << '\n';
        return 1;
    }
}

namespace {

Fields parse_csv_line(std::string_view line)
{
    Fields fields;
    std::string field;
    bool quoted = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const auto character = line[index];
        if (character == '"') {
            if (quoted and index + 1 < line.size() and line[index + 1] == '"') { field += '"'; ++index; }
            else { quoted = not quoted; }
        } else if (character == ',' and not quoted) {
            fields.push_back(std::move(field));
            field.clear();
        } else { field += character; }
    }
    if (quoted) { throw std::runtime_error("Unterminated CSV field"); }
    fields.push_back(std::move(field));
    return fields;
}

Rows read_csv(const std::filesystem::path & path)
{
    std::ifstream input {path};
    if (not input) { throw std::runtime_error("Cannot read " + path.string()); }
    input.exceptions(std::ios::badbit);
    std::string line;
    const auto next_line = [&] {
        if (not std::getline(input, line)) { return false; }
        if (not line.empty() and line.back() == '\r') { line.pop_back(); }
        return true;
    };
    if (not next_line()) { throw std::runtime_error("Missing CSV header: " + path.string()); }
    const auto header = parse_csv_line(line);
    Rows rows;
    while (next_line()) {
        if (line.empty()) { continue; }
        const auto fields = parse_csv_line(line);
        if (fields.size() != header.size()) { throw std::runtime_error("CSV column count mismatch: " + path.string()); }
        Row row;
        for (std::size_t index = 0; index < header.size(); ++index) { row.emplace(header[index], fields[index]); }
        rows.push_back(std::move(row));
    }
    return rows;
}

std::ofstream open_output(const std::filesystem::path & path)
{
    std::ofstream output {path};
    output.exceptions(std::ios::badbit | std::ios::failbit);
    return output;
}

void write_csv_row(std::ostream & output, const Fields & fields)
{
    bool first = true;
    for (const auto & field : fields) {
        if (not first) { output << ','; }
        first = false;
        output << '"';
        for (const auto character : field) {
            if (character == '"') { output << '"'; }
            output << character;
        }
        output << '"';
    }
    output << '\n';
}

Comparisons join_rows(const Rows & asio, const Rows & stdexec)
{
    Comparisons comparisons;
    std::map<std::pair<std::string, std::string>, std::size_t> indices;
    const std::array<const Rows *, 2> backends {&asio, &stdexec};
    for (std::size_t backend = 0; backend < backends.size(); ++backend) {
        for (const auto & row : *backends[backend]) {
            const auto & name = row.at("case");
            const auto size = row.contains("size") ? row.at("size") : "";
            const auto [position_it, inserted] = indices.emplace(std::pair {name, size}, comparisons.size());
            if (inserted) { comparisons.push_back({name, size, {}}); }
            auto & destination_opt = comparisons[position_it->second].m_rows[backend];
            if (destination_opt) { throw std::runtime_error("Duplicate benchmark case: " + name); }
            destination_opt = row;
        }
    }
    return comparisons;
}

double number(const std::string & text)
{
    std::size_t consumed = 0;
    const auto value = std::stod(text, &consumed);
    if (consumed != text.size() or not std::isfinite(value) or value < 0.0) {
        throw std::runtime_error("Invalid measurement: " + text);
    }
    return value;
}

std::optional<double> time_us(const std::optional<Row> & row_opt)
{
    if (not row_opt or row_opt->at("status") != "ok") { return {}; }
    const auto value = number(row_opt->at("real_time"));
    const auto & unit = row_opt->at("time_unit");
    if (unit == "ns") { return value / 1000.0; }
    if (unit == "us") { return value; }
    if (unit == "ms") { return value * 1000.0; }
    if (unit == "s") { return value * 1000000.0; }
    throw std::runtime_error("Unknown benchmark time unit: " + unit);
}

std::string_view time_winner(std::optional<double> asio_time_opt, std::optional<double> stdexec_time_opt) noexcept
{
    if (not asio_time_opt or not stdexec_time_opt) { return "n/a"; }
    if (*asio_time_opt < *stdexec_time_opt) { return "Asio"; }
    if (*stdexec_time_opt < *asio_time_opt) { return "stdexec"; }
    return "tie";
}

std::string format(std::optional<double> value_opt, int precision)
{
    if (not value_opt) { return "n/a"; }
    std::ostringstream output;
    output << std::fixed << std::setprecision(precision) << *value_opt;
    return output.str();
}

std::string metric(const std::optional<Row> & row_opt, const std::string & name, double scale)
{
    if (not row_opt) { return "n/a"; }
    return format(number(row_opt->at(name)) / scale);
}

std::string display_name(const Comparison & comparison, std::string_view suffix)
{
    auto name = comparison.m_name.substr(0, comparison.m_name.size() - suffix.size());
    if (not comparison.m_size.empty()) { name += "/" + comparison.m_size; }
    std::string escaped;
    for (const auto character : name) {
        if (character == '|') { escaped += '\\'; }
        escaped += character;
    }
    return escaped;
}

void write_metric_csv(const std::filesystem::path & path, const Comparisons & comparisons, const Fields & metrics)
{
    auto output = open_output(path);
    Fields header {"case", "size"};
    for (const auto & name : metrics) { header.push_back("asio_" + name); header.push_back("stdexec_" + name); }
    write_csv_row(output, header);
    for (const auto & comparison : comparisons) {
        Fields fields {comparison.m_name, comparison.m_size};
        for (const auto & name : metrics) {
            for (const auto & row_opt : comparison.m_rows) { fields.push_back(row_opt ? row_opt->at(name) : ""); }
        }
        write_csv_row(output, fields);
    }
}

bool write_timing(const std::filesystem::path & directory, std::ostream & summary, const Comparisons & comparisons)
{
    if (comparisons.empty()) { throw std::runtime_error("No benchmark timing results"); }
    std::string suffix;
    const auto options_position = comparisons.front().m_name.find("/min_time:");
    if (options_position != std::string::npos) {
        suffix = comparisons.front().m_name.substr(options_position);
        for (const auto & comparison : comparisons) {
            if (not comparison.m_name.ends_with(suffix)) { suffix.clear(); break; }
        }
    }
    auto output = open_output(directory / "timing.csv");
    write_csv_row(output, {"case", "asio_us", "stdexec_us", "winner", "asio_iterations", "stdexec_iterations", "asio_status", "stdexec_status"});
    summary << "## Workload time\n\n";
    if (not suffix.empty()) {
        summary << "Shared benchmark options: `" << suffix << "`. CSV retains full case names.\n\n";
        std::cout << "\nShared benchmark options: " << suffix << '\n';
    }
    summary << "| Case | Asio (us) | stdexec (us) | Winner |\n| --- | ---: | ---: | --- |\n";
    std::size_t width = 4;
    for (const auto & comparison : comparisons) { width = std::max(width, comparison.m_name.size() - suffix.size()); }
    std::cout << '\n' << std::left << std::setw(static_cast<int>(width)) << "Case"
        << std::right << std::setw(17) << "Asio (us)" << std::setw(17) << "stdexec (us)" << std::setw(12) << "Winner" << '\n';
    bool succeeded = true;
    for (const auto & comparison : comparisons) {
        const auto & asio_opt = comparison.m_rows[0];
        const auto & stdexec_opt = comparison.m_rows[1];
        const auto asio_time_opt = time_us(asio_opt);
        const auto stdexec_time_opt = time_us(stdexec_opt);
        const auto winner = time_winner(asio_time_opt, stdexec_time_opt);
        const auto asio_text = asio_opt and not asio_time_opt ? "error" : format(asio_time_opt);
        const auto stdexec_text = stdexec_opt and not stdexec_time_opt ? "error" : format(stdexec_time_opt);
        summary << "| " << display_name(comparison, suffix) << " | " << asio_text << " | " << stdexec_text << " | " << winner << " |\n";
        std::cout << std::left << std::setw(static_cast<int>(width)) << comparison.m_name.substr(0, comparison.m_name.size() - suffix.size())
            << std::right << std::setw(17) << asio_text << std::setw(17) << stdexec_text << std::setw(12) << winner << '\n';
        write_csv_row(output, {comparison.m_name, asio_time_opt ? format(asio_time_opt, 9) : "", stdexec_time_opt ? format(stdexec_time_opt, 9) : "",
            std::string {winner}, asio_opt ? asio_opt->at("iterations") : "", stdexec_opt ? stdexec_opt->at("iterations") : "",
            asio_opt ? asio_opt->at("status") : "missing", stdexec_opt ? stdexec_opt->at("status") : "missing"});
        if ((asio_opt and not asio_time_opt) or (stdexec_opt and not stdexec_time_opt)) { succeeded = false; }
    }
    std::cout << "Winner: lower measured wall time for this case; equal times are a tie.\n";
    return succeeded;
}

void write_diagnostics(const std::filesystem::path & directory, std::ostream & summary, const Comparisons & comparisons)
{
    write_metric_csv(directory / "diagnostics.csv", comparisons,
        {"callbacks", "poll_p50_us", "poll_p99_us", "poll_max_us", "max_steps_per_poll", "timer_p99_lateness_us"});
    summary << "\n## Poll latency\n\n"
        << "Successful control-context polls; OS preemption affects tails. Full metrics: [diagnostics.csv](diagnostics.csv).\n\n"
        << "| Case / size | Asio p99 (us) | stdexec p99 (us) | Asio max (us) | stdexec max (us) |\n"
        << "| --- | ---: | ---: | ---: | ---: |\n";
    for (const auto & comparison : comparisons) {
        summary << "| " << display_name(comparison);
        for (const auto & name : {"poll_p99_us", "poll_max_us"}) {
            for (const auto & row_opt : comparison.m_rows) { summary << " | " << metric(row_opt, name); }
        }
        summary << " |\n";
    }
}

void write_memory(const std::filesystem::path & directory, std::ostream & summary, const Comparisons & comparisons)
{
    write_metric_csv(directory / "memory.csv", comparisons,
        {"new_calls", "new_requested_bytes", "new_peak_live_bytes", "new_remaining_bytes"});
    summary << "\n## Observable C++ allocations\n\n"
        << "Separate instrumented processes; these are not total process memory or RSS. Full metrics: [memory.csv](memory.csv).\n\n"
        << "| Case / size | Asio new calls | stdexec new calls | Asio peak (KiB) | stdexec peak (KiB) |\n"
        << "| --- | ---: | ---: | ---: | ---: |\n";
    for (const auto & comparison : comparisons) {
        summary << "| " << display_name(comparison);
        for (const auto & row_opt : comparison.m_rows) { summary << " | " << (row_opt ? row_opt->at("new_calls") : "n/a"); }
        for (const auto & row_opt : comparison.m_rows) { summary << " | " << metric(row_opt, "new_peak_live_bytes", 1024.0); }
        summary << " |\n";
    }
}

}
