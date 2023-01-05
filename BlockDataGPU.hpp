#ifndef __BLOCKDATA_GPU_HPP__
#define __BLOCKDATA_GPU_HPP__

#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <iostream>

#include "Bitmap.hpp"
#include "BlockData.hpp"

// betsy GPU header 
#include "betsy/CmdLineParams.h"
#include "betsy/CpuImage.h"
#include "betsy/EncoderETC1.h"
#include "betsy/EncoderETC2.h"
#include "betsy/File/FormatKTX.h"

namespace betsy
{
	extern void initBetsyPlatform();
	extern void shutdownBetsyPlatform();
	extern void pollPlatformWindow();
}  // namespace betsy


class BlockDataGPU
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

	BlockDataGPU();

	void initGPU();
	void initGPU(const char* input);
	void ProcessWithGPU();

private:
	size_t m_Repeat;
	int m_Quality;

	betsy::CpuImage m_CpuImage;
	betsy::EncoderETC2 m_Encoder;

};

typedef std::shared_ptr<BlockDataGPU> BlockDataGPUPtr;

#endif