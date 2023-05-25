#ifndef __BLOCKDATA_HPP__
#define __BLOCKDATA_HPP__

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <queue>

#include "Bitmap.hpp"
#include "ForceInline.hpp"
#include "Vector.hpp"
#include "ErrorBlockData.h"

class BlockData
{
public:
    enum Type
    {
        Etc1,
        Etc2_RGB,
        Etc2_RGBA,
        Dxt1,
        Dxt5
    };

    BlockData( const char* fn );
    BlockData( const char* fn, const v2i& size, bool mipmap, Type type );
    BlockData( const v2i& size, bool mipmap, Type type );
    ~BlockData();

    BitmapPtr Decode();

    void Process( const uint32_t* src, uint32_t blocks, size_t offset, size_t width, Channels type, bool dither, bool useHeuristics );
    // add Hyeon
    void Process(const uint32_t* src, uint32_t blocks, std::shared_ptr<ErrorBlockData> pipeline, size_t offset, size_t width, Channels type, bool dither, bool useHeuristics);
    void Process(const uint32_t* src, uint32_t blocks, PixBlock* pipeline, int *pipeSize, size_t offset, size_t width, Channels type, bool dither, bool useHeuristics);
    void ProcessRGBA( const uint32_t* src, uint32_t blocks, size_t offset, size_t width, bool useHeuristics );
    void ProcessRGBA(const uint32_t* src, uint32_t blocks, PixBlock* pipeline, int *pipeSize, size_t offset, size_t width, bool useHeuristics);

    const v2i& Size() const { return m_size; }

private:
    etcpak_no_inline BitmapPtr DecodeRGB();
    etcpak_no_inline BitmapPtr DecodeRGBA();
    etcpak_no_inline BitmapPtr DecodeDxt1();
    etcpak_no_inline BitmapPtr DecodeDxt5();

    uint8_t* m_data;
    v2i m_size;
    size_t m_dataOffset;
    FILE* m_file;
    size_t m_maplen;
    Type m_type;
};

typedef std::shared_ptr<BlockData> BlockDataPtr;

#endif
