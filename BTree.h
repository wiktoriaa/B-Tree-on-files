#pragma once
#include "Node.h"
#include "Record.h"
#include "Page.h"
#include <fstream>
#include "Cache.h"

class BTree
{
public:
	BTree(uint32_t degree);
	~BTree();
	void displayDataFile();
	void displayPage(uint32_t number);
	void printCache();
	bool savePage(uint32_t pageNum);
	void searchKey(uint32_t key);
	void printTree();
	void insertRecord(Record record);
	void compensation(Page &fullPage, Page &sibling, int parentChildPointer);
	void split(Page& page);
	void displayRecord(uint32_t key);
	bool recordExist(uint32_t key);
	void displayStats();
	void updateRecord(uint32_t key);
	bool remove(uint32_t key);
	void resetStats();
private:
	bool insert(Record record, uint32_t recordPointer);
	bool createDatabase(std::string filename);
	bool openDatabase(std::string filename);
	void generateIndexFileHeader();
	void updateIndexFileHeader();
	void addPage(Page page);
	int saveRecord(Record record);
	uint32_t search(uint32_t key, uint32_t pageNum);
	void print(int pageNum);
	void createDataFile();
	Record getRecord(int positionInDataFile);
	bool update(Record record, int pos);
	void merge(Page page, Page sibling);
	int findMax();

	uint32_t d = 5; // keys on page count
	uint32_t rootPageNum = 1;
	std::fstream indexFile;
	std::fstream dataFile;
	std::string filename;
	uint32_t indexPageSize;
	uint32_t indexPageRecordsCount;
	uint32_t pagesCount = 1;
	uint32_t lastAccessedPageNumber = 0;
	uint32_t treeHeight = 1;

	Cache *cache;
};

