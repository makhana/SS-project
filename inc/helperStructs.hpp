#include <string>
#include <iostream>
#include <vector>
#include <map>
using namespace std;


struct TableOfLiteralsEntry {
  string name; //value
  int index;
  bool isSymbol;
  bool equSymbol;
  string value;

  TableOfLiteralsEntry(int index, string name, bool symbol, bool equ, string val){
    this->index = index;
    this->name = name;
    this->isSymbol = symbol;
    this->equSymbol = equ;
    this->value = val;
  }

  void printTableOfLiteralsEntry(){
    cout << index << " " << name << endl;
  }
};

struct RelocationTableEntrance{
  //int section;
  int offset; // location that should be changed (current locationCounter)
  int type; // 1 - lokalni ; 2 - globalni/eksterni ;
  string value; // name of symbol or section
  int addend; // this should be in the machine code but this way is easier
  string currSection; // section pool is in

  RelocationTableEntrance(int offset, int type, string value, int addend, string currSection){
    //this->section = section;
    this->offset = offset;
    this->type = type;
    this->value = value;
    this->addend = addend;
    this->currSection = currSection;
  }

  void printRelocationTableEntrance(){
    cout << offset << " " << type << " " << value << " " << addend << endl;
  }
};

struct SymbolTableEntry {
  // memeber variables
  static int entranceIndexID;
  int entranceIndex;
  string label; // name of the symbol or section
  int section; // -1 - symbol is a section; 0 - not defined; 1+ - user sections
  int offset; // locationCounter (value)
  int type; // 0 - local; 1 - global; 2 - extern; 3 - absolute
  bool isDefined;
  string sectionName; // section of the symbol
  bool isAbsolute;
  string valueOfAbsolute;
 

  //constructor
  SymbolTableEntry(string name, int locationCounter, int type, int section, bool isDefined, string sec, bool abs, string val){
    this->label = name;
    this->entranceIndex = entranceIndexID++;
    this->section = section;
    this->isAbsolute = abs;
    this->offset = locationCounter;
    this->type = type;
    this->isDefined = isDefined;
    this->sectionName = sec;
    this->valueOfAbsolute = val;
  }

  void printSymbolTableEntrance(){
    
    cout << entranceIndex << " " << label << " " << section << " " << offset << " " << type << " " << isDefined << " " << endl;
  }

};

struct Section {
  int idSection; // entrance in the Symbol Table
  string name;
  int base; // 0 in the assembler part, linker will fix it
  int size; // length of the whole section in bytes
  int literalPoolSize;
  vector<char>* hexCode;
  map<string, TableOfLiteralsEntry*> *literalTable;
  vector<RelocationTableEntrance*> *relocationTable;
  vector<string>* literalsInsertionOrder;
  vector<string>* moveLocationCounter;
  int literalTableIndexCNT;

  Section(int id, string name){
    this->idSection = id;
    this->name = name;
    this->base = 0;
    this->size = 0;
    this->literalPoolSize = 0;
    this->hexCode = new vector<char> ();
    this->relocationTable = new vector<RelocationTableEntrance*>();
    this->literalTable = new map<string, TableOfLiteralsEntry*>();
    this->literalsInsertionOrder = new vector<string>();
    this->moveLocationCounter = new vector<string>();
    this->literalTableIndexCNT = 1;
  }

  void printSection(){
    int cnt = 0;
    cout<< "#."<< name << endl;
    cout << idSection << " " << base << " " << size <<  " " << literalPoolSize << endl;
    for(char c : *hexCode){
      if(cnt % 2 == 1){
        if(cnt == 15){
          cnt = 0;
          cout << c << endl;
        } else {
          cout << c << " ";
          cnt++;
        }
      } else {
        cout << c;
        cnt++;
      }
    }
    cout << endl;

    if(!relocationTable->empty()){
      cout << "#.ret."<< name << endl;
      cout << "offset " << "type " << "value " << "addend" << endl;
      for(auto key: *relocationTable){
        cout << key->offset << " " << key->type << " " << key->value << " " << key->addend << endl;
      }
    }
  }
};


struct MultipleArguments{
  vector<string*> *argumentName;
  vector<int> *argumentType; // 0 - symbol ; 1 - literal ; 2 - register
  int addressing; 
  // ADDRESSING : 10 - immediate literal
  // 11 - immediate symbol
  // 12 - mem[literal] mem direct literal
  // 13 - mem[symbol] mem direct symbol
  // 14 - register direct
  // 15 - mem[reg] register indirect
  // 16 - mem[reg + literal] register direct with literal offset
  // 17 - mem[reg + symbol] register direct with offset

  MultipleArguments(string *argName, int argType, int addr){
    this->argumentName = new vector<string*>();
    this->argumentName->push_back(argName);
    this->argumentType = new vector<int>();
    this->argumentType->push_back(argType);
    this->addressing = addr;
  }
};

