#pragma once

#include <async/config.h>
#include <functional>
#include <stop_token>
#include <utility>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
namespace lcf::async::execution {

LCF_ASYNC_EXPORT
template <typename Completion>
class StopBoundCompletion
{
public:
    StopBoundCompletion(std::stop_token token, Completion completion) : m_token(token), m_completion(std::move(completion)) {}
    std::stop_token getStopToken() const noexcept { return m_token; }
    template <typename... Arguments>
    decltype(auto) operator()(Arguments &&... arguments) { return std::invoke(m_completion, std::forward<Arguments>(arguments)...); }
private:
    std::stop_token m_token;
    Completion m_completion;
};

LCF_ASYNC_EXPORT
template <typename Completion>
auto bind_stop_token(std::stop_token token, Completion completion) { return StopBoundCompletion<Completion> {token, std::move(completion)}; }

}
#endif
