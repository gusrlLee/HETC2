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

ErrorBlock ErrorBlockData::getErrorBlock()
{
	std::lock_guard<std::mutex> lock(blockMutex);
	ErrorBlock errorBlock = m_Pipeline.front();
	m_Pipeline.pop();
	return errorBlock;
}

void ErrorBlockData::pushResultBlock(ErrorBlock errorBlock)
{

}

ErrorBlock ErrorBlockData::getResultBlock()
{

}

unsigned int ErrorBlockData::getSize()
{
	std::lock_guard<std::mutex> lock(blockMutex);
	return m_Pipeline.size();
}

bool ErrorBlockData::isEmpty()
{
	std::lock_guard<std::mutex> lock(blockMutex);
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
