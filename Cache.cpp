#include "Cache.h"
#include <iostream>
#include <algorithm>
#include <utility>

Cache::Cache(uint32_t cacheSize, std::string filename, uint32_t pageSize, uint32_t recordsArraySize, uint32_t lastPageNum)
{
	this->maxSize = cacheSize;
	this->filename = filename;
	this->file.open(filename, std::ios::in | std::ios::out | std::ios::binary);

	this->file.seekg(20, std::ios::beg);
	this->file.read((char*) &this->lastPageNum, 4);
	this->file.clear();

	this->pageSize = pageSize;
	this->recordsArraySize = recordsArraySize;
	this->lastPageNum = lastPageNum;

	std::cout << "Indeks ostatniej strony: " << this->lastPageNum << "\n";
}

Cache::~Cache()
{
	std::cout << "destruktor CACHE\n";
	this->file.clear();
	this->file.seekp(20, std::ios::beg);
	this->file.write((char*)&this->lastPageNum, 4);

	for (auto const& x : this->indexes) {
		std::cout << "zapisano strone nr " << x.first << "\n";
		savePage(x.first);
	}
	this->file.close();
}

bool Cache::isPageInCache(uint32_t number)
{
	if (this->pages.find(number) == this->pages.end()) {
		return false;
	}
	else {
		return true;
	}
}

void Cache::addPage(Page page, bool save)
{
	/* Remove least used element from cache if cache is full */
	if (this->pages.size() == this->maxSize) {
		uint32_t minKey = this->indexes.begin()->first; // 1st key default
		uint32_t minVal = this->indexes.begin()->second; // 1st val default

		for (auto const& x : this->indexes) {
			if (x.second < minVal) {
				minKey = x.first;
				minVal = x.second;
			}
		}
		savePage(minKey);

		this->indexes.erase(minKey);
		this->pages.erase(minKey);
	}

	this->pages.insert({ page.getPageNumber(), page });
	this->indexes.insert({ page.getPageNumber(), page.getAccessCount() });

	if (page.number > this->lastPageNum) {
		this->lastPageNum = page.number;
	}

	/* save new page on disk */
	if (save) {
		savePage(page.number);
	}
}

void Cache::displayPages()
{
	for (auto const& x : this->pages) {
		std::cout << this->pages.find(x.first)->second.getPageNumber() << '\n';
	}
}

Page Cache::getPage(uint32_t number)
{
	if (!isPageInCache(number)) {
		if (!readPageFromDisk(number)) {
			return Page(0,0,0);
		}
	}

	this->pages.find(number)->second.access();
	this->indexes.find(number)->second++;

	return this->pages.find(number)->second;
}

void Cache::savePage(uint32_t number, bool clearPage)
{
	if (this->pages.size() == 0) {
		std::cout << "Nie ma stron do zapisania\n";
		return;
	}

	//std::cout << "\nZapis strony " << number << " na dysk\n";

	if (this->pages.find(number) == this->pages.end()) {
		//std::cout << "Strona o numerze " << number << " nie istnieje w cache\n";
		return;
	}

	this->writesCount++;

	Page page = this->pages.find(number)->second;

	if (clearPage) {
		page.number = 0;
	}

	this->file.clear();

	if (!this->file) {
		std::cout << "Plik nie jest otwarty\n";
		return;
	}

	int position = this->indexFileHeaderSize + ((number - 1) * pageSize);

	//std::cout << "Ostatnia strona ma nr " << lastPageNum << "\n";
	this->file.clear();

	if (number <= lastPageNum) {
		this->file.seekp(position, std::ios_base::beg);
	}
	else {
		//std::cout << "Zapis na koncu pliku\n";
		this->file.seekp(0, std::ios_base::end);
	}

	this->file.write((char*)&page.number, 4);		// page number
	this->file.write((char*)&page.parentNumber, 4);	// parent page number
	this->file.write((char*)&page.recordsCount, 4);	// filled records count

	this->file.write((char*)page.keys, recordsArraySize * 4); // keys
	this->file.write((char*)page.recordPosition, recordsArraySize * 4); // records pointers
	this->file.write((char*)page.childNumbers, (recordsArraySize + 1) * 4); // children

	this->file.flush();
}

void Cache::removePage(uint32_t number)
{
	/* save page before removing from cache */
	savePage(number, true);

	this->pages.erase(number);
}

void Cache::updatePage(Page page)
{
	page.access();

	if (!isPageInCache(page.number)) {
		if (!readPageFromDisk(page.number)) {
			std::cout << "[CACHE][updatePage] ERROR Nie znaleziono strony w pliku\n";
			return;
		}
	}

	this->pages.find(page.number)->second = page;
	this->indexes.find(page.number)->second = page.getAccessCount();

	// test purpose only

	Page test = getPage(page.number);
}

void Cache::changeCacheSize(uint32_t size)
{
	this->maxSize = size;
}

/*
Page numbers from 1...n
*/

bool Cache::readPageFromDisk(uint32_t pageNumber)
{
	char prefix[8];
	uint32_t pageSize;
	uint32_t pageNum;
	uint32_t recordsArraySize;
	uint32_t root;

	if (pageNumber == 0) {
		//std::cout << "bledny numer strony\n";
		return false;
	}

	this->readsCount++;

	this->file.clear();

	this->file.seekg(0, std::ios_base::beg);
	this->file.read(prefix, 8);
	this->file.read((char*)&pageSize, 4);
	this->file.read((char*)&recordsArraySize, 4);
	this->file.read((char*)&root, 4);
	//std::cout << "Odczytano numer roota: " << unsigned((uint32_t)root) << "\n";

	uint32_t i = 0;
	while (1) {
		this->file.clear();
		this->file.seekg(this->indexFileHeaderSize + ((uint64_t)i * pageSize), std::ios_base::beg);
		this->file.read((char*)&pageNum, 4);
		//std::cout << "Odczytano numer strony: " << unsigned((uint32_t)pageNum) << "\n";

		if (this->file.fail()) {
			//std::cout << "[CACHE] nie znaleziono strony nr " << pageNumber << "\n";
			return false;
		}


		if (pageNum == pageNumber) {
			Page page(pageNum, recordsArraySize, 0);
			uint32_t *keysArr = new uint32_t[recordsArraySize];
			uint32_t* pointersArr = new uint32_t[recordsArraySize + 1];

			this->file.read((char*)&page.parentNumber, 4);
			//std::cout << "Odczytano numer rodzica: " << unsigned((uint32_t)page.parentNumber) << "\n";

			this->file.read((char*)&page.recordsCount, 4);
			//std::cout << "Odczytano ilosc rekordow: " << unsigned((uint32_t)page.recordsCount) << "\n";

			char buffer[4];
			this->file.clear();
			this->file.seekg(this->indexFileHeaderSize + ((uint64_t)i * pageSize) + 12, std::ios_base::beg);
			uint32_t num;

			//uint32_t *buffer = new uint32_t[recordsArraySize + 1]();

			// read keys
			for (int i = 0; i < page.recordsCount; i++) {
				this->file.read((char*)&num, 4);
				page.keys[i] = num;
			}

			// read records pointers
			this->file.clear();
			this->file.seekg(this->indexFileHeaderSize + ((uint64_t)i * pageSize) + 12 + (page.getPageSizeInRecords() * 4), std::ios_base::beg);
			for (int i = 0; i < page.recordsCount; i++) {
				this->file.read((char*)&num, 4);
				page.recordPosition[i] = num;
			}

			// read children pointers
			this->file.clear();
			this->file.seekg(this->indexFileHeaderSize + ((uint64_t)i * pageSize) + 12 + (page.getPageSizeInRecords() * 4) + (page.getPageSizeInRecords() * 4), std::ios_base::beg);

			for (int i = 0; i < page.recordsCount + 1; i++) {
				this->file.read((char*)&num, 4);
				page.childNumbers[i] = num;
			}

			addPage(page, false);

			delete[] keysArr;
			delete[] pointersArr;

			return true;
		}

		++i;
	}
	
}


