#pragma once

#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <iostream>

#include "Bitmap.hpp"
#include "BlockData.hpp"

#include "GL/glcorearb.h"

#include "betsy/CpuImage.h"
#include "betsy/EncoderBC1.h"
#include "betsy/EncoderBC4.h"
#include "betsy/EncoderBC6H.h"
#include "betsy/EncoderEAC.h"
#include "betsy/EncoderETC1.h"
#include "betsy/EncoderETC2.h"

#include "betsy/File/FormatKTX.h"
#include "betsy/CmdLineParams.h"

#include "FreeImage.h"

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

	void initGPU(const char* input);
	void setEncodingEnv();
	void ProcessWithGPU();

private:
	size_t m_Repeat;
	int m_Quality;

	betsy::CpuImage m_CpuImage;
	betsy::EncoderETC2 m_Encoder;

};

typedef std::shared_ptr<BlockDataGPU> BlockDataGPUPtr;

