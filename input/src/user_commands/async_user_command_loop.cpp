#include "user_commands/async_user_command_loop.h"
#include "log.h"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

using namespace lcf;
namespace asio = boost::asio;

#ifdef _WIN32

#include <windows.h>
#include <boost/asio/windows/object_handle.hpp>
#include <boost/circular_buffer.hpp>
#include <iostream>

class Win32CommandLineReader
{
    using Self = Win32CommandLineReader;
    using ReadMethod = asio::awaitable<std::string> (Self::*)();
    using HistoryRecords = boost::circular_buffer<std::string>;
    using HistoryRecordsIterator = typename HistoryRecords::reverse_iterator;
    using InputRecords = std::array<INPUT_RECORD, 128>;
public:
    Win32CommandLineReader(asio::windows::object_handle console_handle, size_t history_size = 100);
    Win32CommandLineReader(const Self &) = delete;
    Win32CommandLineReader(Self &&) = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = default;
    asio::awaitable<std::string> readLine();
private:
    asio::awaitable<std::string> readLineFromConsole();
    asio::awaitable<std::string> readLineFromPipe();
    void processConsoleInput(
        const INPUT_RECORD & input_record,
        bool & read_line_finished,
        HistoryRecordsIterator & record_iter,
        std::string & line);
private:
    asio::windows::object_handle m_console_handle;
    ReadMethod m_read_method = nullptr;
    HistoryRecords m_history_records;
};

#else

#include <unistd.h>
#include <boost/asio/posix/stream_descriptor.hpp>

asio::awaitable<std::string> read_line_from_posix(asio::posix::stream_descriptor & stdin_descriptor)

#endif // _WIN32

asio::awaitable<void> lcf::async_user_command_loop(IUserCommandContext & user_cammand_context)
{
#ifdef _WIN32
    HANDLE stdin_handle = ::GetStdHandle(STD_INPUT_HANDLE);
    if (stdin_handle == INVALID_HANDLE_VALUE) {
        std::runtime_error error {"GetStdHandle Failed"};
        lcf_log_error(error.what());
        throw error;
    }
    asio::windows::object_handle console {co_await asio::this_coro::executor, stdin_handle};
    Win32CommandLineReader command_reader {std::move(console)};
    while (user_cammand_context.isActive()) {
        user_cammand_context.execute(co_await command_reader.readLine());
    }
#else
    asio::posix::stream_descriptor stream_descriptor {co_await asio::this_coro::executor, ::dup(STDIN_FILENO)};
    while (user_cammand_context.isActive()) {
        user_cammand_context.execute(co_await read_line_from_posix(stream_descriptor));
    }
#endif // _WIN32
    co_return;
}

#ifdef _WIN32

Win32CommandLineReader::Win32CommandLineReader(asio::windows::object_handle console_handle, size_t history_size) :
    m_console_handle(std::move(console_handle)),
    m_history_records(history_size)
{
    DWORD file_type = ::GetFileType(m_console_handle.native_handle());
    if (file_type == FILE_TYPE_CHAR) {
        m_read_method = &Self::readLineFromConsole;
    } else if (file_type == FILE_TYPE_PIPE || file_type == FILE_TYPE_DISK) {
        m_read_method = &Self::readLineFromPipe;
    } else {
        std::runtime_error error {"Invalid file type"};
        lcf_log_error(error.what());
        throw error;
    }
}

asio::awaitable<std::string> Win32CommandLineReader::readLine()
{
    return (this->*m_read_method)();
}

asio::awaitable<std::string> Win32CommandLineReader::readLineFromConsole()
{
    m_history_records.push_back();
    auto & line = m_history_records.back();
    auto record_iter = m_history_records.rbegin();
    bool read_line_finished = false;
    while (not read_line_finished) {
        co_await m_console_handle.async_wait(asio::use_awaitable);
        InputRecords input_records;
        DWORD events_read = 0;
        if (not ::ReadConsoleInput(m_console_handle.native_handle(), input_records.data(), std::size(input_records), &events_read)) {
            std::runtime_error error {"ReadConsoleInput Failed"};
            lcf_log_error(error.what());
            throw error;
        }
        for (DWORD i = 0; i < events_read; ++i) {
            this->processConsoleInput(input_records[i], read_line_finished, record_iter, line);
        }
    }
    co_return line;
}

asio::awaitable<std::string> Win32CommandLineReader::readLineFromPipe()
{
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer {executor};
    std::string line;
    DWORD available_bytes = 0;
    while (available_bytes == 0) {
        if (not ::PeekNamedPipe(m_console_handle.native_handle(), nullptr, 0, nullptr, &available_bytes, nullptr)) {
            std::runtime_error error {"PeekNamedPipe failed"};
            lcf_log_error(error.what());
            throw error;
        }
        timer.expires_after(std::chrono::milliseconds(500));
        co_await timer.async_wait(asio::use_awaitable);
    }
    std::vector<char> buffer(available_bytes);
    DWORD bytes_read = 0;
    if (not ::ReadFile(m_console_handle.native_handle(), buffer.data(), available_bytes, &bytes_read, nullptr)) {
        std::runtime_error error {"ReadFile failed"};
        lcf_log_error(error.what());
        throw error;
    }
    line = std::string(buffer.data(), bytes_read);
    co_return line;
}

void Win32CommandLineReader::processConsoleInput(
    const INPUT_RECORD & input_record,
    bool & read_line_finished,
    HistoryRecordsIterator & record_iter,
    std::string &line)
{
    if (input_record.EventType != KEY_EVENT or not input_record.Event.KeyEvent.bKeyDown) { return; }
    switch (input_record.Event.KeyEvent.wVirtualKeyCode) {
        case VK_UP: {
            if (std::next(record_iter) == m_history_records.rend()) { break; }
            line = *(++record_iter);
            std::cout << "\r\033[K" << line << std::flush;
        } break;
        case VK_DOWN: {
            if (record_iter == m_history_records.rbegin()) { break; }
            line = *(--record_iter);
            std::cout << "\r\033[K" << line << std::flush;
        } break;
        default: {
            char ch = input_record.Event.KeyEvent.uChar.AsciiChar;
            if (ch == '\r') {
                std::cout << std::endl;
                read_line_finished = true;
            } else if (std::isprint(ch)) {
                line += ch;
                std::cout << ch;
            } else if (ch == '\b' and not line.empty()) {
                line.pop_back();
                std::cout << "\b \b";
            }
        } break;
    }
}

#else
#endif // _WIN32
