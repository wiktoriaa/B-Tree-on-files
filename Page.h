#pragma once
#include "Record.h"

/*
*  Page struct:
*  ptr | rec_1 | ptr | rec_2 | ptr | rec_3 | ptr | rec_4 | ptr | rec_5 | ptr
* 
*  6 x ptr
*  5 x val
*/

class Page
{

public:
	Page();
	Page(uint32_t pageNumber, size_t size, uint32_t parentNumber); // size in records
	~Page();
	uint32_t getAccessCount();
	uint32_t getPageNumber();
	bool isLeaf();
	void access();
	bool isFull();
	int insertKey(uint32_t key);
	void insertChild(uint32_t pageNum, int position);
	bool isValid();
	size_t getPageSizeInRecords();
	int getChildPos(uint32_t key);
	int getKeyPos(uint32_t key);
	void removeKey(uint32_t key);
	void removeChild(uint32_t key);
	void clearPage();

	uint32_t* childNumbers; // count: size + 1
	uint32_t* keys; // count: size
	uint32_t* recordPosition; // count: size
	uint32_t parentNumber;
	uint32_t recordsCount = 0;
	uint32_t number;
private:
	uint32_t accessCount = 0;
	size_t pageSizeInRecords;
};

