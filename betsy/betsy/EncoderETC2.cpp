
#include "betsy/EncoderETC2.h"

#include "betsy/CpuImage.h"

#include <assert.h>
#include <stdio.h>

#define TODO_better_barrier

namespace betsy
{
	static uint64_t GetTime()
	{
		return std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
	}



	EncoderETC2::EncoderETC2() :
		m_thModesTargetRes(0),
		m_thModesError(0),
		m_thModesC0C1(0),
		m_pModeTargetRes(0),
		m_pModeError(0)
	{
	}
	//-------------------------------------------------------------------------
	EncoderETC2::~EncoderETC2() { assert(!m_thModesTargetRes && "deinitResources not called!"); }
	//-------------------------------------------------------------------------
	void EncoderETC2::initResources(const CpuImage& srcImage, const bool bCompressAlpha,
		const bool bDither)
	{
		EncoderETC1::initResources(srcImage, bCompressAlpha, bDither, true);
		m_thModesTargetRes =
			createTexture(TextureParams(getBlockWidth(), getBlockHeight(), PFG_RG32_UINT,
				"m_thModesTargetRes", TextureFlags::Uav));
		m_thModesError = createTexture(TextureParams(getBlockWidth(), getBlockHeight(), PFG_R32_FLOAT,
			"m_thModesError", TextureFlags::Uav));
		m_thModesC0C1 = createTexture(TextureParams(getBlockWidth(), getBlockHeight(), PFG_RG32_UINT,
			"m_thModesC0C1", TextureFlags::Uav, 4u));

		m_pModeTargetRes = createTexture(TextureParams(
			getBlockWidth(), getBlockHeight(), PFG_RG32_UINT, "m_pModeTargetRes", TextureFlags::Uav));
		m_pModeError = createTexture(TextureParams(getBlockWidth(), getBlockHeight(), PFG_R32_FLOAT,
			"m_pModeError", TextureFlags::Uav));
		
		// compile shader 
		//m_thModesPso = createComputePsoFromFile( "etc2_th.glsl", "../Data/" );
		//m_thModesFindBestC0C1 =
		//	createComputePsoFromFile( "etc2_th_find_best_c0c1_k_means.glsl", "../Data/" );
		//m_pModePso = createComputePsoFromFile( "etc2_p.glsl", "../Data/" );

		//if( bCompressAlpha )
		//	m_stitchPso = createComputePsoFromFile( "etc2_rgba_selector.glsl", "../Data/" );
		//else
		//	m_stitchPso = createComputePsoFromFile( "etc2_rgb_selector.glsl", "../Data/" );
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::initResources(const CpuImage& srcInfo, const uint8_t* srcData, 
		const bool bCompressAlpha, const bool bDither)
	{
		EncoderETC1::initResources(srcInfo, srcData, bCompressAlpha, bDither, true); // for ETC2
		m_thModesTargetRes =
			createTexture(TextureParams(getBlockWidth(), getBlockHeight(), PFG_RG32_UINT,
				"m_thModesTargetRes", TextureFlags::Uav));
		m_thModesError = createTexture(TextureParams(getBlockWidth(), getBlockHeight(), PFG_R32_FLOAT,
			"m_thModesError", TextureFlags::Uav));
		m_thModesC0C1 = createTexture(TextureParams(getBlockWidth(), getBlockHeight(), PFG_RG32_UINT,
			"m_thModesC0C1", TextureFlags::Uav, 4u));

		m_pModeTargetRes = createTexture(TextureParams(
			getBlockWidth(), getBlockHeight(), PFG_RG32_UINT, "m_pModeTargetRes", TextureFlags::Uav));
		m_pModeError = createTexture(TextureParams(getBlockWidth(), getBlockHeight(), PFG_R32_FLOAT,
			"m_pModeError", TextureFlags::Uav));
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::encoderShaderCompile(const bool bCompressAlpha, const bool bDither)
	{
		// init ETC1 shader
		EncoderETC1::encoderShaderCompile(bCompressAlpha, bDither, true);

		// init ETC2 shader
		m_thModesPso = createComputePsoFromFile("etc2_th.glsl", "../Data/");
		m_thModesFindBestC0C1 =
			createComputePsoFromFile("etc2_th_find_best_c0c1_k_means.glsl", "../Data/");
		m_pModePso = createComputePsoFromFile("etc2_p.glsl", "../Data/");

		if (bCompressAlpha)
			m_stitchPso = createComputePsoFromFile("etc2_rgba_selector.glsl", "../Data/");
		else
			m_stitchPso = createComputePsoFromFile("etc2_rgb_selector.glsl", "../Data/");
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::deinitResources()
	{
		destroyTexture(m_pModeError);
		m_pModeError = 0;
		destroyTexture(m_pModeTargetRes);
		m_pModeTargetRes = 0;
		destroyTexture(m_thModesError);
		m_thModesError = 0;
		destroyTexture(m_thModesTargetRes);
		m_thModesTargetRes = 0;
		destroyPso(m_pModePso);
		destroyPso(m_thModesFindBestC0C1);
		destroyPso(m_thModesPso);

		EncoderETC1::deinitResources();
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute00()
	{
		EncoderETC1::execute00();

		bindTexture(0u, m_ditheredTexture);
		bindUav(0u, m_thModesC0C1, PFG_RG32_UINT, ResourceAccess::Write);
		bindComputePso(m_thModesFindBestC0C1);

		glDispatchCompute(1u,  //
			alignToNextMultiple(m_width, 16u) / 16u,
			alignToNextMultiple(m_height, 8u) / 8u);

		TODO_better_barrier;
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute01(EncoderETC2::Etc1Quality quality)
	{
		// etc1 
		glFinish();
		auto start = GetTime();
		EncoderETC1::execute01(quality);
		glFinish();
		auto end = GetTime();
		printf("[ Betsy GPU ] ETC1 operation time = %0.3f ms \n", (end - start) / 1000.0f);


		start = GetTime();
		// etc2 T-/H-mode 
		bindTexture(0u, m_ditheredTexture);
		bindUav(0u, m_thModesTargetRes, PFG_RG32_UINT, ResourceAccess::Write);
		bindUav(1u, m_thModesError, PFG_R32_FLOAT, ResourceAccess::Write);
		bindUav(2u, m_thModesC0C1, PFG_RG32_UINT, ResourceAccess::Write);
		bindComputePso(m_thModesPso);
		glDispatchCompute(alignToNextMultiple(m_width, 4u) / 4u,
			alignToNextMultiple(m_height, 4u) / 4u, 1u);
		glFinish();
		end = GetTime();
		printf("[ Betsy GPU ] ETC2 T/H-mode operation time = %0.3f ms \n", (end - start) / 1000.0f);

		start = GetTime();
		// P-mode
		bindUav(0u, m_pModeTargetRes, PFG_RG32_UINT, ResourceAccess::Write);
		bindUav(1u, m_pModeError, PFG_R32_FLOAT, ResourceAccess::Write);
		bindComputePso(m_pModePso);
		glDispatchCompute(alignToNextMultiple(m_width, 8u) / 8u,
			alignToNextMultiple(m_height, 8u) / 8u, 1u);
		glFinish();
		end = GetTime();
		printf("[ Betsy GPU ] ETC2 P-mode operation time = %0.3f ms \n", (end - start) / 1000.0f);
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute02()
	{
		// Decide which the best modes and merge and stitch
		glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		bindTexture(0u, m_etc1Error);
		bindTexture(1u, m_thModesError);
		bindTexture(2u, m_pModeError);

		bindTexture(3u, m_compressTargetRes);
		bindTexture(4u, m_thModesTargetRes);

		if (hasAlpha())
		{
			bindTexture(5u, m_pModeTargetRes);
			bindTexture(6u, m_eacTargetRes);

			bindUav(0u, m_stitchedTarget, PFG_RGBA32_UINT, ResourceAccess::Write);
		}
		else
		{
			bindUav(0u, m_pModeTargetRes, PFG_RG32_UINT, ResourceAccess::ReadWrite);
		}

		bindComputePso(m_stitchPso);
		glDispatchCompute(alignToNextMultiple(m_width, 32u) / 32u,
			alignToNextMultiple(m_height, 32u) / 32u, 1u);
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::execute03()
	{
		// Copy "8x8" PFG_RG32_UINT -> 32x32 PFG_ETC1_RGB8_UNORM
		// Copy "8x8" PFG_RGBA32_UINT -> 32x32 PFG_ETC2_RGBA8_UNORM
		glCopyImageSubData(hasAlpha() ? m_stitchedTarget : m_pModeTargetRes,  //
			GL_TEXTURE_2D, 0, 0, 0, 0,                         //
			m_dstTexture, GL_TEXTURE_2D, 0, 0, 0, 0,           //
			(GLsizei)(getBlockWidth()), (GLsizei)(getBlockHeight()), 1);

		StagingTexture stagingTex =
			createStagingTexture(getBlockWidth(), getBlockHeight(), PFG_RG32_UINT, false);
		downloadStagingTexture(m_pModeTargetRes, stagingTex);
		glFinish();
	}
	//-------------------------------------------------------------------------
	void EncoderETC2::startDownload()
	{
		glMemoryBarrier(GL_PIXEL_BUFFER_BARRIER_BIT);

		if (m_downloadStaging.bufferName)
			destroyStagingTexture(m_downloadStaging);
		m_downloadStaging = createStagingTexture(getBlockWidth(), getBlockHeight(),
			hasAlpha() ? PFG_RGBA32_UINT : PFG_RG32_UINT, false);
		downloadStagingTexture(hasAlpha() ? m_stitchedTarget : m_pModeTargetRes, m_downloadStaging);
	}

	uint64_t FixByteOrder(uint64_t d)
	{
		return ((d & 0x00000000FFFFFFFF)) |
			((d & 0xFF00000000000000) >> 24) |
			((d & 0x000000FF00000000) << 24) |
			((d & 0x00FF000000000000) >> 8) |
			((d & 0x0000FF0000000000) << 8);
	}

	uint8_t* EncoderETC2::getDownloadData()
	{
		glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		if (m_downloadStaging.bufferName)
			destroyStagingTexture(m_downloadStaging);
		m_downloadStaging = createStagingTexture(getBlockWidth(), getBlockHeight(),
			hasAlpha() ? PFG_RGBA32_UINT : PFG_RG32_UINT, false);
		downloadStagingTexture(hasAlpha() ? m_stitchedTarget : m_pModeTargetRes, m_downloadStaging);
		glFinish();
		return reinterpret_cast<uint8_t*>(m_downloadStaging.data);
	}

	void EncoderETC2::saveToOffset(uint64_t* dst, uint8_t* result)
	{
		uint64_t final_data = 0u;
		for (size_t b = 0u; b < 1u; ++b)
		{
			// base color 
			final_data |= (uint64_t(result[b * 8u + size_t(0u)]) << 0);
			final_data |= (uint64_t(result[b * 8u + size_t(1u)]) << 8);
			final_data |= (uint64_t(result[b * 8u + size_t(2u)]) << 16);
			final_data |= (uint64_t(result[b * 8u + size_t(3u)]) << 24);

			// index table
			final_data |= (uint64_t(result[b * 8u + size_t(4u)]) << 56);
			final_data |= (uint64_t(result[b * 8u + size_t(5u)]) << 48);
			final_data |= (uint64_t(result[b * 8u + size_t(6u)]) << 40);
			final_data |= (uint64_t(result[b * 8u + size_t(7u)]) << 32);
		}
		*dst = FixByteOrder(final_data);
	}
}  // namespace betsy
