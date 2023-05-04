#include "BlockDataGPU.hpp"

#include "betsy/EncoderBC1.h"
#include "betsy/EncoderBC4.h"
#include "betsy/EncoderBC6H.h"
#include "betsy/EncoderEAC.h"
#include "betsy/EncoderETC1.h"
#include "betsy/EncoderETC2.h"
#include "betsy/CpuImage.h"

#include "Timing.hpp"

#define TEXTURE_WIDTH 8
#define TEXTURE_HEIGHT 8
#define TEXTURE_CHANNEL 3


static bool cmp(PixBlock a, PixBlock b)
{
    return a.error > b.error;
}

// save FTX format file 
template <typename T>
void saveToOffData(T& encoder, const char* file_path)
{
    betsy::CpuImage downloadImg;
    encoder.startDownload();
    encoder.downloadTo(downloadImg);
    betsy::FormatKTX::save(file_path, downloadImg);
    downloadImg.data = 0;
}

BlockDataGPU::BlockDataGPU()
{
	m_Repeat = 1u;
	m_Quality = 2;
}

BlockDataGPU::~BlockDataGPU()
{
    m_Encoder.deinitResources();
}

void BlockDataGPU::setEncodingEnv()
{
    m_Encoder.encoderShaderCompile(false, false);
    glFinish();
}

void BlockDataGPU::setEncodingEnv(bool alpha)
{
    m_Encoder.encoderShaderCompile(alpha, false);
    glFinish();
}

void BlockDataGPU::initGPU(const char* input)
{
    betsy::CpuImage cpuImage = betsy::CpuImage(input);
    m_Encoder.initResources(cpuImage, false, false);
    glFinish(); // wait initResource
}

void BlockDataGPU::ProcessWithGPU(std::shared_ptr<ErrorBlockData> pipeline, uint64_t blockLimit)
{
    size_t repeat = 1u;
    // ErrorBlock errorBlock = pipeline->getHighErrorBlocks();
    // get pipeline 
    std::vector<PixBlock> pipe = pipeline->getPipe(); // 3ms
    ////std::cout << "Pipe size = " << pipe.size() << std::endl;
    std::sort(pipe.begin(), pipe.end(), cmp); // 4ms
    uint64_t limit = 8 * blockLimit;
    limit = pipe.size() < limit ? pipe.size() : limit; // 4ms

    std::vector<PixBlock> buffer;
    std::vector<unsigned char> image;
    buffer.resize(limit);
    std::copy(pipe.begin(), pipe.begin() + limit, buffer.begin());
    //std::cout << "Buffer Size = " << buffer.size() << std::endl;

    for (int i = 0; i < buffer.size(); i++)
    {
        for (int t = 0; t < 48; t++)
        {
            image.push_back(buffer[i].bgrData[t]);
        }
    } // 5ms

    ////const int width = 4 * buffer.size();
    ////const int height = 4 * buffer.size();
    ////const int width = 800;
    ////const int height = 4 * pipe.size() / width;
    ////const int channel = 3;
    ////const int pitch = channel * width;
    ////const int arraySize = width * height * channel;
    ////unsigned char* data = new unsigned char[arraySize];
    ////int off_x = 0;
    ////int off_y = 0;
    ////for (int t = 0; t < buffer.size(); t++)
    ////{
    ////    for (int j = 0; j < 4; j++)
    ////    {
    ////        for (int i = 0; i < 4; i++)
    ////        {
    ////            int offset = pitch * (off_y + j) + channel * (off_x + i);
    ////            data[offset + 0] = buffer[t].bgrData[channel * (4 * j + i) + 0];
    ////            data[offset + 1] = buffer[t].bgrData[channel * (4 * j + i) + 1];
    ////            data[offset + 2] = buffer[t].bgrData[channel * (4 * j + i) + 2];
    ////        }
    ////    }
    ////    off_x += 4;
    ////    if (off_x > width)
    ////    {
    ////        off_y += 4;
    ////        off_x = 0;
    ////    }
    ////}
    ////betsy::CpuImage cpuImage = betsy::CpuImage(data, arraySize, width, height, channel);
    ////m_Encoder.initResources(cpuImage, false, false);
    
    int w = 4;
    int h = 4 * buffer.size();
    int c = 3;
    int arraySize = w * h * c;

    betsy::CpuImage cpuImage = betsy::CpuImage(image.data(), arraySize, w, h, c);
    m_Encoder.initResources(cpuImage, false, false); // 5ms 

    ////start = GetTime();
    while (repeat--) // this loop is 13 ms 
    {
        m_Encoder.execute00();
        m_Encoder.execute01(static_cast<betsy::EncoderETC1::Etc1Quality>(1)); // setting mid quality
        m_Encoder.execute02();
    }
    //saveToOffData(m_Encoder, "res.ktx");

    uint8_t* result = m_Encoder.getDownloadData();
    uint32_t offset = 0u;

    for (int i=buffer.size() - 1; i>=0; --i)
    {
        auto dst = buffer[i].address;
        m_Encoder.saveToOffset(dst, result);
        result += 8; // for jump 64bits.
    }
}

void BlockDataGPU::ProcessWithGPU(PixBlock* pipeline, int pipeSize, uint64_t blockLimit, bool alpha)
{
    size_t repeat = 1u;
    if ( pipeSize > blockLimit)
        std::sort(pipeline, pipeline + pipeSize, cmp);

    //uint64_t limit = 8 * blockLimit;
    uint64_t limit = blockLimit;
    limit = pipeSize < limit ? pipeSize : limit; // 4ms

    //std::vector<PixBlock> buffer;
    std::vector<unsigned char> image;
    PixBlock* buf = new PixBlock[limit];

    //std::cout << "block pipeline size = " << pipeSize << std::endl;
    //std::cout << "block pipeline size limit = " << blockLimit << std::endl;

    //buffer.resize(limit);
    //std::copy(pipeline, pipeline + limit, buffer.begin());
    std::memcpy(buf, pipeline, limit * sizeof(PixBlock));

    //for (int i = 0; i < limit; i++)
    //{
    //    for (int t = 0; t < 48; t++)
    //    {
    //        image.push_back(buf[i].bgrData[t]);
    //        //std::cout << "image data = " << (int)buf[i].bgrData[t] << std::endl;
    //    }
    //}

    //int w = 4;
    //int h = 4 * limit;
    //int c = 3;
    //int arraySize = w * h * c;

    //std::cout << "image 1D size = " << pipeSize << std::endl;
    //std::cout << "Image limit size " << blockLimit << std::endl;
    //std::cout << "cutting block pipe size = " << limit << std::endl;

    // GPU image implementation.
    int a = std::sqrt(limit);
    int gpuImageWidth = (int)a * 4 + 4;
    int gpuImageHeight = (int)a * 4 + 4;

    int gpuImageChannel = 3;
    int gpuImagePicth = gpuImageWidth * gpuImageChannel;
    int gpuImageArraySize = gpuImageChannel * gpuImageWidth * gpuImageHeight;

    unsigned char* GpuImage = new unsigned char[gpuImageArraySize](); // initalization zero

    // make GPU image
    int idx = 0;
    unsigned char r = 255;
    int image_offset_x = 0;
    int image_offset_y = 0;
    int width = 0;
    int x = 0;
    int y = 0;

    for (int i = 0; i < limit; i++) // buffer size if 4,
    {
        for (int y = 0; y < 4; y++)
        {
            for (int x = 0; x < 4; x++)
            {
                int offset = gpuImagePicth * (image_offset_y + (3 - y)) + gpuImageChannel * (image_offset_x + x);
                GpuImage[offset + 0] = buf[i].bgrData[gpuImageChannel * (4 * y + x) + 0];
                GpuImage[offset + 1] = buf[i].bgrData[gpuImageChannel * (4 * y + x) + 1];
                GpuImage[offset + 2] = buf[i].bgrData[gpuImageChannel * (4 * y + x) + 2];
            }
        }

        image_offset_x += 4;
        if (image_offset_x >= gpuImageWidth)
        {
            image_offset_y += 4;
            image_offset_x = 0;
        }
    }

    //betsy::CpuImage cpuImage = betsy::CpuImage(image.data(), arraySize, w, h, c);
    betsy::CpuImage cpuImage = betsy::CpuImage(GpuImage, gpuImageArraySize, gpuImageWidth, gpuImageHeight, gpuImageChannel);
    m_Encoder.initResources(cpuImage, false, false);

    while (repeat--) // this loop is 13 ms 
    {
        m_Encoder.execute00();
        m_Encoder.execute01(static_cast<betsy::EncoderETC1::Etc1Quality>(1)); // setting mid quality
        m_Encoder.execute02();
    }

    // saveToOffData(m_Encoder, "res.ktx");

    uint8_t* result = m_Encoder.getDownloadData();
    uint32_t offset = 0u;
    for (int i = 0; i < limit; i++)
    {
        auto dst = buf[i].address;
        m_Encoder.saveToOffset(dst, result);
        result += 8; // for jump 64bits.
    }
    delete[] buf;
    delete[] GpuImage;
}
