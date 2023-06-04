#include "Page.h"
#include <iostream>

Page::Page()
{
}

Page::Page(uint32_t pageNumber, size_t size, uint32_t parentNumber)
{
	this->number = pageNumber;
	this->pageSizeInRecords = size;

	this->childNumbers = new uint32_t[size + 2]();
	this->keys = new uint32_t[size + 1]();
	this->recordPosition = new uint32_t[size + 1]();

	this->parentNumber = parentNumber;
}

Page::~Page()
{
	//delete [] this->keys;
	//delete [] this->childNumbers;
}

uint32_t Page::getAccessCount()
{
	return accessCount;
}

uint32_t Page::getPageNumber()
{
	return this->number;
}

bool Page::isLeaf()
{
	if (this->childNumbers[0] == 0) {
		return true;
	}
	return false;
}

void Page::access()
{
	this->accessCount++;
}

bool Page::isFull()
{
	return this->recordsCount == this->pageSizeInRecords + 1;
}

int Page::insertKey(uint32_t key)
{
	if (isFull()) {
		std::cout << "[PAGE][insertKey]: ERROR, strona jest juz pelna\n";
		return -1;
	}

	int position = this->recordsCount - 1;
	while (position >= 0 && key < this->keys[position]) {
		this->keys[position + 1] = this->keys[position];
		this->recordPosition[position + 1] = this->recordPosition[position];
		--position;
	}

	this->keys[position + 1] = key;
	this->recordsCount++;

	return position + 1;
}

void Page::insertChild(uint32_t pageNum, int childPosition)
{
	int position = recordsCount + 1;
	while (childPosition <= position) {
		this->childNumbers[position + 1] = this->childNumbers[position];
		--position;
	}

	this->childNumbers[childPosition] = pageNum;
}

bool Page::isValid()
{
	return (this->number > 0);
}

size_t Page::getPageSizeInRecords()
{
	return this->pageSizeInRecords;
}

int Page::getChildPos(uint32_t key)
{
	int pos = -1;

	for (int i = 0; i < this->recordsCount + 1; i++) {
		if (this->childNumbers[i] == key) {
			pos = i;
			break;
		}
	}

	return pos;
}

int Page::getKeyPos(uint32_t key)
{
	int pos = -1;

	for (int i = 0; i < this->recordsCount; i++) {
		if (this->keys[i] == key) {
			pos = i;
			break;
		}
	}

	return pos;
}

void Page::removeKey(uint32_t key)
{
	int position = getKeyPos(key);
	while (position < recordsCount - 1) {
		this->keys[position] = this->keys[position + 1];
		this->recordPosition[position] = this->recordPosition[position + 1];
		++position;
	}

	this->keys[position] = 0;
	this->recordPosition[position] = 0;
	this->recordsCount--;
}

void Page::removeChild(uint32_t key)
{
	int position = getChildPos(key);
	while (position < recordsCount + 1) {
		this->childNumbers[position] = this->childNumbers[position + 1];
		++position;
	}

	//this->childNumbers[position] = 0;
}

void Page::clearPage()
{
	uint32_t* buffer = new uint32_t[this->recordsCount + 2]();

	memcpy(this->keys, buffer, this->recordsCount + 1);
	memcpy(this->recordPosition, buffer, this->recordsCount + 1);
	memcpy(this->childNumbers, buffer, this->recordsCount + 2);

	this->recordsCount = 0;

	this->childNumbers = new uint32_t[this->pageSizeInRecords + 2]();
	this->keys = new uint32_t[this->pageSizeInRecords + 1]();
	this->recordPosition = new uint32_t[this->pageSizeInRecords + 1]();

	this->parentNumber = 0;

	delete[] buffer;
}
