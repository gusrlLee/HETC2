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
        unsigned int remainder = pipeline->getNumTasks();
        if ((remainder == 0u) && pipeline->isEmpty())
        {
            printf("GPU Exit\n");
            break;
        }
        if (!pipeline->isEmpty())
        {
            ErrorBlock errorBlock = pipeline->getErrorBlock();
            betsy::CpuImage cpuImage = betsy::CpuImage(errorBlock.srcBuffer.data(), errorBlock.srcBuffer.size(), 4, 4, 3);
             m_Encoder.initResources(cpuImage, Codec::etc2_rgb, false);
            //m_Encoder.execute00();
            //m_Encoder.execute01();
            //m_Encoder.execute02();
            m_Encoder.deinitResources();
        }
    }
    //auto start = GetTime();

    // for checking betsy output
    // saveToOffData(m_Encoder, "betsy_out.ktx");
    //glFinish();
    //auto end = GetTime();
    printf("After Number of Tasks = %u\n", pipeline->getNumTasks());
    printf("After error block size = %u\n", pipeline->getSize());
    //printf("betsy encoding time: %0.3f ms\n", (end - start) / 1000.f);
    printf("End betsy GPU mode \n");
}

