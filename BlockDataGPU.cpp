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

void BlockDataGPU::ProcessWithGPU()
{
    printf("start Encoding in GPU\n");

    auto start = GetTime();
    while (m_Repeat--)
    {
        // if write while loop, segementation fault so, twice write excute method 
        m_Encoder.execute00();
        m_Encoder.execute01(static_cast<betsy::EncoderETC1::Etc1Quality>(m_Quality));
        m_Encoder.execute02();
    }


    // for checking betsy output
    // saveToOffData(m_Encoder, "betsy_out.ktx");
    m_Encoder.deinitResources();
    glFinish();

    auto end = GetTime();
    printf("betsy encoding time: %0.3f ms\n", (end - start) / 1000.f);
    printf("End betsy GPU mode \n");
}

