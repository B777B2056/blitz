#pragma once
#include <functional>
#include "common.h"

namespace blitz
{
    class SignalHandler : public Event
    {
    public:
        SignalHandler(SocketDescriptor sock) : Event{0} {}
        void setSignal(int sig) noexcept { this->sig = sig; }
        int signal() noexcept { return this->sig; }

    private:
        int sig{0};
    };
}   // namespace blitz
