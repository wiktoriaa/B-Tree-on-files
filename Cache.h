#pragma once
#include <cstdint>
#include "Page.h"
#include <fstream>
#include <unordered_set>
#include <unordered_map>

class Cache
{
public:
	Cache(uint32_t cacheSize, std::string filename, uint32_t pageSize, uint32_t recordsArraySize, uint32_t lastPageNum); // size in pages
	~Cache();
	bool isPageInCache(uint32_t number);
	void displayPages();
	Page getPage(uint32_t number);
	void savePage(uint32_t number, bool clearPage = false);
	void removePage(uint32_t number);
	void addPage(Page page, bool save = true);
	void updatePage(Page page);
	void changeCacheSize(uint32_t size);
	std::unordered_map<uint32_t, Page> pages;		// <key, Page>

	int readsCount = 0;
	int writesCount = 0;
private:
	uint32_t maxSize;								// cache size, in pages
	std::unordered_map<uint32_t, uint32_t> indexes; // <key, accessCount>
	std::fstream file;
	uint32_t indexFileHeaderSize = 28;
	std::string filename;

	uint32_t pageSize;
	uint32_t recordsArraySize;
	uint32_t lastPageNum;

	bool readPageFromDisk(uint32_t number);
	
};

