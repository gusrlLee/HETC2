#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <queue>
#include <stdio.h>
#include <algorithm>
#include <mutex>
#include <stdint.h>

typedef struct ErrorBlock {
	std::vector<uint64_t*> dstAddress;
	std::vector<unsigned char> srcBuffer;
}; // one image for encoding 

class ErrorBlockData {
public:
	ErrorBlockData();
	void pushErrorBlock(ErrorBlock errorBlock);
	void pushErrorBlock(uint64_t* dst, std::vector<unsigned char>& arr);

	void pushHighErrorBlocks();
	ErrorBlock getHighErrorBlocks();

	unsigned int getSize();
	bool isEmpty();
	void endWorker();
	void setNumTasks( unsigned int n );
	unsigned int getNumTasks();

private:
	std::queue<ErrorBlock> m_Pipeline;
	ErrorBlock m_ErrorBlock;
	unsigned int m_NumTasks;

	std::mutex pipelineMutex;
	std::mutex blockMutex;
	std::mutex taskMutex;
};

typedef std::shared_ptr<ErrorBlockData> ErrorBlockDataPtr;