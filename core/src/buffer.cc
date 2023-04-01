#include "buffer.h"
#include <algorithm>
#include <iostream>

namespace blitz
{
    constexpr static std::uint16_t OneChunkSize = 1024;
    constexpr static std::uint8_t InitChunkListCapacity = 2;

    namespace detail
    {
        BufferChunk::BufferChunk()
            : refCnt(0)
            , readIdx(0), writeIdx(0)
            , next(nullptr)
        {
            this->buf.resize(OneChunkSize);
            this->buf.shrink_to_fit();
        }

        std::size_t BufferChunk::readableSize() const
        {
            return this->writeIdx - this->readIdx;
        }

        std::size_t BufferChunk::writeableSize() const
        {
            return this->buf.size() - this->writeIdx;
        }

        std::size_t BufferChunk::readFromChunk(std::span<char> data)
        {
            std::size_t readableBytes = this->readableSize();
            std::size_t readBytes = (readableBytes > data.size()) ? data.size() : readableBytes;
            std::copy(&this->buf[this->readIdx], &this->buf[this->readIdx + readBytes], data.begin());
            this->readIdx += readBytes;
            return readBytes;
        }

        std::size_t BufferChunk::writeIntoChunk(std::span<const char> data)
        {
            // 若当前空间不足以容纳数据，则尝试对已有数据进行移动
            if (this->writeableSize() < data.size())
            {
                this->moveInside();
            }
            // 重新计算可写空间大小
            std::size_t writeableBytes = this->writeableSize();
            std::size_t writeBytes = (writeableBytes > data.size()) ? data.size() : writeableBytes;
            // 写入缓冲区
            std::copy(data.begin(), data.begin() + writeBytes, &this->buf[this->writeIdx]);
            // 更新写索引
            this->writeIdx += writeBytes;
            return writeBytes;
        }

        void BufferChunk::moveInside()
        {
            if (0 == this->readIdx)  return;
            if (this->readIdx == this->writeIdx)
            {
                this->readIdx = this->writeIdx = 0;
                return;
            }
            std::size_t validBytes = this->readableSize();
            if (validBytes < this->readIdx)
            {
                // 有效数据区域与可移动区域无重叠，直接进行移动
                std::copy(&this->buf[this->readIdx], &this->buf[this->writeIdx], &this->buf[0]);
            }
            else
            {
                // 有效数据区域与可移动区域有重叠，分段进行移动
                std::size_t dstPos = 0;
                std::size_t srcPos = this->readIdx;
                std::size_t segmentSize = this->readIdx;
                while (this->writeIdx - srcPos > segmentSize)
                {
                    std::copy(&this->buf[srcPos], &this->buf[srcPos + segmentSize], &this->buf[dstPos]);
                    srcPos += segmentSize;
                    dstPos += segmentSize;
                }
                std::copy(&this->buf[srcPos], &this->buf[this->writeIdx], &this->buf[dstPos]);
            }
            this->readIdx = 0;
            this->writeIdx = validBytes;
        }
    }   // namespace detail

    ChainBuffer::ChainBuffer()
        : mListSize_{0}
        , mListCapacity_{InitChunkListCapacity}
        , mChunkListHead_{new detail::BufferChunk()}
        , mReadableAreaIovecs_{nullptr}, mWriteableAreaIovecs_{nullptr}
    {
        this->mChunkListLast_ = this->mChunkListHead_;
        this->mChunkListLastWithData_ = this->mChunkListHead_;
        this->expand(this->mListCapacity_);
    }

    ChainBuffer::ChainBuffer(ChainBuffer&& rhs)
        : mListSize_{0}
        , mListCapacity_{0}
        , mChunkListHead_{nullptr}
        , mChunkListLast_{nullptr}
        , mChunkListLastWithData_{nullptr}
        , mReadableAreaIovecs_{nullptr}, mWriteableAreaIovecs_{nullptr}
    {
        *this = std::move(rhs);
    }

    ChainBuffer& ChainBuffer::operator=(ChainBuffer&& rhs)
    {
        if (this != &rhs)
        {
            this->mListSize_ = rhs.mListSize_;
            this->mListCapacity_= rhs.mListCapacity_;
            this->mChunkListHead_= rhs.mChunkListHead_;
            this->mChunkListLast_= rhs.mChunkListLast_;
            this->mChunkListLastWithData_= rhs.mChunkListLastWithData_;
            this->mReadableAreaIovecs_ = rhs.mReadableAreaIovecs_;
            this->mWriteableAreaIovecs_ = rhs.mWriteableAreaIovecs_;
            rhs.mListSize_ = 0;
            rhs.mListCapacity_ = InitChunkListCapacity;
            rhs.mChunkListHead_ = new detail::BufferChunk();
            rhs.mReadableAreaIovecs_ = nullptr;
            rhs.mWriteableAreaIovecs_ = nullptr;
            rhs.expand(rhs.mListCapacity_);
        }
        return *this;
    }

    ChainBuffer::~ChainBuffer()
    {
        auto* tmp = this->mChunkListHead_;
        while (tmp)
        {
            auto* node = tmp;
            tmp = tmp->next;
            delete node;
        }
        if (this->mReadableAreaIovecs_)
        {
            this->destroyReadableIovecs();
        }
        if (this->mWriteableAreaIovecs_)
        {
            this->destroyWriteableIovecs();
        }
    }

    std::size_t ChainBuffer::readFromBuffer(std::span<char> data)
    {
        std::size_t n, transferredBytes = 0;
        auto* chunk = this->mChunkListHead_;
        detail::BufferChunk* lastChunk = nullptr;
        // 读取
        while (true)
        {
            n = chunk->readFromChunk(std::span<char>{data.data() + transferredBytes, data.size() - transferredBytes});
            transferredBytes += n;
            if ((transferredBytes == data.size()) || (0 == n) || (chunk == this->mChunkListLastWithData_)) 
            {
                if (0 == chunk->readableSize())
                {
                    lastChunk = chunk;
                }
                break;
            }
            lastChunk = chunk;
            chunk = chunk->next;
        }
        // 更新chunk链表：将无数据的chunk挂接在链表尾部
        if (lastChunk)
        {
            auto* tmp = chunk->next;
            chunk->next = nullptr;
            this->mChunkListLast_->next = this->mChunkListHead_;
            this->mChunkListHead_ = tmp;
            this->mChunkListLast_ = chunk;
        }
        return transferredBytes;
    }

    std::size_t ChainBuffer::writeIntoBuffer(std::span<const char> data)
    {
        std::size_t n, transferredBytes = 0;
        auto* chunk = this->mChunkListLastWithData_;
        // 写入数据
        while (transferredBytes < data.size())
        {
            n = chunk->writeIntoChunk(std::span<const char>{data.data() + transferredBytes, data.size() - transferredBytes});
            transferredBytes += n;
            if (chunk == this->mChunkListLast_)
            {
                // 扩容：首先计算还需要写入多少字节，然后计算当前块填满后还需要多少个块
                std::size_t restBytes = data.size() - transferredBytes;
                std::uint8_t chunkNum = (restBytes - chunk->writeableSize()) / OneChunkSize + 1;
                this->expand(chunkNum);
            } 
            this->mChunkListLastWithData_ = chunk;
            chunk = chunk->next;
        }
        return transferredBytes;
    }

    void ChainBuffer::expand(std::uint8_t chunkNum)
    {
        if (0 == chunkNum)  return;
        detail::BufferChunk* node = nullptr;
        for (std::uint8_t n = 0; n < chunkNum; ++n) 
        {
            node = new detail::BufferChunk();
            this->mChunkListLast_->next = node;
            this->mChunkListLast_ = node;
        }
        mListCapacity_ += chunkNum;
    }

    std::span<const ChainBuffer::NativeIoVec> ChainBuffer::readableArea2Iovecs()
    {
#ifdef __linux__
        std::size_t i, len;
        i = len = 0;
        for (auto* chunk = this->mChunkListHead_; chunk != this->mChunkListLastWithData_->next; chunk = chunk->next)
        {
            ++len;
        }
        this->mReadableAreaIovecs_ = new NativeIoVec[len];
        for (auto* chunk = this->mChunkListHead_; chunk != this->mChunkListLastWithData_->next; chunk = chunk->next)
        {
            this->mReadableAreaIovecs_[i].iov_base = &chunk->buf[chunk->readIdx];
            this->mReadableAreaIovecs_[i].iov_len = chunk->writeIdx - chunk->readIdx;
            ++i;
        }
        return {this->mReadableAreaIovecs_, len};    
#elif _WIN32
        
#endif
    }

    std::span<const ChainBuffer::NativeIoVec> ChainBuffer::writeableArea2Iovecs()
    {
#ifdef __linux__
        std::size_t i, len;
        i = len = 0;
        for (auto* chunk = this->mChunkListHead_; chunk; chunk = chunk->next)
        {
            ++len;
        }
        this->mWriteableAreaIovecs_ = new NativeIoVec[len];
        for (auto* chunk = this->mChunkListHead_; chunk; chunk = chunk->next)
        {
            this->mWriteableAreaIovecs_[i].iov_base = &chunk->buf[chunk->writeIdx];
            this->mWriteableAreaIovecs_[i].iov_len = chunk->buf.size() - chunk->writeIdx;
            ++i;
        }
        return {this->mWriteableAreaIovecs_, len};
#elif _WIN32
        
#endif
    }

    void ChainBuffer::moveReadableAreaIdx(std::size_t transferredBytes)
    {
        for (auto* chunk = this->mChunkListHead_; chunk != this->mChunkListLastWithData_->next; chunk = chunk->next)
        {
            std::size_t restBytes = chunk->writeIdx - chunk->readIdx;
            if (restBytes >= transferredBytes)
            {
                chunk->readIdx += transferredBytes;
                break;
            }
            transferredBytes -= restBytes;
            chunk->readIdx += restBytes;
        }
    }

    void ChainBuffer::moveWriteableAreaIdx(std::size_t transferredBytes)
    {
        for (auto* chunk = this->mChunkListHead_; chunk != this->mChunkListLastWithData_->next; chunk = chunk->next)
        {
            std::size_t restBytes = chunk->buf.size() - chunk->writeIdx;
            if (restBytes >= transferredBytes)
            {
                chunk->writeIdx += transferredBytes;
                break;
            }
            transferredBytes -= restBytes;
            chunk->writeIdx += restBytes;
        }
    }

    void ChainBuffer::destroyReadableIovecs() 
    { 
        delete[] this->mReadableAreaIovecs_; 
        this->mReadableAreaIovecs_ = nullptr; 
    }

    void ChainBuffer::destroyWriteableIovecs() 
    { 
        delete[] this->mWriteableAreaIovecs_; 
        this->mWriteableAreaIovecs_ = nullptr;
    }
}   // namespace blitz
