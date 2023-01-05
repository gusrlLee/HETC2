#include "BlockDataGPU.hpp"

#include "betsy/EncoderBC1.h"
#include "betsy/EncoderBC4.h"
#include "betsy/EncoderBC6H.h"
#include "betsy/EncoderEAC.h"
#include "betsy/EncoderETC1.h"
#include "betsy/EncoderETC2.h"
#include "betsy/CpuImage.h"

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

void BlockDataGPU::initGPU(const char* input)
{
    betsy::CpuImage cpuImage = betsy::CpuImage(input);
    std::cout << "input = " << input << std::endl;
    std::cout << "repeat = " << m_Repeat << " qualtiy = " << m_Quality << std::endl;
    m_Encoder.initResources(cpuImage, Codec::etc2_rgb, false);
}

void BlockDataGPU::ProcessWithGPU()
{
    printf("start Encoding in GPU\n");
    while (m_Repeat--)
    {
        // if write while loop, segementation fault so, twice write excute method 
        m_Encoder.execute00();
        m_Encoder.execute01(static_cast<betsy::EncoderETC1::Etc1Quality>(m_Quality));
        m_Encoder.execute02();
    }

    saveToOffData(m_Encoder, "betsy_out.ktx");
    m_Encoder.deinitResources();
    printf("End betsy GPU mode \n");
}

