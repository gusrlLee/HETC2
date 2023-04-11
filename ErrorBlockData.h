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
	std::vector<std::pair<uint64_t*, uint64_t>> dstAddress;
	std::vector<unsigned char> srcBuffer;
}; // one image for encoding 

typedef struct PixBlock {
	uint64_t* address;
	uint64_t error;
	//std::vector<unsigned char> bgrData;
	unsigned char bgrData[48]; // 16 x 3
}PixBlock;

class ErrorBlockData {
public:
	std::vector<PixBlock> m_pipe;

	ErrorBlockData();
	void pushErrorBlock(ErrorBlock errorBlock);
	void pushErrorBlock(uint64_t* dst, uint64_t errorValue, std::vector<unsigned char>& arr);

	void pushPixBlock(uint64_t* dst, uint64_t errorValue, unsigned char* arr);
	std::vector<PixBlock> getPipe();

	void pushHighErrorBlocks();
	ErrorBlock getHighErrorBlocks();

	unsigned int getSize();
	bool isEmpty();
	void endWorker();
	void setNumTasks( unsigned int n );
	void merge(std::vector<PixBlock> &others);

	unsigned int getNumTasks();
	unsigned int size();

private:
	ErrorBlock m_ErrorBlock;
	unsigned int m_NumTasks;
	
	unsigned int m_OffsetX;
	unsigned int m_OffsetY;
	unsigned int m_Width;
	unsigned int m_Hight;
	unsigned int m_Channel;

	std::queue<ErrorBlock> m_Pipeline;

	std::mutex pipelineMutex;
	std::mutex blockMutex;
	std::mutex taskMutex;
};

typedef std::shared_ptr<ErrorBlockData> ErrorBlockDataPtr;