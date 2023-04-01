#include "ec.h"
#include <errno.h>
#include <cstring>

namespace blitz
{
    std::string SocketErrorCategory::message(int c) const
    {
        switch (static_cast<SocketError>(c))
        {
        case SocketError::Success:
            return "Success";
    
        case SocketError::PeerClosed:
            return "Peer closed";
    
        case SocketError::InternalError:
            return ::strerror(errno);
        }
        return "Invalid Error Code";
    }

    const char* SocketErrorCategory::name() const noexcept
    {
        return "Socket Error Category";
    }

    const std::error_category& SocketErrorCategory::get()
    {
        const static SocketErrorCategory sCategory;
        return sCategory;
    }

    std::error_code make_error_code(blitz::SocketError ec)
    {
        return std::error_code(static_cast<int>(ec), blitz::SocketErrorCategory::get());
    }
}   // namespace blitz
