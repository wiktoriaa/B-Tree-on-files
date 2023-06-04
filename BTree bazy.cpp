// BTree bazy.cpp : Ten plik zawiera funkcję „main”. W nim rozpoczyna się i kończy wykonywanie programu.
//

#include <iostream>
#include <fstream>
#include <algorithm>
#include "Record.h"
#include "BTree.h"

using namespace std;

void generateFile(int recordsCount, string name) {
    fstream dataFile(name + ".dbd", std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);
    fstream indexFile(name + ".dbx", std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);
}

int main()
{
    int d = 2;
    int recordSize = 5;
    BTree *btree = new BTree(d);
    string input = "";
    int key;
    int val;
    Record record;

    cout << "Drzewo:\n";
    btree->printTree();

    /*cout << "MENU\n\n";
    cout << "INSERT key \n";
    cout << "SEARCH key \n";
    cout << "DIREC key \n";
    cout << "INREC key \n";
    cout << "UPREC key \n";
    cout << "RM key \n";
    cout << "DISPLAY pageNum \n\n";*/

    while (input != "EXIT") {
        cout << ">> ";

        cin >> input;

        std::transform(input.begin(), input.end(), input.begin(), ::toupper);

        if (input == "INSERT") {
            cin >> key;
            record.key = key;

            for (int i = 0; i < recordSize; i++) {
                record.data[i] = 0;
            }

            btree->insertRecord(record);

            //std::cout << "Wstawiono rekord:\n";
            //btree->displayRecord(record.key);
        }
        else if (input == "DISPLAY") {
            cin >> val;
            btree->displayPage(val);
        }
        else if (input == "SEARCH") {
            
            cin >> val;

            btree->resetStats();
            btree->searchKey(val);
            btree->displayStats();
        }
        else if (input == "DIREC") {
            cin >> val;
            btree->displayRecord(val);
        }
        else if (input == "INREC") {
            cin >> key;
            record.key = key;

            for (int i = 0; i < recordSize; i++) {
                cin >> val;
                record.data[i] = val;
            }

            btree->insertRecord(record);

            std::cout << "Wstawiono rekord:\n";
            btree->displayRecord(record.key);
        }

        else if (input == "UPREC") {
            cin >> val;
            btree->updateRecord(val);
        }
        else if (input == "RM") {
            cin >> val;
            btree->remove(val);
        }
        else {
            break;
        }

        cout << "Drzewo:\n";
        btree->printTree();
        //btree->displayStats();
    }

   // btree->printTree();

    btree->displayStats();

    delete btree;

}

