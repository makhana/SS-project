#include <vector>
#include <string>

using namespace std;

struct LinkerRelocationTableEntrance{
  unsigned long offset; // location that should be changed (current locationCounter)
  int type; // 1 - lokalni ; 2 - globalni/eksterni ;
  string value; // name of symbol or section
  int addend; // this should be in the machine code but this way is easier
  string currSection; // section pool is in

  LinkerRelocationTableEntrance(unsigned long offset, int type, string value, int addend, string currSection){
    this->offset = offset;
    this->type = type;
    this->value = value;
    this->addend = addend;
    this->currSection = currSection;
  }

  void printRelocationTableEntrance(){
    cout << offset << " " << type << " " << value << " " << addend << " " << currSection << endl;
  }
};

struct LinkerSectionTableEntry{

  int sectionOldID;
  int fileID;
  string sectionName;
  unsigned long newBaseAddress;
  int size;
  int literalPoolSize;

  vector<char>* hexCode;

  LinkerSectionTableEntry(int oldID, int fileID, string name, unsigned long newBA, int size, int literalPoolSize){
    this->sectionOldID = oldID;
    this->fileID = fileID;
    this->sectionName = name;
    this->newBaseAddress = newBA;
    this->size = size;
    this->literalPoolSize = literalPoolSize;
    this->hexCode = new vector<char> ();
  }
};

struct FinalSectionsEntry{
  static int index;
  vector<char>* machineCode;
  int size;
  unsigned long baseAddress;
  string label;
  int idSection;
  bool isAbsolute; // if base address defined by place
  vector<LinkerRelocationTableEntrance*> *relocationTable;

  FinalSectionsEntry(string label, unsigned long baseAdd, int size, bool absolute){
    this->idSection = index++;
    this->label = label;
    this->baseAddress = baseAdd;
    this->size = size;
    this->machineCode = new vector<char> ();
    this->isAbsolute = absolute;
    this->relocationTable = new vector<LinkerRelocationTableEntrance*>();
  }
};

struct LinkerSymbolTableEntry{
  string label;
  unsigned long offset;
  int section;
  string value;
  bool equSymbol;
  //int type; // 1 - local; 2 - global

  LinkerSymbolTableEntry(string label, unsigned long offset, int section, bool equ, string val){
    this->label = label;
    this->offset = offset;
    this->section = section;
    this->equSymbol = equ;
    this->value = val;
    //this->type = type;
  }
};


