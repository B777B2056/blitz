#pragma once
#include <string>
#include <system_error>

namespace blitz
{
    enum class ErrorCode : std::uint8_t 
    {
        Success = 0,
        SubmitQueueFull,
        PeerClosed,
        InternalError,
        // Other error
    };

    class ErrorCodeCategory : public std::error_category
    {
    public:
        std::string message(int c) const override;
        const char* name() const noexcept override;
        static const std::error_category& get();

    private:
        ErrorCodeCategory() = default;
    };

    std::error_code make_error_code(blitz::ErrorCode ec);
}   // namespace blitz

namespace std
{
    template <>
    struct is_error_code_enum<blitz::ErrorCode> : true_type {};
}
