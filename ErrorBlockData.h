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
	uint64_t* dstAddress;
	std::vector<unsigned char> srcBuffer;
};

class ErrorBlockData {
public:
	ErrorBlockData();
	void pushErrorBlock(ErrorBlock errorBlock);
	ErrorBlock getErrorBlock();
	
	unsigned int getSize();
	bool isEmpty();
	void endWorker();
	void setNumTasks( unsigned int n );
	unsigned int getNumTasks();

private:
	std::queue<ErrorBlock> m_Pipeline;
	unsigned int m_NumTasks;

	std::mutex blockMutex;
	std::mutex taskMutex;
};

typedef std::shared_ptr<ErrorBlockData> ErrorBlockDataPtr;