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
	int getSize() { return pipeline.size(); }

private:
	std::queue<ErrorBlock> pipeline;
	std::mutex blockMutex;
};

typedef std::shared_ptr<ErrorBlockData> ErrorBlockDataPtr;