#include "ec.h"
#include <errno.h>
#include <cstring>

namespace blitz
{
    std::string ErrorCodeCategory::message(int c) const
    {
        switch (static_cast<ErrorCode>(c))
        {
        case ErrorCode::Success:
            return "Success";

        case ErrorCode::SubmitQueueFull:
            return "SubmitQueueFull";
    
        case ErrorCode::PeerClosed:
            return "Peer closed";
    
        case ErrorCode::InternalError:
            return ::strerror(errno);
        }
        return "Invalid Error Code";
    }

    const char* ErrorCodeCategory::name() const noexcept
    {
        return "Socket Error Category";
    }

    const std::error_category& ErrorCodeCategory::get()
    {
        const static ErrorCodeCategory sCategory;
        return sCategory;
    }

    std::error_code make_error_code(blitz::ErrorCode ec)
    {
        return std::error_code(static_cast<int>(ec), blitz::ErrorCodeCategory::get());
    }
}   // namespace blitz
