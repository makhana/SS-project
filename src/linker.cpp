#include <string>
#include <cstring>
#include <map>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>
#include <iomanip>
#include "../inc/linker.hpp"

using namespace std;

vector <LinkerSectionTableEntry*>* linkerSectionTable = new vector <LinkerSectionTableEntry*>(); // string je ime sekcije + index fajla
map <string, LinkerSymbolTableEntry*>* linkerSymbolTable = new map <string, LinkerSymbolTableEntry*>(); // tabela globalnih simbola
set <string> definedSymbols;
set <string> undefinedSymbols;

int FinalSectionsEntry::index = 1;
map <string, FinalSectionsEntry*>* finalSections = new map <string, FinalSectionsEntry*>();
map<unsigned long, string> finalSectionsOrder;
vector<string> relocationFinalSectionsOrder;

map<string, unsigned long>* placeSections =  new map<string, unsigned long>();

vector<LinkerSectionTableEntry*>* unfinishedSections = new vector<LinkerSectionTableEntry*>();

// helper functions
void printSymbolTable();
void printLinkerSectionTable();
void printFinalSectionTable();
void printFinalRelocationSectionTable();
string toLEHexPrint(int val);

void printHexToFile();
void printRelocatableToFile();

string outputFileName;

int main(int argc, char* argv[]){
  vector<ifstream>* files = new vector<ifstream>;
  vector<string> fileNames;
  //ifstream inputFile;

  bool relocatable = false;
  bool hex = false;
  

  int cnt = 1;
  
  bool fileNameDefined = false;

  while(cnt < argc){
    if(strcmp(argv[cnt], "-o") == 0){
      outputFileName = argv[cnt+1];
      cnt += 2;
      fileNameDefined = true;
    } else if(strcmp(argv[cnt], "-hex") == 0){
      hex = true;
      cnt += 1;
      if(!fileNameDefined){
        outputFileName = "default.hex";
      }
    } else if(strcmp(argv[cnt], "-relocatable") == 0){
      relocatable = true;
      cnt += 1;
      if(!fileNameDefined){
        outputFileName = "default.o";
      }
    } else {
      string placeArg = argv[cnt];
      
      if(placeArg.find("-place") != string::npos){
        istringstream tokenStream(placeArg.substr(7));
        string sectionName;
        string sectionPosition;
        getline(tokenStream, sectionName, '@');
        getline(tokenStream, sectionPosition, '@');
        sectionPosition = sectionPosition.substr(2);

        unsigned long pos = stoul(sectionPosition, nullptr, 16);
        placeSections->insert({sectionName, pos});
        cnt += 1;
      }else {
        fileNames.push_back(argv[cnt]);
        cnt += 1;
      }
    }
  }
  
  if(fileNames.empty()){
    cout << "ERROR: no input parammeters in linker" << endl;
    exit(-1);
  }
  if(!hex && !relocatable){
    cout << "ERROR: command line doesn't have -hex or -relocatable" << endl;
    exit(-1);
  } else if(hex && relocatable){
    cout << "ERROR: command line has both -hex and -relocatable but should have only one" << endl;
    exit(-1);
  }
  if(relocatable){
    placeSections->clear();
  }
  
  
  unsigned long finalSectionsSize = 0;

  for(int i = 0; i < fileNames.size() ; i++){
    
    ifstream inputFile(fileNames[i], std::ios::binary);
    
    if(inputFile.is_open()){

      int sectionsNum;
      inputFile.read(reinterpret_cast<char*>(&sectionsNum), sizeof(int)); // we take num of sections

      // taking sections and making sectionTable and finalSectionTable
      for( int j = 0; j < sectionsNum; j++){

        int sectionOldID;
        inputFile.read(reinterpret_cast<char*>(&sectionOldID), sizeof(int)); // we take sectionID

        size_t nameSize;
        string sectionName;

        inputFile.read(reinterpret_cast<char*>(&nameSize), sizeof(size_t)); // we read length of the section name
        sectionName.resize(nameSize);
        inputFile.read(&sectionName[0], nameSize); // we read section name

        int sectionSize;
        inputFile.read(reinterpret_cast<char*>(&sectionSize), sizeof(int)); // size of the section without the pool
        int literalPoolSize;
        inputFile.read(reinterpret_cast<char*>(&literalPoolSize), sizeof(int)); // size of the section pool

        size_t sectionHexCodeSize;
        inputFile.read(reinterpret_cast<char*>(&sectionHexCodeSize), sizeof(size_t));


        // we put sections from address 0 and none is absolute
        if(hex && placeSections->empty()){
          map <string, FinalSectionsEntry*>::iterator iterFinal = finalSections->find(sectionName);

          if(iterFinal == finalSections->end()){
            // section doesn't exist in the final
            bool isAbsolute = false;
            unsigned long baseAddress = finalSectionsSize;


            LinkerSectionTableEntry* newSec = new LinkerSectionTableEntry(sectionOldID, i, sectionName, baseAddress, sectionSize, literalPoolSize);
            
            newSec->hexCode->resize(sectionHexCodeSize);

            inputFile.read(newSec->hexCode->data(), sectionHexCodeSize);
            
            linkerSectionTable->push_back(newSec);

            FinalSectionsEntry* newEntry = new FinalSectionsEntry(sectionName, baseAddress, sectionSize + literalPoolSize, isAbsolute);
            

            for(char c: (*newSec->hexCode)){
              newEntry->machineCode->push_back(c);
            }
            

            finalSections->insert({sectionName, newEntry});
            finalSectionsOrder.insert({baseAddress, sectionName});


            // next final ssection will be after this one
            finalSectionsSize += sectionSize + literalPoolSize;
            

          } else {
            
            // section already exists in the final section table so we should concatenate it
            unsigned long baseAddress = iterFinal->second->size + iterFinal->second->baseAddress;
            iterFinal->second->size += sectionSize + literalPoolSize;


            LinkerSectionTableEntry* newSec = new LinkerSectionTableEntry(sectionOldID, i, sectionName, baseAddress, sectionSize, literalPoolSize);
            
            newSec->hexCode->resize(sectionHexCodeSize);

            inputFile.read(newSec->hexCode->data(), sectionHexCodeSize);
            
            linkerSectionTable->push_back(newSec);

            for(char c: (*newSec->hexCode)){
              iterFinal->second->machineCode->push_back(c);
            }


            finalSectionsSize += sectionSize + literalPoolSize;

            // move all sections in front the current that are not absolute
            bool startMoving = false;
            unsigned long sizeToMove = sectionSize + literalPoolSize;
            for(auto it = finalSectionsOrder.begin(); it!= finalSectionsOrder.end(); ++it){
              if((*(finalSections))[it->second]->idSection == iterFinal->second->idSection){
                startMoving = true;
                continue;
              }
              if(startMoving && !(*(finalSections))[it->second]->isAbsolute){
                (*(finalSections))[it->second]->baseAddress += sizeToMove;
                for(LinkerSectionTableEntry* elem: *linkerSectionTable){
                  if(elem->sectionName == (*(finalSections))[it->second]->label){
                    elem->newBaseAddress += sizeToMove;
                  }
                }
              }
            }
          }
          

        } else if (relocatable){
          
          // every final sections base address is 0
          map <string, FinalSectionsEntry*>::iterator iterFinal = finalSections->find(sectionName);

          if(iterFinal == finalSections->end()){
            
            // section doesn't exist in the final

            bool isAbsolute = false;
            unsigned long baseAddress = 0;


            LinkerSectionTableEntry* newSec = new LinkerSectionTableEntry(sectionOldID, i, sectionName, baseAddress, sectionSize, literalPoolSize);
            
            newSec->hexCode->resize(sectionHexCodeSize);

            inputFile.read(newSec->hexCode->data(), sectionHexCodeSize);
            
            linkerSectionTable->push_back(newSec);

            FinalSectionsEntry* newEntry = new FinalSectionsEntry(sectionName, baseAddress, sectionSize + literalPoolSize, isAbsolute);
            

            for(char c: (*newSec->hexCode)){
              newEntry->machineCode->push_back(c);
            }
            

            finalSections->insert({sectionName, newEntry});
            relocationFinalSectionsOrder.push_back(sectionName);
            
            //printFinalSectionTable();

          } else {
            
            // section already exists in the final section table so we should concatenate it

            unsigned long baseAddress = iterFinal->second->size + iterFinal->second->baseAddress;
            iterFinal->second->size += sectionSize + literalPoolSize;


            LinkerSectionTableEntry* newSec = new LinkerSectionTableEntry(sectionOldID, i, sectionName, baseAddress, sectionSize, literalPoolSize);
            
            newSec->hexCode->resize(sectionHexCodeSize);

            inputFile.read(newSec->hexCode->data(), sectionHexCodeSize);
            
            linkerSectionTable->push_back(newSec);

            for(char c: (*newSec->hexCode)){
              iterFinal->second->machineCode->push_back(c);
            }
          }
        }
        else {
          
          // -hex and -place in command line
          map <string, FinalSectionsEntry*>::iterator iterFinal = finalSections->find(sectionName);

          if(iterFinal == finalSections->end()){
            // section doesn't exist in the final
            bool isAbsolute = false;
            unsigned long baseAddress;
            
            map<string, unsigned long>::iterator itPlace = placeSections->find(sectionName);
            if(itPlace != placeSections->end()){
              // FOUND
              // i want base address to be number of bytes written in placeSections
              
              baseAddress = itPlace->second;
              
              isAbsolute = true;
            }

            if(isAbsolute){
              
              // section FOUND in placeSections
              LinkerSectionTableEntry* newSec = new LinkerSectionTableEntry(sectionOldID, i, sectionName, baseAddress, sectionSize, literalPoolSize);
              
              newSec->hexCode->resize(sectionHexCodeSize);

              inputFile.read(newSec->hexCode->data(), sectionHexCodeSize);
              
              linkerSectionTable->push_back(newSec);

              FinalSectionsEntry* newEntry = new FinalSectionsEntry(sectionName, baseAddress, sectionSize + literalPoolSize, isAbsolute);

              for(char c: (*newSec->hexCode)){
                newEntry->machineCode->push_back(c);
              }

              finalSections->insert({sectionName, newEntry});
              finalSectionsOrder.insert({baseAddress, sectionName});

              
              // so we can later start putting them from the highest address
              if(finalSectionsSize < baseAddress + sectionSize + literalPoolSize){
                finalSectionsSize = baseAddress + sectionSize + literalPoolSize;
              }
              

            } else {
              
              // put it in vector of later Sections
              LinkerSectionTableEntry* newSec = new LinkerSectionTableEntry(sectionOldID, i, sectionName, 0, sectionSize, literalPoolSize);
              
              newSec->hexCode->resize(sectionHexCodeSize);

              inputFile.read(newSec->hexCode->data(), sectionHexCodeSize);

              unfinishedSections->push_back(newSec);
            }

            
          } else {
            // section exists in the final and is absolute
            // section already exists in the final section table so we should concatenate it
            unsigned long baseAddress = iterFinal->second->size + iterFinal->second->baseAddress;
            
            iterFinal->second->size += sectionSize + literalPoolSize;

            LinkerSectionTableEntry* newSec = new LinkerSectionTableEntry(sectionOldID, i, sectionName, baseAddress, sectionSize, literalPoolSize);
            
            newSec->hexCode->resize(sectionHexCodeSize);

            inputFile.read(newSec->hexCode->data(), sectionHexCodeSize);
            
            linkerSectionTable->push_back(newSec);

            for(char c: (*newSec->hexCode)){
              iterFinal->second->machineCode->push_back(c);
            }

            // so we can later start putting them from the highest address
            if(finalSectionsSize < baseAddress + sectionSize + literalPoolSize){
              finalSectionsSize = baseAddress + sectionSize + literalPoolSize;
            }

          }
        }
        
      }
      files->push_back(move(inputFile));
    }
  }
  
  if(hex || !placeSections->empty()){
    // HERE GO THROUGH SECTIONS THAT WERE NOT ADDED ADD THEM AND MOVE THE REST (not absolute sections)

    for(LinkerSectionTableEntry* elem: *unfinishedSections){
      
      map <string, FinalSectionsEntry*>::iterator iterFinal = finalSections->find(elem->sectionName);

      if(iterFinal == finalSections->end()){
        // section doesn't exist in the final
        bool isAbsolute = false;
        unsigned long baseAddress = finalSectionsSize;
        
        elem->newBaseAddress = baseAddress;
        
        linkerSectionTable->push_back(elem);

        FinalSectionsEntry* newEntry = new FinalSectionsEntry(elem->sectionName, baseAddress, elem->size + elem->literalPoolSize, isAbsolute);

        for(char c: (*elem->hexCode)){
          newEntry->machineCode->push_back(c);
        }
        
        
        finalSections->insert({elem->sectionName, newEntry});
        finalSectionsOrder.insert({baseAddress, elem->sectionName});

        finalSectionsSize += elem->size + elem->literalPoolSize;

        

      } else {
        // section already exists in the final section table so we should concatenate it
        unsigned long baseAddress = iterFinal->second->size + iterFinal->second->baseAddress;
        iterFinal->second->size += elem->size + elem->literalPoolSize;

        elem->newBaseAddress = baseAddress;

        
        linkerSectionTable->push_back(elem);

        for(char c: (*elem->hexCode)){
          iterFinal->second->machineCode->push_back(c);
        }

        
        finalSectionsSize += elem->size + elem->literalPoolSize;

        // move all sections after the current (they are not absolute)
        bool startMoving = false;
        unsigned long sizeToMove = elem->size + elem->literalPoolSize;
        for(auto it = finalSectionsOrder.begin(); it!= finalSectionsOrder.end(); ++it){
          if((*(finalSections))[it->second]->idSection == iterFinal->second->idSection){
            startMoving = true;
            continue;
          }
          if(startMoving && !(*(finalSections))[it->second]->isAbsolute){
            (*(finalSections))[it->second]->baseAddress += sizeToMove;
            for(LinkerSectionTableEntry* el: *linkerSectionTable){
              if(el->sectionName == (*(finalSections))[it->second]->label){
                el->newBaseAddress += sizeToMove;
              }
            }
          }
        }
      }
    }
    
    //go over final sections and check overlaps caused by -place
    for(auto it = finalSectionsOrder.begin(); it != finalSectionsOrder.end() && next(it) != finalSectionsOrder.end(); it++){
      string sec1 = it->second;
      string sec2 = next(it)->second;
      if((*(finalSections))[sec1]->baseAddress + (*(finalSections))[sec1]->size > (*(finalSections))[sec2]->baseAddress){
        cout << "ERROR: sections: " << (*(finalSections))[sec1]->label << " and " << (*(finalSections))[sec2]->label << " are overlapping" << endl;
        exit(-1);
      }
    }
  }

 
  // taking symbol tables from files
  for(int i = 0; i < fileNames.size(); i++){

    if((*files)[i].is_open()){

      int symbolsNum;
      (*files)[i].read(reinterpret_cast<char*>(&symbolsNum), sizeof(int)); // number of entries in the symbol table
      
      for(int j = 0; j < symbolsNum; j++){

        int symbolID;
        (*files)[i].read(reinterpret_cast<char*>(&symbolID), sizeof(int)); // index of the symbol

        string symbolName;
        size_t symbolNameSize;

        (*files)[i].read(reinterpret_cast<char*>(&symbolNameSize), sizeof(size_t)); // length of the symbol name
        symbolName.resize(symbolNameSize);
        (*files)[i].read(&symbolName[0], symbolNameSize);

        int symbolSection, symbolOffset, symbolType;
        (*files)[i].read(reinterpret_cast<char*>(&symbolSection), sizeof(int)); // -1 - symbol is a section; 0 - not defined; 1+ - user sections
        (*files)[i].read(reinterpret_cast<char*>(&symbolOffset), sizeof(int)); // symbol offset in the section (from 0)
        (*files)[i].read(reinterpret_cast<char*>(&symbolType), sizeof(int)); // 0 - local; 1 - global; 2 - extern
        
        
        string valueOfEQU;
        size_t valueSize;


        (*files)[i].read(reinterpret_cast<char*>(&valueSize), sizeof(size_t)); // length of the symbol name
        valueOfEQU.resize(valueSize);
        (*files)[i].read(&valueOfEQU[0], valueSize);
        // if(valueSize != 0){
          
        // }
        // else {
        //   valueOfEQU = "";
        // }
        

        
        
        if(symbolType ==  1){
          // GLOBAL
          
          if(definedSymbols.count(symbolName) > 0){
            cout << "ERROR: symbol: " << symbolName << " is double defined" << endl;
            exit(-1);
          } else {
            // add it as defined
            definedSymbols.insert(symbolName);
          }

          if(undefinedSymbols.count(symbolName) > 0){
            // if it was undefined delete it from the undefined set
            undefinedSymbols.erase(symbolName);
          }
        } else if(symbolType == 2){
          // EXTERN
          if(definedSymbols.count(symbolName) == 0){
            // not defined add it as undefined
            if(undefinedSymbols.count(symbolName) == 0){
              // not in the set
              undefinedSymbols.insert(symbolName);
            }
          }
        }

        if(symbolType == 1){
          //cout << symbolName << endl;
          // GLOBAL symbol - add it to the linker symbol table
          if(valueSize != 0){
            // EQU SYMBOL
            //cout << valueOfEQU << endl;
            LinkerSymbolTableEntry* newEntry = new LinkerSymbolTableEntry(symbolName, 0, 0, true, valueOfEQU);
            linkerSymbolTable->insert({symbolName, newEntry});
          } else {
            for(LinkerSectionTableEntry* elem: *linkerSectionTable){
          
              if(elem->sectionOldID == symbolSection && elem->fileID == i){
                map<string, FinalSectionsEntry*>::iterator iterFinalSections = finalSections->find(elem->sectionName);
                LinkerSymbolTableEntry* newEntry = new LinkerSymbolTableEntry(symbolName, symbolOffset + elem->newBaseAddress, iterFinalSections->second->idSection, false, "");
                linkerSymbolTable->insert({symbolName, newEntry});
                
                break;
              }
            }
          }
        }
      }
    }
  }

  // finished reading all symbol tables
  if(!undefinedSymbols.empty()){
    cout << "ERROR: there are undefined symbols in the table: ";
    for(auto elem: undefinedSymbols){
      cout << elem << " ";
    }
    cout << endl;
    exit(-1);
  }

  //printFinalSectionTable();
  //printSymbolTable();
  
  // taking relocationtables from files
  for(int i = 0; i < fileNames.size(); i++){

    if((*files)[i].is_open()){

      int sectionsNum;
      (*files)[i].read(reinterpret_cast<char*>(&sectionsNum), sizeof(int)); // number of sections

      
      for(int j = 0; j < sectionsNum; j++){

        int relocationEntriesNum;
        (*files)[i].read(reinterpret_cast<char*>(&relocationEntriesNum), sizeof(int)); // number of relocation entries in the section
        //cout << "PRE" << endl;
        

        for(int e = 0; e < relocationEntriesNum; e++){
          // read one entry

          int relocOffset, relocType, relocAddend;
          size_t valueLength;
          string relocValue;
          (*files)[i].read(reinterpret_cast<char*>(&relocOffset), sizeof(int)); // offset from the section
          (*files)[i].read(reinterpret_cast<char*>(&relocType), sizeof(int)); // 1 - value is section name ; 2 - value is symbol name
          
          
          
          (*files)[i].read(reinterpret_cast<char*>(&valueLength), sizeof(size_t)); // length of section/symbol name
          relocValue.resize(valueLength);
          (*files)[i].read(&relocValue[0], valueLength);

          
          (*files)[i].read(reinterpret_cast<char*>(&relocAddend), sizeof(int)); // addend
          //cout << "POSLE" << endl;
          size_t currSecLength;
          string currSec;
          
          (*files)[i].read(reinterpret_cast<char*>(&currSecLength), sizeof(size_t)); // length of section/symbol name
          
          currSec.resize(currSecLength);
          (*files)[i].read(&currSec[0], currSecLength);

          

          unsigned long byteToRelocate;
          unsigned long valueToWrite;
          string hexValueToWrite;
          
          for(LinkerSectionTableEntry* elem : *linkerSectionTable){
            if(elem->sectionName == currSec && elem->fileID == i){
              
              byteToRelocate = elem->newBaseAddress + relocOffset;
              
              break;
          
            }
          }
          
          
          if(relocType == 1){
            
            // value is name of the section

            if(relocatable){
              //RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 1, iter->second->sectionName, iter->second->offset, activeSection->name);
              
              map<string, FinalSectionsEntry*>::iterator iterFinal = finalSections->find(currSec);
              unsigned long offset = byteToRelocate - iterFinal->second->baseAddress;

              unsigned long offsetSection;

              for(LinkerSectionTableEntry* elem : *linkerSectionTable){
                if(elem->sectionName == relocValue && elem->fileID == i){
                  //map<string, FinalSectionsEntry*>::iterator itRelocSec = finalSections->find(relocValue);
                  map<string, FinalSectionsEntry*>::iterator iterF = finalSections->find(relocValue);
                  offsetSection = elem->newBaseAddress - iterF->second->baseAddress;
                  break;
                }
              }
              LinkerRelocationTableEntrance * newEntry = new LinkerRelocationTableEntrance(offset, 1, relocValue, offsetSection + relocAddend, currSec);
              iterFinal->second->relocationTable->push_back(newEntry);
            }
            
            for(LinkerSectionTableEntry* elem : *linkerSectionTable){
              if(elem->sectionName == relocValue && elem->fileID == i){
                //map<string, FinalSectionsEntry*>::iterator itRelocSec = finalSections->find(relocValue);
                valueToWrite = elem->newBaseAddress + relocAddend;
                
                hexValueToWrite = toLEHexPrint(valueToWrite); // little endian format
                break;
              }
            }
          } else {
            // value is symbol
            
            if(relocatable){
              map<string, FinalSectionsEntry*>::iterator iterFinal = finalSections->find(currSec);
              unsigned long offset = byteToRelocate - iterFinal->second->baseAddress;

              LinkerRelocationTableEntrance * newEntry = new LinkerRelocationTableEntrance(offset, 2, relocValue, relocAddend, currSec);
              
              iterFinal->second->relocationTable->push_back(newEntry);
            }
            

            map<string, LinkerSymbolTableEntry*>::iterator iter = linkerSymbolTable->find(relocValue);

            unsigned long temp = 0;

            if(iter->second->equSymbol){
              valueToWrite = stoul(iter->second->value, nullptr, 16);
            } else {
              valueToWrite = iter->second->offset;
            }
 
            hexValueToWrite = toLEHexPrint(valueToWrite);
            
          }
          
          
          // write hexCode with byteToRelocate and hexValueToWrite
          if(!relocatable){
            for(auto it = finalSections->begin(); it != finalSections->end(); ++it){
              if(it->second->label ==  currSec){
                byteToRelocate -= it->second->baseAddress;
                //cout << byteToRelocate << endl;
                for(int i = 0; i< 8; i++){
                  (*(it->second->machineCode))[byteToRelocate*2 + i] = hexValueToWrite[i];
                }
                
                break;
              }
            }
          }
          
        }
      }


    }
  }

  if(hex){
    printHexToFile();
  } else {
    printRelocatableToFile();
  }

  // finished reading -> closing files
  for(int i = 0; i < files->size(); i++){
    (*files)[i].close();
  }

  //printSymbolTable();
  //printLinkerSectionTable();
  //printFinalRelocationSectionTable();
  //printFinalSectionTable();

}

void printLinkerSectionTable(){
  cout << "oldID\t" << "section\t" << "fileID\t" << "baseAddress (B)\t" << "sectionSize (B)\t" << "literalPoolSize (B)" << endl;
  for(LinkerSectionTableEntry* elem: *linkerSectionTable){
    cout << elem->sectionOldID << " " << elem->sectionName << " " << elem->fileID << " ";
    cout << elem->newBaseAddress << " " << elem->size << " " << elem->literalPoolSize << endl;
    int cnt = 0;
    for(char elem : *(elem->hexCode)){
      if(cnt % 2 == 1){
        if(cnt == 15){
          cnt = 0;
          cout << elem << endl;
        } else {
          cout << elem << " ";
          cnt++;
        }
      } else {
        cout << elem;
        cnt++;
      }
    }
    cout << endl;
  }
}

void printFinalSectionTable(){
  cout << "ID " << "name " << "baseAddress " << "size (B)" << endl;
  for(auto it = finalSectionsOrder.begin(); it != finalSectionsOrder.end(); ++it){
    FinalSectionsEntry* sec = (*(finalSections))[it->second];
    cout << sec->idSection << " " << sec->label << "\t" << sec->baseAddress << "\t" << sec->size << endl;

    int cnt = 0;
    for(char elem : *(sec->machineCode)){
      if(cnt % 2 == 1){
        if(cnt == 15){
          cnt = 0;
          cout << elem << endl;
        } else {
          cout << elem << " ";
          cnt++;
        }
      } else {
        cout << elem;
        cnt++;
      }
    }
    cout << endl;
  }
}

void printFinalRelocationSectionTable(){
  cout << "ID " << "name " << "baseAddress " << "size (B)" << endl;
  for(string it : relocationFinalSectionsOrder){
    FinalSectionsEntry* sec = (*(finalSections))[it];
    cout << "relocation" << endl;
    for(LinkerRelocationTableEntrance* p: *(sec->relocationTable)){
      p->printRelocationTableEntrance();
    }
    cout << sec->idSection << " " << sec->label << "\t" << sec->baseAddress << "\t" << sec->size << endl;

    int cnt = 0;
    for(char elem : *(sec->machineCode)){
      if(cnt % 2 == 1){
        if(cnt == 15){
          cnt = 0;
          cout << elem << endl;
        } else {
          cout << elem << " ";
          cnt++;
        }
      } else {
        cout << elem;
        cnt++;
      }
    }
    cout << endl;
  }
}

void printSymbolTable(){
  cout << "name " << "offset " << "section " << "value" << endl;
  for(auto it = linkerSymbolTable->begin(); it != linkerSymbolTable->end(); ++it){
    cout << it->second->label << " " << it->second->offset << " " << it->second->section << " " << it->second->value << endl;
  }
}

string toLEHexPrint(int num){
  
  unsigned int littleEndianValue = ((num & 0xFF000000) >> 24) |
                                    ((num & 0x00FF0000) >> 8) |
                                    ((num & 0x0000FF00) << 8) |
                                    ((num & 0x000000FF) << 24);
  stringstream ss;
  ss << std::setw(8) << std::setfill('0') << hex << uppercase << littleEndianValue;

  string result = ss.str();
  return result;
}


void printRelocatableToFile(){

  ofstream outputBinaryFile;

  outputBinaryFile.open(outputFileName, std::ios::binary);

  // sending SECTIONS
  size_t size = finalSections->size();
  int sz = 0;
  outputBinaryFile.write(reinterpret_cast<char*>(&(size)), sizeof(int)); // number of Sections
  
  for(string it: relocationFinalSectionsOrder){
    FinalSectionsEntry* elem = (*finalSections)[it];
    outputBinaryFile.write(reinterpret_cast<char*>(&(elem->idSection)), sizeof(int)); // section ID

    size = elem->label.size();
    outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); // length of the section name
    outputBinaryFile.write(elem->label.data(), size); // section name

    
    outputBinaryFile.write(reinterpret_cast<char*>(&(elem->size)), sizeof(int)); // machine code length without the pool
    outputBinaryFile.write(reinterpret_cast<char*>(&(sz)), sizeof(int)); // pool of literals length (0)
    
    size = elem->machineCode->size();
    outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); // length of the hexCode

    outputBinaryFile.write(elem->machineCode->data(), size); // send hexCode
  }
 

  map<string, LinkerSymbolTableEntry*>::iterator iterSymbol;

  // sending SYMBOL TABLE
  size = linkerSymbolTable->size();
  outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(int)); // number of symbols in the symbol table

  for(iterSymbol = linkerSymbolTable->begin(); iterSymbol != linkerSymbolTable->end(); iterSymbol++){

    outputBinaryFile.write(reinterpret_cast<char*>(&(sz)), sizeof(int)); // symbol ID NOT IMPORTANT

    size = iterSymbol->second->label.size();
    outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); // length of name of symbol/section
    outputBinaryFile.write(iterSymbol->second->label.data(), size); // name of symbol/section

    outputBinaryFile.write(reinterpret_cast<char*>(&(iterSymbol->second->section)), sizeof(int)); // section of the symbol (-1 if section)
    outputBinaryFile.write(reinterpret_cast<char*>(&(iterSymbol->second->offset)), sizeof(int)); // offset of the symbol in the section (0 if section)
    
    // all symbols after linking are GLOBAL
    sz = 1;
    outputBinaryFile.write(reinterpret_cast<char*>(&(sz)), sizeof(int)); // 0 - local; 1 - global; 2 - extern

    // sending value of EQU SYMBOL
    size = iterSymbol->second->value.size();
    outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); // length of name of symbol/section
    outputBinaryFile.write(iterSymbol->second->value.data(), size);
    //outputBinaryFile.write("", size); // name of symbol/section
  }

  //map<int, Section*>::iterator iterSec;

  

  // sending RELOCATION TABLES
  size = finalSections->size();
  outputBinaryFile.write(reinterpret_cast<char*>(&(size)), sizeof(int)); // number of Sections

  for(string it: relocationFinalSectionsOrder){
    FinalSectionsEntry* finalSec = (*finalSections)[it];
    
    size = finalSec->relocationTable->size();
    outputBinaryFile.write(reinterpret_cast<char*>(&(size)), sizeof(int)); // number of relocation table entries
    //cout << size << endl
    for(LinkerRelocationTableEntrance* elem: *(finalSec->relocationTable)){
      outputBinaryFile.write(reinterpret_cast<char*>(&(elem->offset)), sizeof(int)); // offset in the current section that should be fixed
      outputBinaryFile.write(reinterpret_cast<char*>(&(elem->type)), sizeof(int)); // type of relocation (1 - local; 2 - global)
      
      size = elem->value.size();
      outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); // length of name of symbol/section in reloc table
      outputBinaryFile.write(elem->value.data(), size); // name of symbol/section in reloc table

      outputBinaryFile.write(reinterpret_cast<char*>(&(elem->addend)), sizeof(int)); // addend that we should add with the real value and then offset

      size = elem->currSection.size();
      outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); // length of current section in reloc table
      outputBinaryFile.write(elem->currSection.data(), size); // name of current section in reloc table
    }
  }

  outputBinaryFile.close();
}

void printHexToFile(){

  ofstream outputBinaryFile;
  ofstream outputTextFile;
    
  
  
  string binary = outputFileName.substr(0, outputFileName.length() - 3) + "o";

  outputBinaryFile.open(binary, std::ios::binary);
  outputTextFile.open(outputFileName);

  if(outputTextFile.is_open() && outputBinaryFile.is_open()){
    bool firstLine = true;
    for(auto it = finalSectionsOrder.begin(); it != finalSectionsOrder.end(); ++it){
      unsigned long addressCount = (*(finalSections))[it->second]->baseAddress;

      for(int i = 0; i < (*(finalSections))[it->second]->machineCode->size() - 1; i+=2){
        if(addressCount % 8 == 0){
          // we need new line
          if(firstLine){
            firstLine = false;
          }
          else{
            outputTextFile << endl;
          }
          outputTextFile << hex << setfill('0') << setw(4) << addressCount << ": ";
        }
        char first = (*(*(finalSections))[it->second]->machineCode)[i];
        char second = (*(*(finalSections))[it->second]->machineCode)[i+1];
        outputTextFile << first << second << " ";
        addressCount += 1;

        outputBinaryFile.write(reinterpret_cast<char*>(&first), sizeof(char));
        outputBinaryFile.write(reinterpret_cast<char*>(&second), sizeof(char));
      }
    }
  }
  outputBinaryFile.close();
  outputTextFile.close();
}