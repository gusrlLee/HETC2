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
    int w = 4;
    int h = 4;
    int c = 3;

    while (true)
    {
        size_t repeat = 1u;
        
        int arraySize = 4 * 4 * 3;
        unsigned int remainder = pipeline->getNumTasks();
        if ((remainder == 0u) && pipeline->isEmpty())
        {
            printf("GPU Exit\n");
            break;
        }
        else
        {
            ErrorBlock errorBlock = pipeline->getErrorBlock();
            // printf("error block size = %u\n", pipeline->getSize());
            betsy::CpuImage cpuImage = betsy::CpuImage(errorBlock.srcBuffer.data(), arraySize, w, h, c);
            m_Encoder.initResources(cpuImage, Codec::etc2_rgb, false);

            while (repeat--)
            {
                m_Encoder.execute00();
                m_Encoder.execute01();
                m_Encoder.execute02();
            }
            m_Encoder.saveToOffset(errorBlock.dstAddress);
        }
    }
    m_Encoder.deinitResources();

    printf("After Number of Tasks = %u\n", pipeline->getNumTasks());
    printf("After error block size = %u\n", pipeline->getSize());
    //printf("betsy encoding time: %0.3f ms\n", (end - start) / 1000.f);
    printf("End betsy GPU mode \n");
}

