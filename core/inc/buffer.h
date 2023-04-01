#pragma once
#include <vector>
#include <span>
#include <cstdint>

#ifdef __linux__
#include <sys/uio.h> 
#elif _WIN32
        
#endif

namespace blitz
{
    namespace detail
    {
        struct BufferChunk
        {
            int refCnt;
            std::size_t readIdx;
            std::size_t writeIdx;
            std::vector<char> buf;
            BufferChunk* next;

            BufferChunk();
            ~BufferChunk() = default;
            std::size_t readableSize() const;
            std::size_t writeableSize() const;
            std::size_t readFromChunk(std::span<char> data);
            std::size_t writeIntoChunk(std::span<const char> data);
            void moveInside();
        };
    }   // namespace detail

    class ChainBuffer
    {
    public:
        ChainBuffer();
        ChainBuffer(const ChainBuffer&) = delete;
        ChainBuffer& operator=(const ChainBuffer&) = delete;
        ChainBuffer(ChainBuffer&& rhs);
        ChainBuffer& operator=(ChainBuffer&& rhs);
        ~ChainBuffer();

        std::size_t readFromBuffer(std::span<char> data);
        std::size_t writeIntoBuffer(std::span<const char> data);

    public:
#ifdef __linux__
        using NativeIoVec = iovec;    
#elif _WIN32
        
#endif
        // 为分散-聚集IO准备；Linux为iovec，Win为WSABUF；具体转换到平台相关的结构，发生在EventQueue中
        std::span<const NativeIoVec> readableArea2Iovecs();
        std::span<const NativeIoVec> writeableArea2Iovecs();

        // 为完成事件的出现而移动每个chunk的读写指针（即内核异步读写完成后移动读写指针）
        void moveReadableAreaIdx(std::size_t transferredBytes);
        void moveWriteableAreaIdx(std::size_t transferredBytes);

        // 完成事件出现后，才能销毁原先iovec
        void destroyReadableIovecs();
        void destroyWriteableIovecs();

    private:
        std::uint8_t mListSize_;
        std::uint8_t mListCapacity_;
        detail::BufferChunk* mChunkListHead_;
        detail::BufferChunk* mChunkListLast_;
        detail::BufferChunk* mChunkListLastWithData_;

        NativeIoVec* mReadableAreaIovecs_;
        NativeIoVec* mWriteableAreaIovecs_;

        void expand(std::uint8_t chunkNum);
    };
}   // namespace blitz
