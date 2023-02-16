#include "ErrorBlockData.h"

ErrorBlockData::ErrorBlockData()
{

}

// save error block
void ErrorBlockData::pushErrorBlock(ErrorBlock errorBlock)
{
	std::lock_guard<std::mutex> lock(blockMutex);
	m_Pipeline.push(errorBlock);
}

void ErrorBlockData::pushErrorBlock(uint64_t* dst, std::vector<unsigned char>& arr)
{
	std::lock_guard<std::mutex> lock(blockMutex);
	m_ErrorBlock.dstAddress.push_back(dst);
	for (int i = 0; i < 48; i++) // 16 x 4
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
