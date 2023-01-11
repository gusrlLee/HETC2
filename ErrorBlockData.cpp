#include "ErrorBlockData.h"

ErrorBlockData::ErrorBlockData()
{

}

// save error block
void ErrorBlockData::pushErrorBlock(ErrorBlock errorBlock)
{
	std::lock_guard<std::mutex> lock(blockMutex);
	pipeline.push(errorBlock);
}

ErrorBlock ErrorBlockData::getErrorBlock()
{
	std::lock_guard<std::mutex> lock(blockMutex);
	ErrorBlock errorBlock = pipeline.front();
	pipeline.pop();
	return errorBlock;
}

