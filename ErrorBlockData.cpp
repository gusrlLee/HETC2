#include "ErrorBlockData.h"

ErrorBlockData::ErrorBlockData()
{
	m_Width = 480;
	m_Hight = 480;
	m_Channel = 3;
	m_OffsetX = 0;
	m_OffsetY = 0;
}

void ErrorBlockData::pushPixBlock(uint64_t* dst, uint64_t errorValue, unsigned char *arr)
{
	PixBlock p;
	p.address = dst;
	p.error = errorValue;
	//for (int i = 0; i < 48; i++) // 16 x 3
	//{
	//	//p.bgrData.push_back(arr[i]);
	//	p.bgrData[i] = arr[i];
	//}
	std::memcpy(p.bgrData, arr, 48);
	//std::lock_guard<std::mutex> lock(blockMutex);
	m_pipe.push_back(p);
}

void ErrorBlockData::merge(std::vector<PixBlock> &others)
{
	m_pipe.insert(m_pipe.end(), others.begin(), others.end());
}

std::vector<PixBlock> ErrorBlockData::getPipe()
{
	std::lock_guard<std::mutex> lock(blockMutex);
	return m_pipe;
}

// save error block
void ErrorBlockData::pushErrorBlock(ErrorBlock errorBlock)
{
	std::lock_guard<std::mutex> lock(blockMutex);
	m_Pipeline.push(errorBlock);
}

void ErrorBlockData::pushErrorBlock(uint64_t* dst, uint64_t errorValue, std::vector<unsigned char>& arr)
{
	std::lock_guard<std::mutex> lock(blockMutex);
	std::pair<uint64_t*, uint64_t> dstAddress = std::make_pair(dst, errorValue);
	m_ErrorBlock.dstAddress.push_back(dstAddress);
	for (int i = 0; i < 48; i++) // 16 x 3
	{
		m_ErrorBlock.srcBuffer.push_back( arr[i] );
	}
}

void ErrorBlockData::pushHighErrorBlocks()
{
	std::lock_guard<std::mutex> lock(pipelineMutex);
	m_Pipeline.push(m_ErrorBlock);

	// init buffer 
	m_ErrorBlock.dstAddress.clear();
	m_ErrorBlock.srcBuffer.clear();
}

ErrorBlock ErrorBlockData::getHighErrorBlocks()
{
	std::lock_guard<std::mutex> lock(pipelineMutex);
	ErrorBlock errorBlock = m_Pipeline.front();
	m_Pipeline.pop();

	return errorBlock;
}

unsigned int ErrorBlockData::getSize()
{
	std::lock_guard<std::mutex> lock(pipelineMutex);
	return m_Pipeline.size();
}

unsigned int ErrorBlockData::size()
{
	return m_pipe.size();
}

bool ErrorBlockData::isEmpty()
{
	std::lock_guard<std::mutex> lock(pipelineMutex);
	return m_Pipeline.empty();
}

void ErrorBlockData::setNumTasks( unsigned int n )
{
	std::lock_guard<std::mutex> lock(taskMutex);
	m_NumTasks = n;
}

unsigned int ErrorBlockData::getNumTasks()
{
	std::lock_guard<std::mutex> lock(taskMutex);
	return m_NumTasks;
}

void ErrorBlockData::endWorker()
{
	std::lock_guard<std::mutex> lock(taskMutex);
	m_NumTasks -= 1;
}
