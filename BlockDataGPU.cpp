#include "BlockDataGPU.hpp"

#include "betsy/EncoderBC1.h"
#include "betsy/EncoderBC4.h"
#include "betsy/EncoderBC6H.h"
#include "betsy/EncoderEAC.h"
#include "betsy/EncoderETC1.h"
#include "betsy/EncoderETC2.h"
#include "betsy/CpuImage.h"

#include "Timing.hpp"

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

void BlockDataGPU::setEncodingEnv()
{
    m_Encoder.encoderShaderCompile(Codec::etc2_rgb, false);
    glFinish();
}

void BlockDataGPU::initGPU(const char* input)
{
    betsy::CpuImage cpuImage = betsy::CpuImage(input);
    m_Encoder.initResources(cpuImage, Codec::etc2_rgb, false);
    glFinish(); // wait initResource
}

void BlockDataGPU::ProcessWithGPU( std::shared_ptr<ErrorBlockData> pipeline)
{
    printf("start Encoding in GPU\n");
    while (true)
    {
        size_t repeat = 1u;
        
        unsigned int remainder = pipeline->getNumTasks();
        if ((remainder == 0u) && pipeline->isEmpty())
        {
            printf("GPU Exit\n");
            break;
        }
        else
        {
            ErrorBlock errorBlock = pipeline->getHighErrorBlocks();
            int w = 4;
            int h = 4 * errorBlock.dstAddress.size();
            int c = 3;
            int arraySize = w * h * c;

            auto start = GetTime();

            betsy::CpuImage cpuImage = betsy::CpuImage(errorBlock.srcBuffer.data(), arraySize, w, h, c);
            m_Encoder.initResources(cpuImage, Codec::etc2_rgb, false);

            while (repeat--)
            {
                m_Encoder.execute00();
                m_Encoder.execute01(static_cast<betsy::EncoderETC1::Etc1Quality>(2)); // high Quality
                m_Encoder.execute02();
            }

            uint8_t* result = m_Encoder.getDownloadData();
            uint32_t offset = 0u;
            for (int i=errorBlock.dstAddress.size() - 1; i>=0; --i)
            {
                auto dst = errorBlock.dstAddress[i];
                uint64_t final_data = 0u;
                for (size_t b = 0u; b < 1u; ++b)
                {
                    // base color 
                    final_data |= (uint64_t(result[offset * 8u + size_t(0u)]) << 0);
                    final_data |= (uint64_t(result[offset * 8u + size_t(1u)]) << 8);
                    final_data |= (uint64_t(result[offset * 8u + size_t(2u)]) << 16);
                    final_data |= (uint64_t(result[offset * 8u + size_t(3u)]) << 24);

                    // index table
                    final_data |= (uint64_t(result[offset * 8u + size_t(4u)]) << 56);
                    final_data |= (uint64_t(result[offset * 8u + size_t(5u)]) << 48);
                    final_data |= (uint64_t(result[offset * 8u + size_t(6u)]) << 40);
                    final_data |= (uint64_t(result[offset * 8u + size_t(7u)]) << 32);
                }
                *dst = FixByteOrder(final_data);
                offset++;
            }
            auto end = GetTime();
            printf("betsy encoding time: %0.3f ms\n", (end - start) / 1000.f);
        }
    }
    m_Encoder.deinitResources();

    printf("After Number of Tasks = %u\n", pipeline->getNumTasks());
    printf("After error block size = %u\n", pipeline->getSize());
    printf("End betsy GPU mode \n");
}

