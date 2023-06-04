#include "BTree.h"
#include <iostream>
#include <algorithm>
#include <map>

BTree::BTree(uint32_t degree)
{
    std::string filename;
    this->d = degree;
    this->indexPageRecordsCount = degree * 2;

    std::string option;

    while (true) {
        std::cout << "Wybierz opcje:\n";
        std::cout << "OPEN otworz baze danych\n";
        std::cout << "CREATE Utworz nowa baze danych\n";

        std::cin >> option;

        std::transform(option.begin(), option.end(), option.begin(), ::toupper);

        if (option == "OPEN" || option == "CREATE") {
            break;
        }

        std::cout << "Wybierz poprawna opcje\n\n";
    }
    
    std::cout << "Wpisz nazwe bazy:\n";
    std::cin >> filename;
    this->filename = filename;

    if (option == "OPEN") {
        openDatabase(filename);
    }
    else {
        createDatabase(filename);
    }
}

BTree::~BTree()
{
    this->dataFile.close();
    delete cache;
    updateIndexFileHeader();
}

bool BTree::insert(Record record, uint32_t recordPointer)
{
    if (record.key == 0) {
        std::cout << "[BTREE][insert] dopuszczalne s¹ tylko klucze wieksze od 0\n";
        return false;
    }

    /*
        check if key exist
        get last accessed page number in class field lastAccessedPageNumber
    */ 
   
    if (this->search(record.key, this->rootPageNum) > 0) {
        std::cout << "Rekord o takim kluczu juz istnieje\n";
        return false;
    }

    /* get current page */
    if (this->lastAccessedPageNumber == 0) {
        std::cout << "[BTREE][insert]: ERROR brak stron w pliku\n";
    }

    Page currentPage = this->cache->getPage(this->lastAccessedPageNumber);

    if (!currentPage.isValid()) {
        std::cout << "[BTREE][insert]: ERROR currentPage jest null, numer strony: " << this->lastAccessedPageNumber << "\n";
    }

    if (currentPage.recordsCount < 2 * d) {
        //std::cout << "d = " << d << "\n";
        currentPage.insertKey(record.key);
        int pos = currentPage.getKeyPos(record.key);
        currentPage.recordPosition[pos] = recordPointer;
        this->cache->updatePage(currentPage);
        return true;
    }
    else { 
        /* overflow currentPage */
        currentPage.insertKey(record.key);
        currentPage.recordPosition[currentPage.getKeyPos(record.key)] = recordPointer;

        // próba kompensacji
        if (currentPage.parentNumber > 0) {
            Page parent = this->cache->getPage(currentPage.parentNumber);

            if (!parent.isValid()) {
                std::cout << "[BTREE][insert]: ERROR rodzic o numerze " << currentPage.number << " nie istnieje\n";
                return false;
            }

            int childPosition = parent.getChildPos(currentPage.number);

            if (childPosition < 0) {
                std::cout << "[BTREE][insert]: ERROR w rodzicu nr " << parent.number << " nie ma dziecka nr " << currentPage.number << "\n";
                return false;
            }

            /*
                check if non-full sibling exist
            */

            Page leftSibling = this->cache->getPage(parent.childNumbers[childPosition - 1]);
            Page rightSibling = this->cache->getPage(parent.childNumbers[childPosition + 1]);

            if (leftSibling.isValid() && leftSibling.recordsCount < 2*d) {
                compensation(currentPage, leftSibling, childPosition);
            }
            else if (rightSibling.isValid() && rightSibling.recordsCount < 2 * d) {
                compensation(currentPage, rightSibling, childPosition);
            }
            else {
                split(currentPage);
            }
        }
        else {
            split(currentPage);
        }
    }
    return true;
}

/*
    helper function for adding page to cache
*/

void BTree::addPage(Page page)
{
    this->cache->addPage(page);
    this->pagesCount++;
}

/*
    insert record into data file and return record position in file (line number)
*/

int BTree::saveRecord(Record record)
{
    this->dataFile.clear();
    this->dataFile.seekg(0, std::ios_base::end);
    int length = this->dataFile.tellg();

    this->dataFile.clear();
    this->dataFile.seekg(((uint64_t)length - 4), std::ios_base::beg);
    uint32_t recordsCount;
    
    this->dataFile.read((char*)&recordsCount, 4);
    //std::cout << "Liczba linii w pliku danych: " << recordsCount << "\n";

    this->dataFile.clear();
    this->dataFile.seekp((length - 4), std::ios_base::beg);

    this->dataFile.write((char*)(&record.key), 4);
    this->dataFile.write((char*)(&record.data), 5);

    // increment records count and save on the end
    recordsCount++;
    this->dataFile.write((char*)&recordsCount, 4);

    this->dataFile.flush();

    return recordsCount - 1;
}

/*
    return:
    - page number if record found
    - 0 if not fount

    set lastAccessedPageNumber variable:
    - last accessed page number if exist
    - 0 if no pages found
*/
uint32_t BTree::search(uint32_t key, uint32_t pageNum)
{
    Page page = this->cache->getPage(pageNum);

    if (!page.isValid()) {
        std::cout << "[search]: strona nr " << pageNum << " nie istnieje\n";
        return 0;
    }

    this->lastAccessedPageNumber = page.number;

    int i = 0;
    while (i < page.recordsCount && key > page.keys[i]) {
        ++i;
    }

    if (page.keys[i] == key) {
        return page.number;
    }

    if (page.isLeaf()) {
        return 0;
    }

    return search(key, page.childNumbers[i]);
}

bool BTree::createDatabase(std::string filename)
{
    this->dataFile.open(filename + ".dbd", std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);
    this->indexFile.open(filename + ".dbx", std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);
    this->filename = filename;
    this->indexPageSize = 12 + (indexPageRecordsCount * 4) + (indexPageRecordsCount * 4) + ((indexPageRecordsCount + 1)) * 4; // in bytes, page metadata = 12B, page keys arr, page child keys arr

    this->createDataFile();
    this->generateIndexFileHeader();

    this->indexFile.close();

    this->cache = new Cache(1, filename + ".dbx", this->indexPageSize, this->indexPageRecordsCount, this->pagesCount);

    return true;
}

bool BTree::openDatabase(std::string filename)
{
    this->filename = filename;
    this->dataFile.open(filename + ".dbd", std::ios::in | std::ios::out | std::ios::binary);

    /* read metadata */
    this->indexFile.open(filename + ".dbx", std::ios::in | std::ios::out | std::ios::binary);
    this->indexFile.seekg(8, std::ios_base::beg);
    this->indexFile.read((char*)&this->indexPageSize, 4);
    this->indexFile.read((char*)&this->indexPageRecordsCount, 4);
    this->indexFile.read((char*)&this->rootPageNum, 4);
    this->indexFile.read((char*)&this->pagesCount, 4);
    this->indexFile.read((char*)&this->treeHeight, 4);
    this->indexFile.close();

    std::cout << "Numer roota: " << this->rootPageNum << "\n";
    std::cout << "Wysokosc drzewa: " << this->treeHeight << "\n";

    this->cache = new Cache(this->treeHeight + 1, filename + ".dbx", this->indexPageSize, this->indexPageRecordsCount, this->pagesCount);
    return true;
}

bool BTree::savePage(uint32_t pageNum)
{
    cache->savePage(pageNum);
    return false;
}

void BTree::searchKey(uint32_t key)
{
    uint32_t pageNum = search(key, rootPageNum);
    if (pageNum > 0) {
        std::cout << "Znaleziono klucz na stronie " << pageNum << "\n";
    }
    else {
        std::cout << "Nie znaleziono klucza nr " << key << "\n";
    }
}

void BTree::printTree()
{
    print(this->rootPageNum);
    std::cout << "\n";
}

void BTree::insertRecord(Record record)
{
    // insert record into DBD file
    int recordPos = saveRecord(record);

    insert(record, recordPos);

    // save pointer to data file
    Page recordPage = this->cache->getPage(search(record.key, this->rootPageNum));

    if (!recordPage.isValid()) {
        std::cout << "[BTREE][insert]COS POSZLO NIE TAK NIE MA REKORDU!\n";
        return;
    }

    int keyPos = recordPage.getKeyPos(record.key);
    recordPage.recordPosition[keyPos] = recordPos;

    this->cache->updatePage(recordPage);
}

void BTree::compensation(Page& fullPage, Page& sibling, int parentChildPointer)
{
    std::map<uint32_t, uint32_t> keys;
    std::map<uint32_t, uint32_t> recordsPointers;
    uint32_t nextPointer;
    bool rightChild = fullPage.keys[0] < sibling.keys[0];

    uint32_t siblingLastPointer = sibling.childNumbers[sibling.recordsCount];

    // save record pointers
    for (int i = 0; i < fullPage.recordsCount; i++) {
        recordsPointers.insert({fullPage.keys[i], fullPage.recordPosition[i]});
    }
    for (int i = 0; i < sibling.recordsCount; i++) {
        recordsPointers.insert({ sibling.keys[i], sibling.recordPosition[i] });
    }

    if (rightChild) {
        parentChildPointer;
        nextPointer = fullPage.childNumbers[0];

        for (int i = 0; i < fullPage.recordsCount; i++) {
            keys.insert({ fullPage.keys[i], nextPointer});
            nextPointer = fullPage.childNumbers[i + 1];
        }

        nextPointer = sibling.childNumbers[0];

        for (int i = 0; i < sibling.recordsCount; i++) {
            keys.insert({ sibling.keys[i], nextPointer });
            nextPointer = sibling.childNumbers[i + 1];
        }
    }
    else {
        --parentChildPointer;
        nextPointer = sibling.childNumbers[0];

        for (int i = 0; i < sibling.recordsCount; i++) {
            keys.insert({ sibling.keys[i], nextPointer });
            nextPointer = sibling.childNumbers[i + 1];
        }

        nextPointer = fullPage.childNumbers[0];

        for (int i = 0; i < fullPage.recordsCount; i++) {
            keys.insert({ fullPage.keys[i], nextPointer });
            nextPointer = fullPage.childNumbers[i + 1];
        }
    }

    /*
        Now nextPointer => last pointer
    */

    //keys.erase(0);
    //int middleIndex = (keys.size() / 2);

    Page parent = this->cache->getPage(fullPage.parentNumber);

    int middleIndex = 0;
    uint32_t parentKey = parent.keys[parentChildPointer];

    for (auto const& x : keys) {
        if (x.first > parentKey || middleIndex == (keys.size() / 2)) {
            break;
        }
        ++middleIndex;
    }

    if (!parent.isValid()) {
        std::cout << "[BTREE][compensation]: ERROR strona rodzica o numerze " << fullPage.parentNumber << " nie istnieje\n";
        return;
    }

    int index = 0;
    int fullIdx = 0;
    int siblingIdx = 0;
    int middleKey = 0;
    int middlePointer = 0;

    for (auto const& x : keys) {
        if (index == middleIndex) {
            middleKey = x.first;
            middlePointer = x.second;
            break;
        }
        ++index;
    }

    /* swap middle key with parent */

    keys.erase(middleKey);
    keys.insert({ parent.keys[parentChildPointer], middlePointer });

    recordsPointers.insert({ parent.keys[parentChildPointer], parent.recordPosition[parentChildPointer] });
    parent.keys[parentChildPointer] = middleKey;

    parent.recordPosition[parentChildPointer] = recordsPointers.find(middleKey)->second;

    for (int i = 0; i < this->indexPageRecordsCount + 1; i++) {
        fullPage.keys[i] = 0;
        fullPage.childNumbers[i] = 0;
        sibling.keys[i] = 0;
        sibling.childNumbers[i] = 0;
    }

    fullPage.recordsCount = 0;
    sibling.recordsCount = 0;

    index = 0;
    bool change = true;

    for (auto const& x : keys) {
        if (x.first < middleKey) {
            if (rightChild) {
                fullPage.keys[fullIdx] = x.first;
                fullPage.childNumbers[fullIdx] = x.second;
                fullPage.recordPosition[fullIdx++] = recordsPointers.find(x.first)->second;
                fullPage.recordsCount++;
            }
            else {
                sibling.keys[siblingIdx] = x.first;
                sibling.childNumbers[siblingIdx] = x.second;
                sibling.recordPosition[siblingIdx++] = recordsPointers.find(x.first)->second;
                sibling.recordsCount++;
            }
        }
        else {
            if (change) {
                if (rightChild) {
                    fullPage.childNumbers[fullIdx] = siblingLastPointer;
                }
                else {
                    sibling.childNumbers[siblingIdx] = siblingLastPointer;
                }
                change = false;
            }

            if (!rightChild) {
                fullPage.keys[fullIdx] = x.first;
                fullPage.childNumbers[fullIdx] = x.second;
                fullPage.recordPosition[fullIdx++] = recordsPointers.find(x.first)->second;
                fullPage.recordsCount++;
            }
            else {
                sibling.keys[siblingIdx] = x.first;
                sibling.childNumbers[siblingIdx] = x.second;
                sibling.recordPosition[siblingIdx++] = recordsPointers.find(x.first)->second;
                sibling.recordsCount++;
            }
        }
        ++index;
    }

    this->cache->updatePage(fullPage);
    this->cache->updatePage(sibling);
    this->cache->updatePage(parent);
}

void BTree::split(Page& fullPage)
{
    Page emptyPage(this->pagesCount + 1, this->indexPageRecordsCount, fullPage.parentNumber);
    int middlePos = fullPage.recordsCount / 2;
    int middleKey = fullPage.keys[middlePos];
    uint32_t middleRecordPointer = fullPage.recordPosition[middlePos];

    int idx = 0;
    int i = middlePos + 1;
    int recordsRemoved = 0;

    for (i; i < fullPage.recordsCount; i++) {
        emptyPage.keys[idx] = fullPage.keys[i];
        emptyPage.childNumbers[idx] = fullPage.childNumbers[i];
        emptyPage.recordPosition[idx] = fullPage.recordPosition[i];
        emptyPage.recordsCount++;

        fullPage.keys[i] = 0;
        fullPage.childNumbers[i] = 0;
        fullPage.recordPosition[i] = 0;
        recordsRemoved++;
        ++idx;
    }
    fullPage.recordsCount -= recordsRemoved;

    /* copy last pointer */
    emptyPage.childNumbers[idx] = fullPage.childNumbers[i];
    fullPage.childNumbers[i] = 0;

    /* change parent field in childrens with new parent */
    for (int i = 0; i < emptyPage.recordsCount + 1; i++) {
        if (emptyPage.childNumbers[i] != 0) {
            Page page = this->cache->getPage(emptyPage.childNumbers[i]);
            page.parentNumber = emptyPage.number;
            this->cache->updatePage(page);
        }
    }

    /* remove middle key from full page */
    fullPage.keys[middlePos] = 0;
    fullPage.recordPosition[middlePos] = 0;
    fullPage.recordsCount--;

    this->cache->updatePage(fullPage);
    addPage(emptyPage);

    /* ******
        insert middle key into parent
     ****** */

    Page parent = this->cache->getPage(fullPage.parentNumber);

    /* if root (parent doesn't exist) */
    if (!parent.isValid()) {
        /* fullPage was root */
        Page newRoot(this->pagesCount + 1, this->indexPageRecordsCount, 0);
        this->rootPageNum = newRoot.number;

        emptyPage.parentNumber = newRoot.number;
        fullPage.parentNumber = newRoot.number;

        /* grow tree */
        this->treeHeight++;
        this->cache->changeCacheSize(this->treeHeight);

        newRoot.insertKey(middleKey);
        newRoot.recordPosition[newRoot.getKeyPos(middleKey)] = middleRecordPointer;
        newRoot.childNumbers[0] = fullPage.number;
        newRoot.childNumbers[1] = emptyPage.number;

        this->cache->updatePage(emptyPage);
        this->cache->updatePage(fullPage);
        addPage(newRoot);
    }
    else {
        int pos = parent.insertKey(middleKey) + 1;
        parent.recordPosition[parent.getKeyPos(middleKey)] = middleRecordPointer;
        parent.insertChild(emptyPage.number, pos);
        //parent.childNumbers[parent.recordsCount] = emptyPage.number;

        if (parent.recordsCount > 2 * this->d) {
            this->cache->updatePage(parent);
            split(parent);
            this->cache->updatePage(parent);
        }
        else {
            this->cache->updatePage(parent);
        }
    }
}

void BTree::displayRecord(uint32_t key)
{
    Page page = this->cache->getPage(search(key, this->rootPageNum));

    if (!page.isValid()) {
        std::cout << "[BTREE][displayRecord] Nie znaleziono rekordu o kluczu " << key << "\n";
        return;
    }

    int pos = page.getKeyPos(key);
    Record record = getRecord(page.recordPosition[pos]);

    std::cout << "Klucz: " << record.key << ", wartosci: ";

    for (int i = 0; i < 5; i++) {
        std::cout << unsigned((uint8_t)record.data[i]) << " ";
    }
    std::cout << "\n";
}

bool BTree::recordExist(uint32_t key)
{
    return search(key, this->rootPageNum) > 0;
}

void BTree::displayStats()
{
    std::cout << ">>>>>>>>>>>>>> STATYSTYKI ODCZYT/ZAPIS\n";
    std::cout << ">>>>>>>>>>>>>> Laczna liczba odczytow stron z dysku: " << this->cache->readsCount << "\n";
    std::cout << ">>>>>>>>>>>>>> Laczna liczba zapisow stron na dysk: " << this->cache->writesCount << "\n";
}

void BTree::updateRecord(uint32_t key)
{
    Page page = this->cache->getPage(search(key, this->rootPageNum));

    if (!page.isValid()) {
        std::cout << "[BTREE][updateRecord] Nie znaleziono rekordu o kluczu " << key << "\n";
        return;
    }

    int pos = page.getKeyPos(key);
    Record record = getRecord(page.recordPosition[pos]);

    std::cout << "Wpisz 5 liczb [0-255] oddzielonych spracjami: ";
    std::string byte;

    for (int i = 0; i < 5; i++) {
        std::cin >> byte;
        record.data[i] = (uint8_t)atoi(byte.c_str());;
    }

    if (!update(record, page.recordPosition[pos])) {
        std::cout << "[BTREE][updateRecord] Nie udalo sie zaktualizowac rekordu\n";
        return;
    }

    std::cout << "Pomyslnie zaktualizowano rekord\n";
}

void BTree::print(int pageNum)
{
    Page page = this->cache->getPage(pageNum);
    if (!page.isValid()) {
        std::cout << "[print]: strona o numerze " << pageNum << " nie istnieje\n";
        return;
    }

    int i;
    for (i = 0; i < page.recordsCount; i++) {
        if (!page.isLeaf()) {
            print(page.childNumbers[i]);
        }
        std::cout << " " << page.keys[i];
    }

    // print last child
    if (!page.isLeaf()) {
        print(page.childNumbers[i]);
    }
}

bool BTree::remove(uint32_t key)
{
    uint32_t pageNum = this->search(key, this->rootPageNum);

    if (pageNum == 0) {
        std::cout << "Rekord o takim kluczu nie istnieje\n";
        return false;
    }

    Page page = this->cache->getPage(pageNum);
    int pos = page.getKeyPos(key);

    if (!page.isLeaf()) {
        page.removeKey(key);

        Page child = this->cache->getPage(page.childNumbers[pos]);
        while (!child.isLeaf()) {
            int childPos = child.recordsCount;
            if (child.childNumbers[child.recordsCount + 1] != 0) {
                childPos++;
            }
            else {

            }

            child = this->cache->getPage(child.childNumbers[childPos]);
        }

        if (!child.isValid()) {
            std::cout << "[BTREE][remove] Strona numer " << page.childNumbers[pos] << " nie istnieje\n";
            return false;
        }

        int lastChildKeyIdx = child.recordsCount - 1;
        uint32_t lastChildKey = child.keys[lastChildKeyIdx];

        page.insertKey(lastChildKey);
        page.recordPosition[page.getKeyPos(lastChildKey)] = child.recordPosition[lastChildKeyIdx];

        child.removeKey(lastChildKey);

        this->cache->updatePage(page);
        this->cache->updatePage(child);

        page = child;
    }
    else {
        page.removeKey(key);
        this->cache->updatePage(page);
    }

    if (page.recordsCount >= this->d) {
        return true;
    }
    else if (page.parentNumber != 0) {
        COMP:
        Page parent = this->cache->getPage(page.parentNumber);

        if (!parent.isValid()) {
            //std::cout << "[BTREE][remove]: ERROR rodzic o numerze " << page.number << " nie istnieje\n";
            return false;
        }

        int childPosition = parent.getChildPos(page.number);

        if (childPosition < 0) {
            std::cout << "[BTREE][remove]: ERROR w rodzicu nr " << parent.number << " nie ma dziecka nr " << page.number << "\n";
            return false;
        }

        /*
            check if min d records sibling exist
        */

        Page leftSibling = this->cache->getPage(parent.childNumbers[childPosition - 1]);
        Page rightSibling = this->cache->getPage(parent.childNumbers[childPosition + 1]);

        if (leftSibling.isValid() && leftSibling.recordsCount > d) {
            //std::cout << "KOMPENSACJA\n";
            compensation(page, leftSibling, childPosition);
            return true;
        }
        else if (rightSibling.isValid() && rightSibling.recordsCount > d) {
            //std::cout << "KOMPENSACJA\n";
            compensation(page, rightSibling, childPosition);
            return true;
        }
        else {
            //std::cout << "MERGE\n";
            if (rightSibling.isValid()) {
                merge(page, rightSibling);
            }
            else {
                merge(leftSibling, page);
                page = leftSibling;
            }
        }
    }
    else {
        //std::cout << "Only root survive\n";
    }

    page = this->cache->getPage(page.parentNumber);

    if (page.number != this->rootPageNum && page.recordsCount < this->d) {
        goto COMP;
    }
    else if (page.number == this->rootPageNum && page.recordsCount == 0) {

        /* change root, -1 level */

        this->rootPageNum = page.childNumbers[0];
        page.clearPage();
        this->cache->removePage(page.number);

        this->treeHeight--;
        this->cache->changeCacheSize(treeHeight);

        Page root = this->cache->getPage(this->rootPageNum);
        root.parentNumber = 0;
        this->cache->updatePage(root);

        /* Update new leafs parents */

        for (int i = 0; i < root.recordsCount + 1; i++) {
            page = this->cache->getPage(root.childNumbers[i]);

            if (page.isValid()) {
                page.parentNumber = this->rootPageNum;
                this->cache->updatePage(page);
            }
        }

        std::cout << "nowy root: " << this->rootPageNum << "\n";
    }

    return true;
}

void BTree::resetStats()
{
    this->cache->readsCount = 0;
    this->cache->writesCount = 0;
}

/*
    page < sibling
*/

void BTree::merge(Page page, Page sibling)
{
    Page parent = this->cache->getPage(page.parentNumber);

    int parentKeyIdx = parent.getChildPos(page.number);

    page.keys[page.recordsCount] = parent.keys[parentKeyIdx];
    page.recordPosition[page.recordsCount++] = parent.recordPosition[parentKeyIdx];

    parent.removeChild(sibling.number);
    parent.removeKey(parent.keys[parentKeyIdx]);

    int idx = 0;
    int childAlign = 0;

    if (page.childNumbers[page.recordsCount] != 0) {
        childAlign = 1;
    }
    

    for (int i = page.recordsCount; i < this->indexPageRecordsCount; i++) {
        page.keys[i] = sibling.keys[idx];
        page.recordPosition[i] = sibling.recordPosition[idx];
        page.childNumbers[i + childAlign] = sibling.childNumbers[idx++];
        ++page.recordsCount;
    }

    page.childNumbers[page.recordsCount] = sibling.childNumbers[idx];

    sibling.clearPage();
    this->cache->removePage(sibling.number);

    this->cache->updatePage(parent);
    this->cache->updatePage(page);
}

bool BTree::update(Record record, int pos)
{
    this->dataFile.clear();
    this->dataFile.seekp(pos * 9, std::ios_base::beg);

    this->dataFile.write((char*)&record.key, 4);
    this->dataFile.write((char*)&record.data, 5);
    this->dataFile.flush();

    if (this->dataFile.fail()) {
        return false;
    }
    return true;
}


/*
* File format: uint32_t key uin8_t[5] value
*/

void BTree::createDataFile()
{
    srand(time(NULL));
    uint32_t recordsCount = 0;

    this->dataFile.write((char*) (&recordsCount), 4);
    this->dataFile.flush();
}

Record BTree::getRecord(int pos)
{
    Record record;

    this->dataFile.clear();
    this->dataFile.seekg((9 * pos), std::ios_base::beg);
    this->dataFile.read((char*)&record.key, 4);
    this->dataFile.read((char*)&record.data, 5);

    if (this->dataFile.fail()) {
        std::cout << "[BTREE][getRecord]Blad przy odczycie rekordu\n";
        record.key = 0;
    }

    return record;
}

void BTree::generateIndexFileHeader()
{
    uint32_t zero = 0;
    uint32_t one = 1;

    // header 28B

    this->indexFile.write("BTREEDBX", 8);
    this->indexFile.write((char*)&indexPageSize, 4);                 // page size
    this->indexFile.write((char*)&indexPageRecordsCount, 4);         // max records per page count
    this->indexFile.write((char*)&zero, 4);                          // index of root page, 0 - no root, empty tree
    this->indexFile.write((char*)&one, 4);                           // last page number  
    this->indexFile.write((char*)&zero, 4);                          // tree height 

    // first page, 12B page metadata + data

    this->indexFile.write((char*)&one, 4);                           // page number
    this->indexFile.write((char*)&zero, 4);                          // parent key
    this->indexFile.write((char*)&zero, 4);                          // filled records count, 0 - empty tree

    // save keys
    uint32_t *zeroData = new uint32_t[(indexPageRecordsCount + 1) * 4]();

    this->indexFile.write((char*)zeroData, indexPageRecordsCount * 4); // keys
    this->indexFile.write((char*)zeroData, indexPageRecordsCount * 4); // keys
    this->indexFile.write((char*)zeroData, (indexPageRecordsCount + 1) * 4); // children

    delete[] zeroData;
}

void BTree::updateIndexFileHeader()
{
    this->indexFile.open(filename + ".dbx", std::ios::in | std::ios::out | std::ios::binary);
    this->indexFile.seekp(16, std::ios::beg);

    this->indexFile.write((char*)&this->rootPageNum, 4);                          // index of root page, 0 - no root, empty tree
    this->indexFile.write((char*)&this->pagesCount, 4);                           // last page number 
    this->indexFile.write((char*)&this->treeHeight, 4);                           // tree height 
    this->indexFile.close();
}

void BTree::displayPage(uint32_t number)
{
    Page page = this->cache->getPage(number);
    std::cout << "***************************\n";
    if (page.isValid()) {
        std::cout << "NUMER STRONY: ";
        std::cout << page.getPageNumber() << "\n";
        std::cout << "***************************\n";

        std::cout << "Klucze: ";
        for (int i = 0; i < indexPageRecordsCount + 1; i++) {
            std::cout << page.keys[i] << " ";
        }
        std::cout << "\n";

        /*std::cout << "Numery wierszy w pliku danych: ";
        for (int i = 0; i < indexPageRecordsCount + 1; i++) {
            std::cout << page.recordPosition[i] << " ";
        }
        std::cout << "\n";*/

        std::cout << "Potomkowie: ";
        for (int i = 0; i < indexPageRecordsCount + 1; i++) {
            std::cout << page.childNumbers[i] << " ";
        }
        std::cout << "\n";

        std::cout << "Numer rodzica: " << page.parentNumber << "\n";
        std::cout << "Liczba kluczy: " << page.recordsCount << "\n";
    }
    else {
        std::cout << "Nie udalo sie pobrac strony nr " << number << "\n";
    }
    std::cout << "***************************\n";
}

void BTree::printCache()
{
    this->cache->displayPages();
}


