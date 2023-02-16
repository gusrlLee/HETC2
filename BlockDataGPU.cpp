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

BlockDataGPU::~BlockDataGPU()
{
    m_Encoder.deinitResources();
}

void BlockDataGPU::setEncodingEnv()
{
    m_Encoder.encoderShaderCompile(false, false);
    glFinish();
}

void BlockDataGPU::initGPU(const char* input)
{
    betsy::CpuImage cpuImage = betsy::CpuImage(input);
    m_Encoder.initResources(cpuImage, false, false);
    glFinish(); // wait initResource
}

void BlockDataGPU::ProcessWithGPU( std::shared_ptr<ErrorBlockData> pipeline)
{
    size_t repeat = 1u;
    ErrorBlock errorBlock = pipeline->getHighErrorBlocks();

    int w = 4;
    int h = 4 * errorBlock.dstAddress.size();
    int c = 3;
    int arraySize = w * h * c;

    betsy::CpuImage cpuImage = betsy::CpuImage(errorBlock.srcBuffer.data(), arraySize, w, h, c);
    m_Encoder.initResources(cpuImage, false, false);

    while (repeat--)
    {
        m_Encoder.execute00();
        m_Encoder.execute01(static_cast<betsy::EncoderETC1::Etc1Quality>(1)); // setting mid quality
        m_Encoder.execute02();
    }

    //saveToOffData(m_Encoder, "errorBlock_no_etc2TH.ktx");

    uint8_t* result = m_Encoder.getDownloadData();
    uint32_t offset = 0u;
    for (int i=errorBlock.dstAddress.size() - 1; i>=0; --i)
    {
        auto dst = errorBlock.dstAddress[i];
        m_Encoder.saveToOffset(dst, result);
        result += 8; // for jump 64bits.
    }
}

