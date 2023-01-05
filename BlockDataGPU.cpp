#include "BlockDataGPU.hpp"

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
    betsy::initBetsyPlatform();
	m_Repeat = 1u;
	m_Quality = 2;
}

void BlockDataGPU::initGPU() 
{
	BYTE highError_block[4 * 4 * 3] = {0,};
	m_CpuImage = betsy::CpuImage(highError_block, 4 * 4 * 3, 4, 4, 3);
	m_Encoder.initResources(m_CpuImage, Codec::etc2_rgb, false);
}

void BlockDataGPU::initGPU(const char* input)
{
    m_CpuImage = betsy::CpuImage(input);
    m_Encoder.initResources(m_CpuImage, Codec::etc2_rgb, false);
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

