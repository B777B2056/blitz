#pragma once
#include <string>
#include <system_error>

namespace blitz
{
    enum class SocketError : std::uint8_t 
    {
        Success = 0,
        PeerClosed,
        InternalError,
        // Other error
    };

    class SocketErrorCategory : public std::error_category
    {
    public:
        std::string message(int c) const override;
        const char* name() const noexcept override;
        static const std::error_category& get();

    private:
        SocketErrorCategory() = default;
    };

    std::error_code make_error_code(blitz::SocketError ec);
}   // namespace blitz

namespace std
{
    template <>
    struct is_error_code_enum<blitz::SocketError> : true_type {};
}
