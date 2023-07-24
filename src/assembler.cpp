#include <string>
#include <cstring>
#include <map>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include "../inc/assembler.hpp"

using namespace std;

extern bool secondPass;
extern int locationCounter;
extern Section* activeSection;

extern map<string, SymbolTableEntry*> *symbolTable;
vector<string> symbolTableInsertionOrder;
extern map<int, Section*> *allSections;


extern string outputFileName;


//map<string, string> equSymbols;

const int pcOffset = 4;

// NAPOMENA: ako se odlucim da uradim EQU direktivu, kod svake instrukcije koja koristi simbol dodaj jos jedan if koji nakon trazenja u tabeli simbola proverava da li je simbol nadjen (ako nije baca gresku) 
// a proverava i da li je apsolutan - ako je apsolutan onda se vrednost iz tabele simbola ugradjuje u bazen literala i radi pomeraj

void printOutputTextFile();

string toHex(int num, bool ins){
  ostringstream oss;
  oss << hex << uppercase << num;
  string hexString = oss.str();
  if(ins && hexString.size() > 3){
    return hexString.substr(hexString.size() - 3);
  }
  return hexString;
}

string toHexPrint(int num){
  ostringstream oss;
  oss << hex << uppercase << num;
  string hexString = oss.str();
  
  while(hexString.length()<8){
    hexString = "0" + hexString;
  }
  return hexString;
}

void assemblerAddLabel(string* symbol){
  if(activeSection == nullptr){
    cout << "ERROR: Label" << *symbol << " is not defined in the section";
    exit(-1);
  }

  if(!secondPass){
    // FIRST PASS - we add every symbol we find to the Symbol Table

    map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*symbol);

    if(iter == symbolTable->end()){
      // symbol not found in the table

      SymbolTableEntry* newEntry = new SymbolTableEntry(*symbol, locationCounter, 0 , activeSection->idSection, true, activeSection->name, 0, "");
      symbolTable->insert({*symbol, newEntry});
      symbolTableInsertionOrder.push_back(*symbol);

    } else{
      // symbol is in the table
      if(iter->second->isDefined == true){
        cout << "ERROR: Symbol " << *symbol <<" is already defined in the Symbol Table";
        exit(-1);

      }else {
        //symbol is not defined (global or extern)
        if(iter->second->section == 0){
          //symbol has appeared before but as a global or word -> now we need to define it
          iter->second->section = activeSection->idSection;
          iter->second->isDefined = true;
          iter->second->offset = locationCounter;
        }
      }
    }
  }
  //printSymbolTable();
  
}

void assemblerGlobalDIR(MultipleArguments* arguments){

  if(!secondPass){

    for (int i = 0; i< arguments->argumentName->size(); i++){
      string globalDirName = *(*(arguments->argumentName))[i];

      map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(globalDirName);

      if(iter == symbolTable->end()){
        // global symbol is not in the symbol table - add it as undefined
        SymbolTableEntry* newEntry = new SymbolTableEntry(globalDirName, 0, 1, 0, false, "", 0, "");
        symbolTable->insert({globalDirName, newEntry});
        symbolTableInsertionOrder.push_back(globalDirName);
      } else {
        // symbol in the table should become global
        iter->second->type = 1;

      }
    }
  }

  if(secondPass){
    for (int i = 0; i< arguments->argumentName->size(); i++){
      string globalDirName = *(*(arguments->argumentName))[i];

      map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(globalDirName);
      if(!iter->second->isDefined){
        cout << "ERROR: Global symbol is not defined";
        exit(-1);
      }
    }
    
  }

  //printSymbolTable();
}

void assemblerExternDIR(MultipleArguments* arguments){

  if(!secondPass){

    for (int i = 0; i< arguments->argumentName->size(); i++){
      string externDirName = *(*(arguments->argumentName))[i];

      map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(externDirName);

      if(iter == symbolTable->end()){
        // extern symbol is not in the symbol table - add it as undefined
        SymbolTableEntry* newEntry = new SymbolTableEntry(externDirName, 0, 2, 0, false, "", 0, "");
        symbolTable->insert({externDirName, newEntry});
        symbolTableInsertionOrder.push_back(externDirName);
      } else {
        // symbol in the table should become extern
        iter->second->type = 2;
      }
    }
  }
  
  //printSymbolTable();
}

void assemblerSectionDIR(string* name){

  map<string, SymbolTableEntry*>::iterator iterSymbol = symbolTable->find(*name);
  
  if(!secondPass){
    // FIRST PASS
    if(iterSymbol != symbolTable->end()){
      // section is already defined
      cout << "ERROR: Section" << *name << " is already defined in the Symbol Table";
      exit(-1);
    }

    if(activeSection != nullptr){
      // closing an active section (insert it to allSections)
      activeSection->size = locationCounter;
      allSections->insert({activeSection->idSection, activeSection});

    }

    SymbolTableEntry *newEntry = new SymbolTableEntry(*name, 0, 0, -1, true, "", 0, "");
    symbolTable->insert({*name, newEntry});
    symbolTableInsertionOrder.push_back(*name);
    Section *sec = new Section(newEntry->entranceIndex, *name);
    activeSection = sec;
    locationCounter = 0;

  } else {
    // SECOND PASS - make the pool of literals in the current active section AND take the next active section from Symbol Table
    
    if(activeSection && !activeSection->literalTable->empty()){

      for(auto key: (*(activeSection->literalsInsertionOrder))){
        activeSection->literalPoolSize += 4;
        if((*(activeSection->literalTable))[key]->equSymbol){
          unsigned long value = stoul((*(activeSection->literalTable))[key]->value, nullptr, 16);
          
          unsigned long littleEndianValue = ((value & 0xFF000000) >> 24) |
                                            ((value & 0x00FF0000) >> 8) |
                                            ((value & 0x0000FF00) << 8) |
                                            ((value & 0x000000FF) << 24);

          stringstream ss;
          ss << std::setw(8) << std::setfill('0') << hex << uppercase << littleEndianValue;

          string result = ss.str();
          for(int i = 0; i< result.length(); i++){
            activeSection->hexCode->push_back(result[i]);
          }
        }
        else if((*(activeSection->literalTable))[key]->isSymbol){
          for(int i = 0; i<8; i++){
            activeSection->hexCode->push_back('0');
          }
        }else{
          
          unsigned long value = stoul((*(activeSection->literalTable))[key]->name, nullptr, 16);

          unsigned long littleEndianValue = ((value & 0xFF000000) >> 24) |
                                            ((value & 0x00FF0000) >> 8) |
                                            ((value & 0x0000FF00) << 8) |
                                            ((value & 0x000000FF) << 24);
          
          stringstream ss;
          ss << std::setw(8) << std::setfill('0') << hex << uppercase << littleEndianValue;

  
          string result = ss.str();
          for(int i = 0; i< result.length(); i++){
            activeSection->hexCode->push_back(result[i]);
          }
        }
      }
    }
    

    map<int, Section*>::iterator iterSection = allSections->find(iterSymbol->second->entranceIndex);
    activeSection = iterSection->second;
    locationCounter = 0;
  }
}

void assemblerWordDIr(MultipleArguments* arguments){
  if(!secondPass){
    locationCounter+=4*arguments->argumentName->size();  

    if(locationCounter > 4096){
        cout << "ERROR in WORD: Section shouldn't be larger than 4096B";
        exit(-1);
    }
  }

  if(secondPass){
    for (int i = 0; i< arguments->argumentName->size(); i++){

      string wordDirName = *(*(arguments->argumentName))[i];
      int argumentType = (*arguments->argumentType)[i];

      if(argumentType == 0){
        // argument is a SYMBOL

        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(wordDirName);

        if(iter == symbolTable->end()){
          cout << "ERROR: WORD symbol " << wordDirName << " is not extern nor global nor local";
          exit(-1);
        }
        
        // we put relocation table for all cases (linker will now the final value of the symbol whatever type it is)
          
        for(int i = 0; i < 8; i++){
            activeSection->hexCode->push_back('0');
        }

        // RELOCATION TABLE
        if(iter->second->type == 0){
          // LOCAL
          RelocationTableEntrance *newEntry = new RelocationTableEntrance(locationCounter, 1, iter->second->sectionName, iter->second->offset, activeSection->name);
          activeSection->relocationTable->push_back(newEntry);
        } else {
          // GLOBAL / EXTERN
          RelocationTableEntrance *newEntry = new RelocationTableEntrance(locationCounter, 2, iter->second->label, 0, activeSection->name);
          activeSection->relocationTable->push_back(newEntry);
        }
        
        
        
        
      }  else if(argumentType == 1){
        // argument is a LITERAL
        // we will push it to activeSection machine code

        int base = 10;
        if(wordDirName.substr(0, 2) == "0x"){
          base = 16;
          wordDirName = wordDirName.substr(2);
        } else if (wordDirName[0] == '0'){
          base = 8;
        }

        unsigned long val = stoul(wordDirName, nullptr, base);
        string hexVal = toHex(val, false);
        
        int j = 0;
        for(int i = hexVal.length() - 1; i > 0; i--){
          activeSection->hexCode->push_back(hexVal[i]);
          j++;
        }
        // for( int i = 0; i<hexVal.length(); i+=2){
        //     if(i+1 == hexVal.length()){
        //       activeSection->hexCode->push_back("0" + string(1, hexVal[i]));
        //       j++;
        //     } else{
        //       activeSection->hexCode->push_back(hexVal.substr(i, 2));
        //       j++;
        //     }
        // }
        while(j < 8){
          activeSection->hexCode->push_back('0');
          j++;
        }
      }
      locationCounter += 4;

    }    
  }  
}

void assemblerSkipDIR(string *value){ // FINISHED

  // turn string value into literal

  int base = 10;
  if((*value).substr(0, 2) == "0x"){
    base = 16;
    (*value) = (*value).substr(2);
  } else if ((*value)[0] == '0'){
    base = 8;
  }

  unsigned long val = stoul((*value), nullptr, base);

  if(secondPass){
    
    // add to active section
    for( int i = 0; i<val; i++){
      activeSection->hexCode->push_back('0');
      activeSection->hexCode->push_back('0');
    }
    
  }

  locationCounter += val;
  
  if(locationCounter > 4096){
      cout << "ERROR in skip: Section shouldn't be larger than 4096B";
      exit(-1);
  }

}

void assemblerASCIIDir(string *argument){

  string argum = *argument;
  int length = argum.substr(1, argum.length()-2).length(); // removing quotation marks
  if(!secondPass){
    for(int i = 1; i<=length; i++){
      int arg1 = static_cast<int>((*argument)[i]);
      if(i+1 != length + 1 && arg1 == 92){
        continue;
      }
      locationCounter+=1;
    }
  }
  
  if(secondPass){

    for( int i = 1; i<=length; i++){
      int arg1 = static_cast<int>((*argument)[i]);
      if(i+1 != length + 1 && arg1 == 92){
        // arg1 is an '\' (escape) character
        char c = (*argument)[i+1];
        switch(c){ // check a special sequence
          case 'a': {
            arg1 = 7;
            i++;
            break;
          }
          case 'b': {
            arg1 = 8;
            i++;
            break;
          }
          case 'f': {
            arg1 = 12;
            i++;
            break;
          }
          case 'n': {
            arg1 = 10;
            i++;
            break;
          }
          case 'r': {
            arg1 = 13;
            i++;
            break;
          }
          case 't': {
            arg1 = 9;
            i++;
            break;
          }
          case 'v': {
            arg1 = 11;
            i++;
            break;
          }
          default: continue;
        }
      }

      string hexValue = toHex(arg1, false);

      if(hexValue.length() == 1){
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back(hexValue[0]);
      } else {
        activeSection->hexCode->push_back(hexValue[0]);
        activeSection->hexCode->push_back(hexValue[1]);
      }
      locationCounter+=1;

    }
  }
  if(locationCounter > 4096){
      cout << "ERROR in ASCII: Section shouldn't be larger than 4096B";
      exit(-1);
  }

}

void assemblerEQUDir(string *symbol, MultipleArguments* arguments){
  // we are processing this function only in FIRST PASS
  if(!secondPass){
    if(arguments->argumentName->size() == 1 && arguments->addressing == 0){
      // ONE LITERAL
      string exp = *(*(arguments->argumentName))[0];
      int base = 10;
      if((exp).substr(0, 2) == "0x"){
        base = 16;
        (exp) = (exp).substr(2);
      } else if ((exp)[0] == '0'){
        base = 8;
      }
      string hexValue;
      if(base != 16){
        unsigned long val = stoul((exp), nullptr, base);
        // convert it now to hexadecimal value to store it
        hexValue = toHex(val, false);
        

        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*symbol);

        if(iter == symbolTable->end()){
          // not in the symbol table
          SymbolTableEntry* newEntry = new SymbolTableEntry(*symbol, 0, 3, 0, 1, "", 1, hexValue);
          symbolTable->insert({*symbol, newEntry});
        } else {
          // in the symbol table, define it
          iter->second->isAbsolute = true;
          iter->second->valueOfAbsolute = hexValue;
          iter->second->isDefined = true;
        }
        
        
      } else {
      
        hexValue = (exp); // remove 0x
        
        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*symbol);

        if(iter == symbolTable->end()){
          // not in the symbol table
          SymbolTableEntry* newEntry = new SymbolTableEntry(*symbol, 0, 3, 0, 1, "", 1, hexValue);
          symbolTable->insert({*symbol, newEntry});
        } else {
          // in the symbol table, define it
          iter->second->isAbsolute = true;
          iter->second->valueOfAbsolute = hexValue;
          iter->second->isDefined = true;
        }
      
        //equSymbols.insert({*symbol, hexValue});
        //cout << *expression << endl;
      }
    } else if(arguments->addressing == 2 || arguments->addressing == 3){
      // two literals
      
      string exp1 = *(*(arguments->argumentName))[0];
      string exp2 = *(*(arguments->argumentName))[1];
      int base = 10;
      if((exp1).substr(0, 2) == "0x"){
        base = 16;
        (exp1) = (exp1).substr(2);
      } else if ((exp1)[0] == '0'){
        base = 8;
      }
      unsigned long val1 = stoul((exp1), nullptr, base);

      base = 10;
      if((exp2).substr(0, 2) == "0x"){
        base = 16;
        (exp2) = (exp2).substr(2);
      } else if ((exp2)[0] == '0'){
        base = 8;
      }
      unsigned long val2 = stoul((exp2), nullptr, base);
      unsigned long val;

      
      if(arguments->addressing == 2){
        // PLUS
        val = val1 + val2;
      } else {
        val = val1 - val2;
      }
      string hexValue = toHex(val, false);
      
      
      map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*symbol);

      if(iter == symbolTable->end()){
        // not in the symbol table
        SymbolTableEntry* newEntry = new SymbolTableEntry(*symbol, 0, 3, 0, 1, "", 1, hexValue);
        symbolTable->insert({*symbol, newEntry});
      } else {
        // in the symbol table, define it
        iter->second->isAbsolute = true;
        iter->second->valueOfAbsolute = hexValue;
        iter->second->isDefined = true;
      }
    }
  } else {
    // SECOND PASS
    if(arguments->addressing == 1){
      // one symbol
      map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*(*(arguments->argumentName))[0]);

      if(iter == symbolTable->end()){
        cout << "ERROR: EQU symbol definition not found in symbol table: " << *(*(arguments->argumentName))[0] << endl;
        exit(-1);
      }
      SymbolTableEntry* newEntry = new SymbolTableEntry(*symbol, iter->second->offset, iter->second->type, iter->second->section, 1, iter->second->sectionName, 0, "");
      symbolTable->insert({*symbol, newEntry});
    }
    else if(arguments->addressing == 4){
      // PLUS SYMBOLS
      string sym1 = *(*(arguments->argumentName))[0];
      string sym2 = *(*(arguments->argumentName))[1];


      map<string, SymbolTableEntry*>::iterator iter1 = symbolTable->find(sym1);
      map<string, SymbolTableEntry*>::iterator iter2 = symbolTable->find(sym2);

      if(iter1 == symbolTable->end() || iter2 == symbolTable->end()){
        cout << "ERROR in EQU: Symbol is not defined in the symbol table in the EQU" << endl;
        exit(-1);
      }
      if(iter1->second->section != iter2->second->section){
        cout << "ERROR in EQU: EQU symbols are not in the same section" << endl;
        exit(-1);
      }

      int offset1 = iter1->second->offset;
      int offset2 = iter2->second->offset;

      SymbolTableEntry* newEntry = new SymbolTableEntry(*symbol, 0, 3, 0, 1, "", 1, toHex(offset1 + offset2, false));
      symbolTable->insert({*symbol, newEntry});

    } else if(arguments->addressing == 5){
      // MINUS SYMBOLS

      string sym1 = *(*(arguments->argumentName))[0];
      string sym2 = *(*(arguments->argumentName))[1];

      map<string, SymbolTableEntry*>::iterator iter1 = symbolTable->find(sym1);
      map<string, SymbolTableEntry*>::iterator iter2 = symbolTable->find(sym2);

      if(iter1 == symbolTable->end() || iter2 == symbolTable->end()){
        cout << "ERROR in EQU: Symbol is not defined in the symbol table in the EQU" << endl;
        exit(-1);
      }
      if(iter1->second->section != iter2->second->section){
        cout << "ERROR: EQU symbols are not in the same section" << endl;
        exit(-1);
      }

      int offset1 = iter1->second->offset;
      int offset2 = iter2->second->offset;

      SymbolTableEntry* newEntry = new SymbolTableEntry(*symbol, 0, 3, 0, 1, "", 1, toHex(offset1 - offset2, false));
      symbolTable->insert({*symbol, newEntry});

    }
  }
}

void assemblerENDDIR(){
  if(!secondPass){
    if(activeSection != nullptr){
      activeSection->size = locationCounter;
      allSections->insert({activeSection->idSection, activeSection});
    }
  }

  

  if(secondPass){

    if(activeSection && !activeSection->literalTable->empty()){
      for(auto key: (*(activeSection->literalsInsertionOrder))){
        activeSection->literalPoolSize += 4;
        if((*(activeSection->literalTable))[key]->equSymbol){
          unsigned long value = stoul((*(activeSection->literalTable))[key]->value, nullptr, 16);
          
          unsigned long littleEndianValue = ((value & 0xFF000000) >> 24) |
                                            ((value & 0x00FF0000) >> 8) |
                                            ((value & 0x0000FF00) << 8) |
                                            ((value & 0x000000FF) << 24);

          stringstream ss;
          ss << std::setw(8) << std::setfill('0') << hex << uppercase << littleEndianValue;

          string result = ss.str();
          for(int i = 0; i< result.length(); i++){
            activeSection->hexCode->push_back(result[i]);
          }
        }
        else if((*(activeSection->literalTable))[key]->isSymbol){
          for(int i = 0; i < 8; i++){
            activeSection->hexCode->push_back('0');
          }
        }else{
         
          unsigned long value = stoul((*(activeSection->literalTable))[key]->name, nullptr, 16);

          unsigned long littleEndianValue = ((value & 0xFF000000) >> 24) |
                                            ((value & 0x00FF0000) >> 8) |
                                            ((value & 0x0000FF00) << 8) |
                                            ((value & 0x000000FF) << 24);

          stringstream ss;
          ss << std::setw(8) << std::setfill('0') << hex << uppercase << littleEndianValue;

          string result = ss.str();
          for(int i = 0; i< result.length(); i++){
            activeSection->hexCode->push_back(result[i]);
          }
        }
      }
    }
    // PRINT cout
    // printSymbolTable();
    // if(!allSections->empty()){
    //   for(auto iter = allSections->begin(); iter != allSections->end(); ++iter){
    //     iter->second->printSection();
    //   }
    // } 
    // MAKE TEXT OUTPUT FILE

    printOutputTextFile();
    

    // MAKE BINARY OUTPUT FILE

    ofstream outputBinaryFile;
    
    outputBinaryFile.open(outputFileName, std::ios::binary);

    if(outputBinaryFile.is_open()){
      map<int, Section*>::iterator iterSection;

      // sending SECTIONS
      size_t size = allSections->size();
      outputBinaryFile.write(reinterpret_cast<char*>(&(size)), sizeof(int)); // number of Sections

      for(iterSection = allSections->begin(); iterSection != allSections->end(); iterSection++){
        outputBinaryFile.write(reinterpret_cast<char*>(&(iterSection->second->idSection)), sizeof(int)); // section ID

        size = iterSection->second->name.size();
        outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); // length of the section name
        outputBinaryFile.write(iterSection->second->name.data(), size); // section name

        outputBinaryFile.write(reinterpret_cast<char*>(&(iterSection->second->size)), sizeof(int)); // machine code length without the pool
        outputBinaryFile.write(reinterpret_cast<char*>(&(iterSection->second->literalPoolSize)), sizeof(int)); // pool of literals length

        // vector<string> to vector<char> so we could write it to file
        // vector<char> charHexCode;
        // for(const string& str: *(iterSection->second->hexCode)){
        //   charHexCode.insert(charHexCode.end(), str.begin(), str.end());
        // }
        size = iterSection->second->hexCode->size();
        outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); // length of the hexCode

        outputBinaryFile.write(iterSection->second->hexCode->data(), size); // send hexCode
      }

      map<string, SymbolTableEntry*>::iterator iterSymbol;

      // sending SYMBOL TABLE
      size = symbolTable->size();
      outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(int)); // number of symbols in the symbol table

      for(iterSymbol = symbolTable->begin(); iterSymbol != symbolTable->end(); iterSymbol++){
        outputBinaryFile.write(reinterpret_cast<char*>(&(iterSymbol->second->entranceIndex)), sizeof(int)); // symbol ID

        size = iterSymbol->second->label.size();
        outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); // length of name of symbol/section
        outputBinaryFile.write(iterSymbol->second->label.data(), size); // name of symbol/section

        outputBinaryFile.write(reinterpret_cast<char*>(&(iterSymbol->second->section)), sizeof(int)); // section of the symbol (-1 if section)
        outputBinaryFile.write(reinterpret_cast<char*>(&(iterSymbol->second->offset)), sizeof(int)); // offset of the symbol in the section (0 if section)
        outputBinaryFile.write(reinterpret_cast<char*>(&(iterSymbol->second->type)), sizeof(int)); // type of symbol (local or global or extern)
      
        // sending value of EQU SYMBOL
        size = iterSymbol->second->valueOfAbsolute.size();
        outputBinaryFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); // length of name of symbol/section
        outputBinaryFile.write(iterSymbol->second->valueOfAbsolute.data(), size); // name of symbol/section
      }

      //map<int, Section*>::iterator iterSec;

      // sending RELOCATION TABLES
      size = allSections->size();
      outputBinaryFile.write(reinterpret_cast<char*>(&(size)), sizeof(int)); // number of Sections

      for(iterSection = allSections->begin(); iterSection != allSections->end(); iterSection++){
        
        size = iterSection->second->relocationTable->size();
        outputBinaryFile.write(reinterpret_cast<char*>(&(size)), sizeof(int)); // number of relocation table entries
        //cout << size << endl;
        for(RelocationTableEntrance* elem: (*iterSection->second->relocationTable)){
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
  }
  
}

void assemblerHALT(){
  if(activeSection == nullptr){
    cout << "ERROR: HALT Instruction is not in the section";
    exit(-1);
  }
  
  if(secondPass){
    for(int i = 0; i < 8; i++){
      activeSection->hexCode->push_back('0');
    }
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in HALT: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerINT(){ // FINISHED
  if(activeSection == nullptr){
    cout << "ERROR: INT Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    activeSection->hexCode->push_back('1');
    for(int i = 0; i < 7; i++){
      activeSection->hexCode->push_back('0');
    }
  }
  
  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in INT: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerIRET(){
  if(activeSection == nullptr){
    cout << "ERROR: IRET Instruction is not in the section";
    exit(-1);
  }
  if(!secondPass){
    locationCounter += 8;
  }
  if(secondPass){
    // ISPRAVI REDOSLED INSTRUKCIJA

    // status <= mem[sp+4]
    activeSection->hexCode->push_back('9'); // op code
    activeSection->hexCode->push_back('6'); // MMMM
    activeSection->hexCode->push_back('0'); // AAAA (csr status)
    activeSection->hexCode->push_back('E'); // BBBB (sp)
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('4');


    // pc <= mem[sp] ; sp <= sp + 8
    activeSection->hexCode->push_back('9'); // op code
    activeSection->hexCode->push_back('3'); // MMMM
    activeSection->hexCode->push_back('F'); // AAAA (pc)
    activeSection->hexCode->push_back('E'); // BBBB (sp)
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('8');

    
    
    locationCounter += 8;
  }
  if(locationCounter > 4096){
      cout << "ERROR in IRET: Section shouldn't be larger than 4096B";
      exit(-1);
  }

}

void assemblerRET(){
  if(activeSection == nullptr){
    cout << "ERROR: RET Instruction is not in the section";
    exit(-1);
  }
  if(!secondPass){
    locationCounter += 4;
  }
  if(secondPass){
    string *reg = new string("r15");
    assemblerPOP(reg);
  }

  if(locationCounter > 4096){
      cout << "ERROR in RET: Section shouldn't be larger than 4096B";
      exit(-1);
  }

}

void assemblerPUSH(string* reg){
  if(activeSection == nullptr){
    cout << "ERROR: PUSH Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNum = stoi((*reg).substr(1));
    string hexValue = toHex(regNum, false);
      
   
    activeSection->hexCode->push_back('8'); // op code
    activeSection->hexCode->push_back('1'); // MMMM
    activeSection->hexCode->push_back('E'); // AAAA (sp)
    activeSection->hexCode->push_back('0'); // BBBB (0)
    activeSection->hexCode->push_back(hexValue[0]); // CCCC (gpr)
    activeSection->hexCode->push_back('0'); // DDDD
    activeSection->hexCode->push_back('0'); // DDDD
    activeSection->hexCode->push_back('4'); // DDDD
    
    
    
    //activeSection->printSection();
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in PUSH: Section shouldn't be larger than 4096B";
      exit(-1);
  }

}

void assemblerPOP(string* reg){
  if(activeSection == nullptr){
    cout << "ERROR: POP Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    if(*reg == "status"){
      int regNumProgram = 0;
      string hex = to_string(regNumProgram);

      activeSection->hexCode->push_back('9'); // op code
      activeSection->hexCode->push_back('7'); // MMMM (pop)
      activeSection->hexCode->push_back(hex[0]); // AAAA (csr)
      activeSection->hexCode->push_back('E'); // BBBB (sp)
      activeSection->hexCode->push_back('0'); // CCCC
      activeSection->hexCode->push_back('0'); // DDDD
      activeSection->hexCode->push_back('0'); // DDDD (sp += 4)
      activeSection->hexCode->push_back('4'); // DDDD
      
      

    } else {

      int regNum = stoi((*reg).substr(1));
      string hexValue = toHex(regNum, false);

      activeSection->hexCode->push_back('9'); // op code
      activeSection->hexCode->push_back('3'); // MMMM (pop)
      activeSection->hexCode->push_back(hexValue[0]); // AAAA (gpr)
      activeSection->hexCode->push_back('E'); // BBBB (sp)
      activeSection->hexCode->push_back('0'); // CCCC
      activeSection->hexCode->push_back('0'); // DDDD
      activeSection->hexCode->push_back('0'); // DDDD (sp += 4)
      activeSection->hexCode->push_back('4'); // DDDD
      
    }
  }
  
  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in POP: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerXCHG(string* regS, string* regD){
  if(activeSection == nullptr){
    cout << "ERROR: XCHG Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNumS = stoi((*regS).substr(1));
    string hexValueS = toHex(regNumS, false);
    int regNumD = stoi((*regD).substr(1));
    string hexValueD = toHex(regNumD, false);

    activeSection->hexCode->push_back('4'); // op code
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back(hexValueD[0]); // BBBB (destination)
    activeSection->hexCode->push_back(hexValueS[0]); // CCCC (source)
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');

  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in XCHG: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerADD(string* regS, string* regD){
  if(activeSection == nullptr){
    cout << "ERROR: ADD Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNumS = stoi((*regS).substr(1));
    string hexValueS = toHex(regNumS, false);
    int regNumD = stoi((*regD).substr(1));
    string hexValueD = toHex(regNumD, false);

    activeSection->hexCode->push_back('5'); // op code
    activeSection->hexCode->push_back('0'); // MMMM (add)
    activeSection->hexCode->push_back(hexValueD[0]); // AAAA
    activeSection->hexCode->push_back(hexValueD[0]); // BBBB
    activeSection->hexCode->push_back(hexValueS[0]); // CCCC
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');

  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in ADD: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerSUB(string* regS, string* regD){
  if(activeSection == nullptr){
    cout << "ERROR: SUB Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNumS = stoi((*regS).substr(1));
    string hexValueS = toHex(regNumS, false);
    int regNumD = stoi((*regD).substr(1));
    string hexValueD = toHex(regNumD, false);


    activeSection->hexCode->push_back('5'); // op code
    activeSection->hexCode->push_back('1'); // MMMM (sub)
    activeSection->hexCode->push_back(hexValueD[0]); // AAAA
    activeSection->hexCode->push_back(hexValueD[0]); // BBBB
    activeSection->hexCode->push_back(hexValueS[0]); // CCCC
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in SUB: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerMUL(string* regS, string* regD){
  if(activeSection == nullptr){
    cout << "ERROR: MUL Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNumS = stoi((*regS).substr(1));
    string hexValueS = toHex(regNumS, false);
    int regNumD = stoi((*regD).substr(1));
    string hexValueD = toHex(regNumD, false);

    activeSection->hexCode->push_back('5'); // op code
    activeSection->hexCode->push_back('2'); // MMMM (mul)
    activeSection->hexCode->push_back(hexValueD[0]); // AAAA
    activeSection->hexCode->push_back(hexValueD[0]); // BBBB
    activeSection->hexCode->push_back(hexValueS[0]); // CCCC
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in MUL: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerDIV(string* regS, string* regD){
  if(activeSection == nullptr){
    cout << "ERROR: DIV Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNumS = stoi((*regS).substr(1));
    string hexValueS = toHex(regNumS, false);
    int regNumD = stoi((*regD).substr(1));
    string hexValueD = toHex(regNumD, false);


    activeSection->hexCode->push_back('5'); // op code
    activeSection->hexCode->push_back('3'); // MMMM (div)
    activeSection->hexCode->push_back(hexValueD[0]); // AAAA
    activeSection->hexCode->push_back(hexValueD[0]); // BBBB
    activeSection->hexCode->push_back(hexValueS[0]); // CCCC
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in DIV: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerNOT(string* reg){
  if(activeSection == nullptr){
    cout << "ERROR: NOT Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNum = stoi((*reg).substr(1));
    string hexValue = toHex(regNum, false);

    activeSection->hexCode->push_back('6'); // op code
    activeSection->hexCode->push_back('0'); // MMMM(not)
    activeSection->hexCode->push_back(hexValue[0]); // AAAA
    activeSection->hexCode->push_back(hexValue[0]); // BBBB
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
     
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in NOT: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerAND(string* regS, string* regD){
  if(activeSection == nullptr){
    cout << "ERROR: AND Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNumS = stoi((*regS).substr(1));
    string hexValueS = toHex(regNumS, false);
    int regNumD = stoi((*regD).substr(1));
    string hexValueD = toHex(regNumD, false);

    activeSection->hexCode->push_back('6'); // op code
    activeSection->hexCode->push_back('1'); // MMMM (and)
    activeSection->hexCode->push_back(hexValueD[0]); // AAAA
    activeSection->hexCode->push_back(hexValueD[0]); // BBBB
    activeSection->hexCode->push_back(hexValueS[0]); // CCCC
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in AND: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerOR(string* regS, string* regD){
  if(activeSection == nullptr){
    cout << "ERROR: OR Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNumS = stoi((*regS).substr(1));
    string hexValueS = toHex(regNumS, false);
    int regNumD = stoi((*regD).substr(1));
    string hexValueD = toHex(regNumD, false);

    activeSection->hexCode->push_back('6'); // op code
    activeSection->hexCode->push_back('2'); // MMMM (or)
    activeSection->hexCode->push_back(hexValueD[0]); // AAAA
    activeSection->hexCode->push_back(hexValueD[0]); // BBBB
    activeSection->hexCode->push_back(hexValueS[0]); // CCCC
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in OR: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerXOR(string* regS, string* regD){
  if(activeSection == nullptr){
    cout << "ERROR: XOR Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNumS = stoi((*regS).substr(1));
    string hexValueS = toHex(regNumS, false);
    int regNumD = stoi((*regD).substr(1));
    string hexValueD = toHex(regNumD, false);

    activeSection->hexCode->push_back('6'); // op code
    activeSection->hexCode->push_back('3'); // MMMM (xor)
    activeSection->hexCode->push_back(hexValueD[0]); // AAAA
    activeSection->hexCode->push_back(hexValueD[0]); // BBBB
    activeSection->hexCode->push_back(hexValueS[0]); // CCCC
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in XOR: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerSHL(string* regS, string* regD){
  if(activeSection == nullptr){
    cout << "ERROR: SHL Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNumS = stoi((*regS).substr(1));
    string hexValueS = toHex(regNumS, false);
    int regNumD = stoi((*regD).substr(1));
    string hexValueD = toHex(regNumD, false);

    activeSection->hexCode->push_back('7'); // op code
    activeSection->hexCode->push_back('0'); // MMMM (shl)
    activeSection->hexCode->push_back(hexValueD[0]); // AAAA
    activeSection->hexCode->push_back(hexValueD[0]); // BBBB
    activeSection->hexCode->push_back(hexValueS[0]); // CCCC
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');

  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in SHL: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerSHR(string* regS, string* regD){
  if(activeSection == nullptr){
    cout << "ERROR: SHR Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNumS = stoi((*regS).substr(1));
    string hexValueS = toHex(regNumS, false);
    int regNumD = stoi((*regD).substr(1));
    string hexValueD = toHex(regNumD, false);

    activeSection->hexCode->push_back('7'); // op code
    activeSection->hexCode->push_back('1'); // MMMM (shr)
    activeSection->hexCode->push_back(hexValueD[0]); // AAAA
    activeSection->hexCode->push_back(hexValueD[0]); // BBBB
    activeSection->hexCode->push_back(hexValueS[0]); // CCCC
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in SHR: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerCSRRD(string* programReg, string* reg){
  if(activeSection == nullptr){
    cout << "ERROR: CSRRD Instruction is not in the section";
    exit(-1);
  }

  if(secondPass){
    int regNumProgram = 0;
    if((*programReg) == "handler" || (*programReg) == "HANDLER"){
      regNumProgram = 1;
    } else if ((*programReg) == "cause" || (*programReg) == "CAUSE"){
      regNumProgram = 2;
    }

    int regNum = stoi((*reg).substr(1));
    string hexValue = toHex(regNum, false);

    string hex = to_string(regNumProgram);

    activeSection->hexCode->push_back('9'); // op code
    activeSection->hexCode->push_back('0'); // MMMM (csrrd)
    activeSection->hexCode->push_back(hexValue[0]); // AAAA
    activeSection->hexCode->push_back(hex[0]); // BBBB
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');

  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in CSRRD: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerCSRWR(string* reg, string* programReg){
  if(activeSection == nullptr){
    cout << "ERROR: CSRWR Instruction is not in the section";
    exit(-1);
  }
  if(secondPass){
    int regNumProgram = 0;
    if((*programReg) == "handler" || (*programReg) == "HANDLER"){
      regNumProgram = 1;
    } else if ((*programReg) == "cause" || (*programReg) == "CAUSE"){
      regNumProgram = 2;
    }

    int regNum = stoi((*reg).substr(1));
    string hexValue = toHex(regNum, false);

    string hex = to_string(regNumProgram);

    activeSection->hexCode->push_back('9'); // op code
    activeSection->hexCode->push_back('4'); // MMMM (csrwr)
    activeSection->hexCode->push_back(hex[0]); // AAAA
    activeSection->hexCode->push_back(hexValue[0]); // BBBB
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    activeSection->hexCode->push_back('0');
    
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in CSRWR: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

// OPERAND instructions

void assemblerCALL(MultipleArguments* arguments){
  if(activeSection == nullptr){
    cout << "ERROR: CALL Instruction is not in the section";
    exit(-1);
  }
  int addressing = (*arguments).addressing;
  string *arg = (*(*arguments).argumentName)[0];

  if(!secondPass){
    if(addressing == 10){

      // LITERAL

      int base = 10;
      if((*arg).substr(0, 2) == "0x"){
        base = 16;
        (*arg) = (*arg).substr(2);
      } else if ((*arg)[0] == '0'){
        base = 8;
      }

      unsigned long val = stoul((*arg), nullptr, base);
      string hexArg = toHex(val, false);
      if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
      }
      
      // try to find the literal in the literalTable
      auto iter = activeSection->literalTable->begin();
      for(; iter != activeSection->literalTable->end(); iter++){
        if(iter->second->name == hexArg){
          break;
        }
      }
      if(iter == activeSection->literalTable->end()){
        // add LITERAL to the table of literals
        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        TableOfLiteralsEntry* newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, hexArg, false, 0, "");
        activeSection->literalTable->insert({hexArg, newEntry});
        activeSection->literalsInsertionOrder->push_back(hexArg);
      }
    }
  }

  if(secondPass){
    
    switch(addressing){
      case 10: {
        // argument is a LITERAL

        int base = 10;
        if((*arg).substr(0, 2) == "0x"){
          base = 16;
          (*arg) = (*arg).substr(2);
        } else if ((*arg)[0] == '0'){
          base = 8;
        }

        unsigned long val = stoul((*arg), nullptr, base);
        string hexArg = toHex(val, false);

        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        
        map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(hexArg);

        // offset to the pool of literals
        int id = iter2->second->index;
        int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4;
        string hexOffset = toHex(offset, true);


        // push pc; pc <= mem[pc + offset]
        activeSection->hexCode->push_back('2'); // op code
        activeSection->hexCode->push_back('1'); // MMMM
        activeSection->hexCode->push_back('F'); // AAAA (pc)
        activeSection->hexCode->push_back('0'); // BBBB
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }
        
        break;
      }
      case 11: {
        // argument is a SYMBOL
        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*arg);

        bool found = true;

        if(iter == symbolTable->end()){
          cout << "ERROR in CALL: Symbol " << *arg << " doesn't exist in the symbol table";
          exit(-1);
        }
        if(iter->second->section == activeSection->idSection){
          // SYMBOL is in the same section - we can use its value and put it in machine code
          // we don't need pool of literals/symbols here

          string hexOffset = toHex(iter->second->offset - locationCounter - pcOffset, true); //calculate offset to the symbol for pc relative

          // push pc; pc <= pc + offset
          activeSection->hexCode->push_back('2'); // op code
          activeSection->hexCode->push_back('0'); // MMMM
          activeSection->hexCode->push_back('F'); // AAAA (pc)
          activeSection->hexCode->push_back('0'); // BBBB
          if(hexOffset.length() == 0){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
          } else if(hexOffset.length() == 1){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
          } else if(hexOffset.length() == 2){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
          } else{
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
            activeSection->hexCode->push_back(hexOffset[2]);
          }


        }else {
          // SYMBOL is NOT in the same section
          // RELOKACIONA TABELA
          // try to find symbol in the table of ltierals
          map<string, TableOfLiteralsEntry*>::iterator litTbl = activeSection->literalTable->find(*arg);

          if(litTbl == activeSection->literalTable->end()){
            // if not found add it
            TableOfLiteralsEntry* newEntry;
            if(iter->second->isAbsolute){
              newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 1, iter->second->valueOfAbsolute);
            } else{
              newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 0, "");
              found = false;
            }
            activeSection->literalTable->insert({*arg, newEntry});
            activeSection->literalsInsertionOrder->push_back(*arg);

            
          }

          // take that symbol from the table
          map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(*arg);

          // offset to the pool of literals
          int id = iter2->second->index;
          int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4; // offset to the pool where symbol value will be
          string hexOffset = toHex(offset, true);

         
          // push pc; pc <= mem[pc + offset]
          activeSection->hexCode->push_back('2'); // op code
          activeSection->hexCode->push_back('1'); // MMMM
          activeSection->hexCode->push_back('F'); // AAAA (pc)
          activeSection->hexCode->push_back('0'); // BBBB
          if(hexOffset.length() == 0){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
          } else if(hexOffset.length() == 1){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
          } else if(hexOffset.length() == 2){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
          } else{
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
            activeSection->hexCode->push_back(hexOffset[2]);
          }
          

          if(!found){
            if(iter->second->type == 0){
              // LOCAL - defined in other section
              // u relokacionoj tabeli je offset locationCounter + pomeraj do bajta gde treba offset da se upise (u nasem slucaju offset do bazena literala u sekciji), 
              //vrednost je sekcija u kojoj se nalazi trazeni simbol a ugradjujemo (locationCounter simbola - pcOffset)

              int relocationTableOffset = activeSection->size + (id-1)*4;
              RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 1, iter->second->sectionName, iter->second->offset, activeSection->name);
              activeSection->relocationTable->push_back(newEntry);

            } else {
              // GLOBAL or EXTERN - defined in other section
              // u kod cemo ugraditi -pcOffset a u relokacionu tabelu cemo kao offset ugraditi bazen literala
              // vrednost je simbol koji se trazi
            
            int relocationTableOffset = activeSection->size + (id-1)*4;
            RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 2, iter->second->label, 0, activeSection->name);
            activeSection->relocationTable->push_back(newEntry);
            }
          }
          
        }
        break;
      }
    }
  }
 
  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in CALL: Section shouldn't be larger than 4096B";
      exit(-1);
  }
  
}

void assemblerLD(MultipleArguments* arguments, string* reg){
  
  if(activeSection == nullptr){
    cout << "ERROR: LD Instruction is not in the section";
    exit(-1);
  }
  int addressing = (*arguments).addressing;
  string* arg = (*(*arguments).argumentName)[0];


  if(!secondPass){
    if(addressing == 10 || addressing == 12){

      int base = 10;
      if((*arg).substr(0, 2) == "0x"){
        base = 16;
        (*arg) = (*arg).substr(2);
      } else if ((*arg)[0] == '0'){
        base = 8;
      }

      unsigned long val = stoul((*arg), nullptr, base);
      string hexArg = toHex(val, false);
      if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
      }
      
      // try to find the literal in the literalTable
      auto iter = activeSection->literalTable->begin();
      for(; iter != activeSection->literalTable->end(); iter++){
        if(iter->second->name == hexArg){
          break;
        }
      }
      if(iter == activeSection->literalTable->end()){
        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        TableOfLiteralsEntry* newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, hexArg, false, 0, "");
        activeSection->literalTable->insert({hexArg, newEntry});
        activeSection->literalsInsertionOrder->push_back(hexArg);

      }

    } 
    else if(addressing == 16){
      string* arg2 = (*(*arguments).argumentName)[1]; // literal
      int base = 10;
      if((*arg2).substr(0, 2) == "0x"){
        base = 16;
        (*arg2) = (*arg2).substr(2);
      } else if ((*arg2)[0] == '0'){
        base = 8;
      }

      unsigned long val = stoul((*arg2), nullptr, base);
      string hexArg = toHex(val, false);
      if(hexArg.length() > 3){
        cout << "ERROR in LD: literal length is more than 12b in mem[reg + literal] addressing: " << hexArg << endl;
        exit(-1);
      }
      // string* arg2 = (*(*arguments).argumentName)[1];

      // int base = 10;
      // if((*arg2).substr(0, 2) == "0x"){
      //   base = 16;
      //   (*arg2) = (*arg2).substr(2);
      // } else if ((*arg2)[0] == '0'){
      //   base = 8;
      // }

      // unsigned long val = stoul((*arg2), nullptr, base);
      // string hexArg = toHex(val, false);

      // // try to find the literal in the literalTable
      // auto iter = activeSection->literalTable->begin();
      // for(; iter != activeSection->literalTable->end(); iter++){
      //   if(iter->second->name == hexArg){
      //     break;
      //   }
      // }
      // if(iter == activeSection->literalTable->end()){
      //   if(hexArg.length() == 1){
      //     hexArg = "0" + hexArg;
      //   }
      //   TableOfLiteralsEntry* newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, hexArg, false);
      //   activeSection->literalTable->insert({hexArg, newEntry});
      //   activeSection->literalsInsertionOrder->push_back(hexArg);

      // }
    }
  }
  
  if(secondPass){

    
    int regNumD = stoi((*reg).substr(1));
    string hexValueD = toHex(regNumD, false);

    switch(addressing){
      case 10:{
        // immediate LITERAL
        
        
        int base = 10;
        if((*arg).substr(0, 2) == "0x"){
          base = 16;
          (*arg) = (*arg).substr(2);
        } else if ((*arg)[0] == '0'){
          base = 8;
        }

        unsigned long val = stoul((*arg), nullptr, base);
        string hexArg = toHex(val, false);

        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        
        map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(hexArg);
        
        int id = iter2->second->index;
        int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4; // offset to a literal
        string hexOffset = toHex(offset, true);


        // gprD <= mem[pc+offset] (literal)
        activeSection->hexCode->push_back('9'); // op code
        activeSection->hexCode->push_back('2'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back('F'); // BBBB (pc)
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }

        locationCounter += 4;
        
        break;
      }
      case 11:{
        // immediate SYMBOL

        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*arg);

        if(iter == symbolTable->end()){
          cout << "ERROR in LD: Symbol " << *arg << " doesn't exist in the symbol table: " << endl;
          exit(-1);
        }

        bool found = true;

        // try to find symbol in the table of ltierals
        map<string, TableOfLiteralsEntry*>::iterator litTbl = activeSection->literalTable->find(*arg);

        if(litTbl == activeSection->literalTable->end()){
          // if not found add it
          TableOfLiteralsEntry* newEntry;
          if(iter->second->isAbsolute){
            newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 1, iter->second->valueOfAbsolute);
          } else {
            newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 0, "");
            found = false;
          }
          
          activeSection->literalTable->insert({*arg, newEntry});
          activeSection->literalsInsertionOrder->push_back(*arg);

          
        }

        // take that symbol from the table
        map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(*arg);

        int id = iter2->second->index;
        int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4; // offset to the pool where symbol value will be
        string hexOffset = toHex(offset, true);

        // gprD <= mem[pc+offset] (symbol)
        activeSection->hexCode->push_back('9'); // op code
        activeSection->hexCode->push_back('2'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back('F'); // BBBB (pc)
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }


        if(!found){
          int relocationTableOffset = activeSection->size + (id-1)*4;

          if(iter->second->type == 0){
            // LOCAL
            RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 1, iter->second->sectionName, iter->second->offset, activeSection->name);
            activeSection->relocationTable->push_back(newEntry);
           
          }
          else{
            // GLOBAL / EXTERN (udnefined)
          
            RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 2, iter->second->label, 0, activeSection->name);
            activeSection->relocationTable->push_back(newEntry);
           
          }
        }

        locationCounter += 4;
        
        break;
      }
      case 12:{
        // mem[literal] mem direct literal

        int base = 10;
        if((*arg).substr(0, 2) == "0x"){
          base = 16;
          (*arg) = (*arg).substr(2);
        } else if ((*arg)[0] == '0'){
          base = 8;
        }

        unsigned long val = stoul((*arg), nullptr, base);
        string hexArg = toHex(val, false);

        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        
        
        map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(hexArg);

        int id = iter2->second->index;
        int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4;
        string hexOffset = toHex(offset, true);


        // gprD <= mem[pc+offset] (literal)
        activeSection->hexCode->push_back('9'); // op code
        activeSection->hexCode->push_back('2'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back('F'); // BBBB (pc)
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }


        // gprD<=mem[literal]
        activeSection->hexCode->push_back('9'); // op code
        activeSection->hexCode->push_back('2'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back(hexValueD[0]); // BBBB (literal)
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        

        locationCounter += 8;

        break;
        
      }
      case 13:{
        // mem[symbol] mem direct symbol
        
        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*arg);

        if(iter == symbolTable->end()){
          cout << "ERROR in LD: Symbol " << *arg << " doesn't exist in the symbol table";
          exit(-1);
        }

        bool found = true;

        // try to find symbol in the table of ltierals
        map<string, TableOfLiteralsEntry*>::iterator litTbl = activeSection->literalTable->find(*arg);

        if(litTbl == activeSection->literalTable->end()){
          // if not found add it
          TableOfLiteralsEntry* newEntry;
          if(iter->second->isAbsolute){
            newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 1, iter->second->valueOfAbsolute);
          } else {
            newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 0, "");
            found = false;
          }
        
          activeSection->literalTable->insert({*arg, newEntry});
          activeSection->literalsInsertionOrder->push_back(*arg);

        }

        // take that symbol from the table
        map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(*arg);

        int id = iter2->second->index;
        int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4; // offset to the pool where symbol value will be
        string hexOffset = toHex(offset, true);

        // gprD <= mem[pc+offset] (symbol)
        activeSection->hexCode->push_back('9'); // op code
        activeSection->hexCode->push_back('2'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back('F'); // BBBB (pc)
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }

        // gprD<=mem[symbol]
        activeSection->hexCode->push_back('9'); // op code
        activeSection->hexCode->push_back('2'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back(hexValueD[0]); // BBBB (literal)
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');


        if(!found){
          int relocationTableOffset = activeSection->size + (id-1)*4;

          if(iter->second->type == 0){
            // LOCAL
            
            RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 1, iter->second->sectionName, iter->second->offset, activeSection->name);
            activeSection->relocationTable->push_back(newEntry);
          }
          else{
            // GLOBAL / EXTERN (udnefined)

            RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 2, iter->second->label, 0, activeSection->name);
            activeSection->relocationTable->push_back(newEntry);
            
          }
        }

        locationCounter += 8;

        break;
      }
      case 14:{
        // register direct
        string* arg = (*(*arguments).argumentName)[0];

        int regNumS = stoi((*arg).substr(1));
        string hexValueS = toHex(regNumS, false);

        activeSection->hexCode->push_back('9'); // op code
        activeSection->hexCode->push_back('1'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back(hexValueS[0]); // BBBB (gprS)
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        

        locationCounter += 4;

        break;
      }
      case 15:{
        // mem[reg] register indirect
        string* arg = (*(*arguments).argumentName)[0];

        int regNumS = stoi((*arg).substr(1));
        string hexValueS = toHex(regNumS, false);

        activeSection->hexCode->push_back('9'); // op code
        activeSection->hexCode->push_back('2'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back(hexValueS[0]); // BBBB (gprS)
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        

        locationCounter += 4;

        break;
      }
      case 16:{
        // mem[reg + literal] register direct with literal offset
        string* arg1 = (*(*arguments).argumentName)[0]; // register
        string* arg2 = (*(*arguments).argumentName)[1]; // literal


        int base = 10;
        if((*arg2).substr(0, 2) == "0x"){
          base = 16;
          (*arg2) = (*arg2).substr(2);
        } else if ((*arg2)[0] == '0'){
          base = 8;
        }

        unsigned long val = stoul((*arg2), nullptr, base);
        string hexOffset = toHex(val, false);


        int regNumS = stoi((*arg1).substr(1));
        string hexValueS = toHex(regNumS, false);
        

        // regD <= mem[regS+literal]
        activeSection->hexCode->push_back('9'); // op code
        activeSection->hexCode->push_back('2'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back(hexValueS[0]); // BBBB (gprS)
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }
        

        locationCounter += 4;

        break;
      }
      case 17:{
        // mem[reg + symbol] register direct with offset
        string* arg1 = (*(*arguments).argumentName)[0]; // register
        string* arg2 = (*(*arguments).argumentName)[1]; // symbol

        int regNumS = stoi((*arg1).substr(1));
        string hexValueS = toHex(regNumS, false);

        
        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*arg2);

        if(iter == symbolTable->end()){
          cout << "ERROR in LD: Symbol " << *arg << " doesn't exist in the symbol table" << endl;
          exit(-1);
        }
        if(!iter->second->isAbsolute || iter->second->valueOfAbsolute.length() > 3){ // add value of the symbol > 3
          cout << "ERROR: LD symbol " << *arg << " is not absolute or length of the symbol is more than 12b" << endl;
          exit(-1);
        }

        string hexOffset = iter->second->valueOfAbsolute;
        
        // regD <= mem[regS+symbol]
        activeSection->hexCode->push_back('9'); // op code
        activeSection->hexCode->push_back('2'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back('F'); // pc
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }
        

        locationCounter += 4;
        
        break;
      } 
    }
  }
  if(!secondPass){
    if(addressing == 12 || addressing == 13){
      locationCounter += 8;
    }else {
        locationCounter += 4;
    }
  }
  
  if(locationCounter > 4096){
      cout << "ERROR in LD: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerST(string* reg, MultipleArguments* arguments){
  if(activeSection == nullptr){
    cout << "ERROR: ST Instruction is not in the section";
    exit(-1);
  }
  int addressing = (*arguments).addressing;
  string* arg = (*(*arguments).argumentName)[0];


  if(!secondPass){
    if(addressing == 10 || addressing == 12){

      int base = 10;
      if((*arg).substr(0, 2) == "0x"){
        base = 16;
        (*arg) = (*arg).substr(2);
      } else if ((*arg)[0] == '0'){
        base = 8;
      }

      unsigned long val = stoul((*arg), nullptr, base);
      string hexArg = toHex(val, false);
      if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
      }
      
      // try to find the literal in the literalTable
      auto iter = activeSection->literalTable->begin();
      for(; iter != activeSection->literalTable->end(); iter++){
        if(iter->second->name == hexArg){
          break;
        }
      }
      if(iter == activeSection->literalTable->end()){
        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        TableOfLiteralsEntry* newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, hexArg, false, 0, "");
        activeSection->literalTable->insert({hexArg, newEntry});
        activeSection->literalsInsertionOrder->push_back(hexArg);
      }

    } else if(addressing == 16){
      string* arg2 = (*(*arguments).argumentName)[1];

      int base = 10;
      if((*arg2).substr(0, 2) == "0x"){
        base = 16;
        (*arg2) = (*arg2).substr(2);
      } else if ((*arg2)[0] == '0'){
        base = 8;
      }

      unsigned long val = stoul((*arg2), nullptr, base);
      string hexArg = toHex(val, false);
      
      if(hexArg.length() > 3){
        cout << "ERROR in ST: Literal value is more than 12b: " << hexArg << endl;
        exit(-1);
      }
    }

  }
  if(secondPass){

    int regNumS = stoi((*reg).substr(1));
    string hexValueS = toHex(regNumS, false);

    switch(addressing){
      case 10:{
        // immediate LITERAL

        cout << "ERROR in ST: ST can't work with immediate literals";
        exit(-1);
        
        break;
      }
      case 11:{
        // immediate SYMBOL

        cout << "ERROR in ST: ST can't work with immediate symbols";
        exit(-1);

        break;
      }
      case 12:{
        // mem[literal] mem direct literal

        int base = 10;
        if((*arg).substr(0, 2) == "0x"){
          base = 16;
          (*arg) = (*arg).substr(2);
        } else if ((*arg)[0] == '0'){
          base = 8;
        }

        unsigned long val = stoul((*arg), nullptr, base);
        string hexArg = toHex(val, false);

        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        
        
        map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(hexArg);

        int id = iter2->second->index;
        int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4;
        string hexOffset = toHex(offset, true);

        // mem[mem[pc+offset]] <= gprS
        activeSection->hexCode->push_back('8'); // op code
        activeSection->hexCode->push_back('2'); // MMMM
        activeSection->hexCode->push_back('F'); // AAAA (pc)
        activeSection->hexCode->push_back('0'); // BBBB
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }
        
        break;
        
      }
      case 13:{
        // mem[symbol] mem direct symbol

        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*arg);

        if(iter == symbolTable->end()){
          cout << "ERROR in ST: Symbol " << *arg << " doesn't exist in the symbol table";
          exit(-1);
        }

        bool found = true;

        // try to find symbol in the table of ltierals
        map<string, TableOfLiteralsEntry*>::iterator litTbl = activeSection->literalTable->find(*arg);

        if(litTbl == activeSection->literalTable->end()){
          // if not found add it
          TableOfLiteralsEntry* newEntry;
          if(iter->second->isAbsolute){
            newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 1, iter->second->valueOfAbsolute);
          } else {
            newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 0, "");
            found = false;
          }
          
          activeSection->literalTable->insert({*arg, newEntry});
          activeSection->literalsInsertionOrder->push_back(*arg);

        }

        // take that symbol from the table
        map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(*arg);

        int id = iter2->second->index;
        int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4; // offset to the pool where symbol value will be
        string hexOffset = toHex(offset, true);

        // mem[mem[pc+offset]] <= gprS
        activeSection->hexCode->push_back('8'); // op code
        activeSection->hexCode->push_back('2'); // MMMM
        activeSection->hexCode->push_back('F'); // AAAA (pc)
        activeSection->hexCode->push_back('0'); // BBBB
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }
      

        if(!found){
          int relocationTableOffset = activeSection->size + (id-1)*4;

          if(iter->second->type == 0){
            // LOCAL
            
            RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 1, iter->second->sectionName, iter->second->offset, activeSection->name);
            activeSection->relocationTable->push_back(newEntry);
          }
          else{
            // GLOBAL / EXTERN (udnefined)
            
            RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 2, iter->second->label, 0, activeSection->name);
            activeSection->relocationTable->push_back(newEntry);
          }
        }

        
        break;
      }
      case 14:{
        // register direct
        string* arg = (*(*arguments).argumentName)[0];

        int regNumD = stoi((*arg).substr(1));
        string hexValueD = toHex(regNumD, false);

        activeSection->hexCode->push_back('9'); // op code
        activeSection->hexCode->push_back('1'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back(hexValueS[0]); // BBBB (gprS)
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        

        break;
      }
      case 15:{
        // mem[reg] register indirect
        string* arg = (*(*arguments).argumentName)[0];

        int regNumD = stoi((*arg).substr(1));
        string hexValueD = toHex(regNumD, false);

        // mem[gprD] <= gprS
        activeSection->hexCode->push_back('8'); // op code
        activeSection->hexCode->push_back('0'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (gprD)
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back(hexValueS[0]); // CCCC (gprS)
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        activeSection->hexCode->push_back('0');
        

        break;
      }
      case 16:{
        // mem[reg + literal] register direct with literal offset

        string* arg1 = (*(*arguments).argumentName)[0]; // register
        string* arg2 = (*(*arguments).argumentName)[1]; // literal

        int regNumD = stoi((*arg1).substr(1));
        string hexValueD = toHex(regNumD, false);

        int base = 10;
        if((*arg2).substr(0, 2) == "0x"){
          base = 16;
          (*arg2) = (*arg2).substr(2);
        } else if ((*arg2)[0] == '0'){
          base = 8;
        }

        unsigned long val = stoul((*arg2), nullptr, base);
        string hexOffset = toHex(val, false);

        // mem[regS + literal] <= regS
        activeSection->hexCode->push_back('8'); // op code
        activeSection->hexCode->push_back('0'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (regD)
        activeSection->hexCode->push_back('0'); // BBBB
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }
        

        break;
      }
      case 17:{
        // mem[reg + symbol] register direct with offset
        string* arg1 = (*(*arguments).argumentName)[0]; // register
        string* arg2 = (*(*arguments).argumentName)[1]; // symbol

        int regNumD = stoi((*arg1).substr(1));
        string hexValueD = toHex(regNumD, false);

        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*arg2);

        if(iter == symbolTable->end()){
          cout << "ERROR in ST: Symbol " << *arg2 << " doesn't exist in the symbol table" << endl;
          exit(-1);
        }
        if(!iter->second->isAbsolute || iter->second->valueOfAbsolute.length()>3){ // or length of the symbol is not less than 2
          cout << "ERROR in ST: Symbol is not absolute or symbol value is more than 12b: " << *arg2 << endl;;
          exit(-1);
        }

        string hexOffset = iter->second->valueOfAbsolute;

        // mem[regS + symbol] <= regS
        activeSection->hexCode->push_back('8'); // op code
        activeSection->hexCode->push_back('0'); // MMMM
        activeSection->hexCode->push_back(hexValueD[0]); // AAAA (regD)
        activeSection->hexCode->push_back('0'); // BBBB
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back(hexValueS[0]);
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }
        

        locationCounter += 4;
        
        break;
      } 
    }
  }

  locationCounter += 4;

  if(locationCounter > 4096){
      cout << "ERROR in ST: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerJMP(MultipleArguments* arguments){
  if(activeSection == nullptr){
    cout << "ERROR: JMP Instruction is not in the section";
    exit(-1);
  }
  int addressing = (*arguments).addressing;
  string *arg = (*(*arguments).argumentName)[0];

  if(!secondPass){
    if(addressing == 10){
      int base = 10;
      if((*arg).substr(0, 2) == "0x"){
        base = 16;
        (*arg) = (*arg).substr(2);
      } else if ((*arg)[0] == '0'){
        base = 8;
      }

      unsigned long val = stoul((*arg), nullptr, base);
      string hexArg = toHex(val, false);
      if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
      }
      
      // try to find the literal in the literalTable
      auto iter = activeSection->literalTable->begin();
      for(; iter != activeSection->literalTable->end(); iter++){
        if(iter->second->name == hexArg){
          break;
        }
      }
      if(iter == activeSection->literalTable->end()){
        // add LITERAL to the table of literals
        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        TableOfLiteralsEntry* newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, hexArg, false, 0, "");
        activeSection->literalTable->insert({hexArg, newEntry});
        activeSection->literalsInsertionOrder->push_back(hexArg);
      }
    }
  }

  if(secondPass){
    
    
    switch(addressing){
      case 10: {
        // argument is a LITERAL

        int base = 10;
        if((*arg).substr(0, 2) == "0x"){
          base = 16;
          (*arg) = (*arg).substr(2);
        } else if ((*arg)[0] == '0'){
          base = 8;
        }

        unsigned long val = stoul((*arg), nullptr, base);
        string hexArg = toHex(val, false);

        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        
        
        map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(hexArg);

        int id = iter2->second->index;
        int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4;
        string hexOffset = toHex(offset, true);

        // pc <= mem[pc+offset] (literal)
        activeSection->hexCode->push_back('3'); // op code
        activeSection->hexCode->push_back('8'); // MMMM
        activeSection->hexCode->push_back('F'); // AAAA (pc)
        activeSection->hexCode->push_back('0'); // BBBB
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }
       
        
        break;
      }
      case 11: {
        // argument is SYMBOL

        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*arg);

        if(iter == symbolTable->end()){
          cout << "ERROR in JMP: Symbol " << *arg << " doesn't exist in the symbol table";
          exit(-1);
        }

        bool found = true;

        if(iter->second->section == activeSection->idSection){
          // SYMBOL is in the same section - we can use its value and put it in machine code
          // we don't need pool of literals/symbols here

          string hexOffset = toHex(iter->second->offset - locationCounter - pcOffset, true); //calculate offset to the symbol for pc relative
          
          // pc <= pc+offset (symbol)
          activeSection->hexCode->push_back('3'); // op code
          activeSection->hexCode->push_back('0'); // MMMM
          activeSection->hexCode->push_back('F'); // AAAA (pc)
          activeSection->hexCode->push_back('0');
          if(hexOffset.length() == 0){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
          } else if(hexOffset.length() == 1){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
          } else if(hexOffset.length() == 2){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
          } else{
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
            activeSection->hexCode->push_back(hexOffset[2]);
          }
          

        }else {
          // SYMBOL is NOT in the same section
        

          // try to find symbol in the table of ltierals
          map<string, TableOfLiteralsEntry*>::iterator litTbl = activeSection->literalTable->find(*arg);

          if(litTbl == activeSection->literalTable->end()){
            // if not found add it
            TableOfLiteralsEntry* newEntry;
            if(iter->second->isAbsolute){
              newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 1, iter->second->valueOfAbsolute);
            } else {
              newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 0, "");
              found = false;
            }
        
            activeSection->literalTable->insert({*arg, newEntry});
            activeSection->literalsInsertionOrder->push_back(*arg);

           
          }

          // take that symbol from the table
          map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(*arg);

          int id = iter2->second->index;
          int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4; // offset to the pool where symbol value will be
          string hexOffset = toHex(offset, true);

          // pc <= mem[pc+offset] (symbol)
          activeSection->hexCode->push_back('3'); // op code
          activeSection->hexCode->push_back('8'); // MMMM
          activeSection->hexCode->push_back('F'); // AAAA (pc)
          activeSection->hexCode->push_back('0'); // BBBB
          if(hexOffset.length() == 0){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
          } else if(hexOffset.length() == 1){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
          } else if(hexOffset.length() == 2){
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
          } else{
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
            activeSection->hexCode->push_back(hexOffset[2]);
          }

          if(!found){
            if(iter->second->type == 0){
              // LOCAL - defined in other section
              
              int relocationTableOffset = activeSection->size + (id-1)*4;
              RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 1, iter->second->sectionName, iter->second->offset, activeSection->name);
              activeSection->relocationTable->push_back(newEntry);

            } else {
              // GLOBAL or EXTERN - defined in other section
             
              int relocationTableOffset = activeSection->size + (id-1)*4;
              RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 2, iter->second->label, 0, activeSection->name);
              activeSection->relocationTable->push_back(newEntry);
            }
          }
        }
        break;
      }
    }
  }
  
  locationCounter += 4;
  

  if(locationCounter > 4096){
      cout << "ERROR in JMP: Section shouldn't be larger than 4096B";
      exit(-1);
  }

}

void assemblerBEQ(string* reg1, string* reg2, MultipleArguments* arguments){
  if(activeSection == nullptr){
    cout << "ERROR: BEQ Instruction is not in the section";
    exit(-1);
  }
  int addressing = (*arguments).addressing;
  string *arg = (*(*arguments).argumentName)[0];

  if(!secondPass){
    if(addressing == 10){
      int base = 10;
      if((*arg).substr(0, 2) == "0x"){
        base = 16;
        (*arg) = (*arg).substr(2);
      } else if ((*arg)[0] == '0'){
        base = 8;
      }

      unsigned long val = stoul((*arg), nullptr, base);
      string hexArg = toHex(val, false);
      if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
      }
      
      // try to find the literal in the literalTable
      auto iter = activeSection->literalTable->begin();
      for(; iter != activeSection->literalTable->end(); iter++){
        if(iter->second->name == hexArg){
          break;
        }
      }
      if(iter == activeSection->literalTable->end()){
        // add LITERAL to the table of literals
        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        TableOfLiteralsEntry* newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, hexArg, false, 0, "");
        activeSection->literalTable->insert({hexArg, newEntry});
        activeSection->literalsInsertionOrder->push_back(hexArg);
      }
    }
  }

  if(secondPass){

    int numGPR1 = stoi((*reg1).substr(1));
    string hexGPR1 = toHex(numGPR1, false);
    
    int numGPR2 = stoi((*reg2).substr(1));
    string hexGPR2 = toHex(numGPR2, false);

    
    switch(addressing){
      case 10: {
        // argument is a LITERAL

        int base = 10;
        if((*arg).substr(0, 2) == "0x"){
          base = 16;
          (*arg) = (*arg).substr(2);
        } else if ((*arg)[0] == '0'){
          base = 8;
        }

        unsigned long val = stoul((*arg), nullptr, base);
        string hexArg = toHex(val, false);
        
        
        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
          
        
        map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(hexArg);

        int id = iter2->second->index;
        int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4;
        string hexOffset = toHex(offset, true);


        // if(gpr1 == gpr2) pc <= mem[pc + offset] (literal)
        activeSection->hexCode->push_back('3'); // op code
        activeSection->hexCode->push_back('9'); // MMMM
        activeSection->hexCode->push_back('F'); // AAAA (pc)
        activeSection->hexCode->push_back(hexGPR1[0]); // BBBB (gpr1)
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }


        locationCounter += 4;
        
        break;
      }
      case 11: {
        // argument is a SYMBOL
        
        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*arg);
        // we ony work with symbols that are not absolute

        if(iter == symbolTable->end()){
          cout << "ERROR in BEQ: Symbol " << *arg << " doesn't exist in the symbol table";
          exit(-1);
        }

        bool found = true;

        if(iter->second->section == activeSection->idSection){
          // SYMBOL is in the same section - we can use its value and put it in machine code
          // we don't need pool of literals/symbols here
          string hexOffset = toHex(iter->second->offset - locationCounter - pcOffset, true);
          
          // if(gpr1 == gpr2) pc <= pc + offset (symbol)
          activeSection->hexCode->push_back('3'); // op code
          activeSection->hexCode->push_back('1'); // MMMM (beq)
          activeSection->hexCode->push_back('F'); // AAAA (pc)
          activeSection->hexCode->push_back(hexGPR1[0]); // BBBB (gpr1)
          if(hexOffset.length() == 0){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
          } else if(hexOffset.length() == 1){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
          } else if(hexOffset.length() == 2){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
          } else{
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
            activeSection->hexCode->push_back(hexOffset[2]);
          }
          
          locationCounter += 4;

        }else {
          // SYMBOL is NOT in the same section
          // RELOKACIONA TABELA

          // try to find symbol in the table of ltierals
          map<string, TableOfLiteralsEntry*>::iterator litTbl = activeSection->literalTable->find(*arg);

          if(litTbl == activeSection->literalTable->end()){
            // if not found add it
            TableOfLiteralsEntry* newEntry;
            if(iter->second->isAbsolute){
              newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 1, iter->second->valueOfAbsolute);
            } else {
              newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 0, "");
              found = false;
            }
        
            activeSection->literalTable->insert({*arg, newEntry});
            activeSection->literalsInsertionOrder->push_back(*arg);

          }

          // take that symbol from the table
          map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(*arg);

          int id = iter2->second->index;
          int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4; // offset to the pool where symbol value will be
          string hexOffset = toHex(offset, true);

          // if(gpr1 == gpr2) pc <= mem[pc + offset] (symbol)
          activeSection->hexCode->push_back('3'); // op code
          activeSection->hexCode->push_back('9'); // MMMM
          activeSection->hexCode->push_back('F'); // AAAA (pc)
          activeSection->hexCode->push_back(hexGPR1[0]); // BBBB (gpr1)
          if(hexOffset.length() == 0){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
          } else if(hexOffset.length() == 1){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
          } else if(hexOffset.length() == 2){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
          } else{
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
            activeSection->hexCode->push_back(hexOffset[2]);
          }


          locationCounter += 4;

          if(!found){
            if(iter->second->type == 0){
              // LOCAL - defined in other section
              // u relokacionoj tabeli je offset locationCounter + pomeraj do bajta gde treba offset da se upise (u nasem slucaju offset do bazena literala u sekciji), 
              //vrednost je sekcija u kojoj se nalazi trazeni simbol a ugradjujemo (locationCounter simbola - pcOffset)

              int relocationTableOffset = activeSection->size + (id-1)*4;
              RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 1, iter->second->sectionName, iter->second->offset, activeSection->name);
              activeSection->relocationTable->push_back(newEntry);

            } else {
              // GLOBAL or EXTERN - defined in other section
              // u kod cemo ugraditi -pcOffset a u relokacionu tabelu cemo kao offset ugraditi bazen literala
              // vrednost je simbol koji se trazi
              
              int relocationTableOffset = activeSection->size + (id-1)*4;
              RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 2, iter->second->label, 0, activeSection->name);
              activeSection->relocationTable->push_back(newEntry);
            }
          }
        }
        break;
      }
    }
  }

  if(!secondPass){
    locationCounter += 4;
  }
  
  if(locationCounter > 4096){
      cout << "ERROR in BEQ: Section shouldn't be larger than 4096B";
      exit(-1);
  }

}

void assemblerBNE(string* reg1, string* reg2, MultipleArguments* arguments){
  if(activeSection == nullptr){
    cout << "ERROR: BNE Instruction is not in the section";
    exit(-1);
  }
  int addressing = (*arguments).addressing;
  string *arg = (*(*arguments).argumentName)[0];

  if(!secondPass){
    if(addressing == 10){
      int base = 10;
      if((*arg).substr(0, 2) == "0x"){
        base = 16;
        (*arg) = (*arg).substr(2);
      } else if ((*arg)[0] == '0'){
        base = 8;
      }

      unsigned long val = stoul((*arg), nullptr, base);
      string hexArg = toHex(val, false);
      if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
      }
      
      // try to find the literal in the literalTable
      auto iter = activeSection->literalTable->begin();
      for(; iter != activeSection->literalTable->end(); iter++){
        if(iter->second->name == hexArg){
          break;
        }
      }
      if(iter == activeSection->literalTable->end()){
        // add LITERAL to the table of literals
        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        TableOfLiteralsEntry* newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, hexArg, false, 0, "");
        activeSection->literalTable->insert({hexArg, newEntry});
        activeSection->literalsInsertionOrder->push_back(hexArg);
      }
    }
  }
  
  if(secondPass){

    int numGPR1 = stoi((*reg1).substr(1));
    string hexGPR1 = toHex(numGPR1, false);
    
    int numGPR2 = stoi((*reg2).substr(1));
    string hexGPR2 = toHex(numGPR2, false);

    
    switch(addressing){
      case 10: {
        // argument is a LITERAL

        int base = 10;
        if((*arg).substr(0, 2) == "0x"){
          base = 16;
          (*arg) = (*arg).substr(2);
        } else if ((*arg)[0] == '0'){
          base = 8;
        }

        unsigned long val = stoul((*arg), nullptr, base);
        string hexArg = toHex(val, false);
        
        
        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
          
        
        map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(hexArg);


        int id = iter2->second->index;
        int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4;
        string hexOffset = toHex(offset, true);

        // if(gpr1 != gpr2) pc <= mem[pc + offset] (literal)
        activeSection->hexCode->push_back('3'); // op code
        activeSection->hexCode->push_back('A'); // MMMM
        activeSection->hexCode->push_back('F'); // AAAA (pc)
        activeSection->hexCode->push_back(hexGPR1[0]); // BBBB (gpr1)
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }

        locationCounter += 4;
        
        break;
      }
      case 11: {
        // argument is a SYMBOL
        
        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*arg);
        // we ony work with symbols that are not absolute

        if(iter == symbolTable->end()){
          cout << "ERROR in BNE: Symbol " << *arg << " doesn't exist in the symbol table";
          exit(-1);
        }

        bool found = true;

        if(iter->second->section == activeSection->idSection){
          // SYMBOL is in the same section - we can use its value and put it in machine code
          // we don't need pool of literals/symbols here
          string hexOffset = toHex(iter->second->offset - locationCounter - pcOffset, true);
          
          // if(gpr1 != gpr2) pc <= pc + offset (symbol)
          activeSection->hexCode->push_back('3'); // op code
          activeSection->hexCode->push_back('2'); // MMMM (bne)
          activeSection->hexCode->push_back('F'); // AAAA (pc)
          activeSection->hexCode->push_back(hexGPR1[0]); // BBBB (gpr1)
          if(hexOffset.length() == 0){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
          } else if(hexOffset.length() == 1){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
          } else if(hexOffset.length() == 2){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
          } else{
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
            activeSection->hexCode->push_back(hexOffset[2]);  
          }
          
          locationCounter += 4;

        }else {
          // SYMBOL is NOT in the same section
          // RELOKACIONA TABELA

          // try to find symbol in the table of ltierals
          map<string, TableOfLiteralsEntry*>::iterator litTbl = activeSection->literalTable->find(*arg);

          if(litTbl == activeSection->literalTable->end()){
            // if not found add it
            TableOfLiteralsEntry* newEntry;
            if(iter->second->isAbsolute){
              newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 1, iter->second->valueOfAbsolute);
            } else {
              newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 0, "");
              found = false;
            }
      
            activeSection->literalTable->insert({*arg, newEntry});
            activeSection->literalsInsertionOrder->push_back(*arg);

          }

          // take that symbol from the table
          map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(*arg);

          int id = iter2->second->index;
          int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4; // offset to the pool where symbol value will be
          string hexOffset = toHex(offset, true);

          // if(gpr1 != gpr2) pc <= mem[pc + offset] (symbol)
          activeSection->hexCode->push_back('3'); // op code
          activeSection->hexCode->push_back('A'); // MMMM
          activeSection->hexCode->push_back('F'); // AAAA (pc)
          activeSection->hexCode->push_back(hexGPR1[0]); // BBBB (gpr1)
          if(hexOffset.length() == 0){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
          } else if(hexOffset.length() == 1){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
          } else if(hexOffset.length() == 2){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
          } else{
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
            activeSection->hexCode->push_back(hexOffset[2]);
          }


          locationCounter += 4;

          if(!found){
            if(iter->second->type == 0){
              // LOCAL - defined in other section
              // u relokacionoj tabeli je offset locationCounter + pomeraj do bajta gde treba offset da se upise (u nasem slucaju offset do bazena literala u sekciji), 
              //vrednost je sekcija u kojoj se nalazi trazeni simbol a ugradjujemo (locationCounter simbola - pcOffset)

              int relocationTableOffset = activeSection->size + (id-1)*4;
              RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 1, iter->second->sectionName, iter->second->offset, activeSection->name);
              activeSection->relocationTable->push_back(newEntry);

            } else {
              // GLOBAL or EXTERN - defined in other section
              // u kod cemo ugraditi -pcOffset a u relokacionu tabelu cemo kao offset ugraditi bazen literala
              // vrednost je simbol koji se trazi
              
              int relocationTableOffset = activeSection->size + (id-1)*4;
              RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 2, iter->second->label, 0, activeSection->name);
              activeSection->relocationTable->push_back(newEntry);
            }
          }
        }
        break;
      }
    }
  }
  
  if(!secondPass){
    locationCounter += 4;
  }

  if(locationCounter > 4096){
      cout << "ERROR in BNE: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}

void assemblerBGT(string* reg1, string* reg2, MultipleArguments* arguments){
  if(activeSection == nullptr){
    cout << "ERROR: BGT Instruction is not in the section";
    exit(-1);
  }
  int addressing = (*arguments).addressing;
  string *arg = (*(*arguments).argumentName)[0];

  if(!secondPass){
    if(addressing == 10){
      int base = 10;
      if((*arg).substr(0, 2) == "0x"){
        base = 16;
        (*arg) = (*arg).substr(2);
      } else if ((*arg)[0] == '0'){
        base = 8;
      }

      unsigned long val = stoul((*arg), nullptr, base);
      string hexArg = toHex(val, false);

      if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
      }
      
      // try to find the literal in the literalTable
      auto iter = activeSection->literalTable->begin();
      for(; iter != activeSection->literalTable->end(); iter++){
        if(iter->second->name == hexArg){
          break;
        }
      }
      if(iter == activeSection->literalTable->end()){
        // add LITERAL to the table of literals
        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        TableOfLiteralsEntry* newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, hexArg, false, 0, "");
        activeSection->literalTable->insert({hexArg, newEntry});
        activeSection->literalsInsertionOrder->push_back(hexArg);
      }
    }
  }

  if(secondPass){

    int numGPR1 = stoi((*reg1).substr(1));
    string hexGPR1 = toHex(numGPR1, false);
    
    int numGPR2 = stoi((*reg2).substr(1));
    string hexGPR2 = toHex(numGPR2, false);

    
    switch(addressing){
      case 10: {
        // argument is a LITERAL

        int base = 10;
        if((*arg).substr(0, 2) == "0x"){
          base = 16;
          (*arg) = (*arg).substr(2);
        } else if ((*arg)[0] == '0'){
          base = 8;
        }

        unsigned long val = stoul((*arg), nullptr, base);
        string hexArg = toHex(val, false);
        
        
        if(hexArg.length() == 1){
          hexArg = "0" + hexArg;
        }
        
        map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(hexArg);

        int id = iter2->second->index;
        int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4;
        string hexOffset = toHex(offset, true);

        // if(gpr1 signed> gpr2) pc <= mem[pc + offset] (literal)

        activeSection->hexCode->push_back('3'); // op code
        activeSection->hexCode->push_back('B'); // MMMM
        activeSection->hexCode->push_back('F'); // AAAA (pc)
        activeSection->hexCode->push_back(hexGPR1[0]); // BBBB (gpr1)
        if(hexOffset.length() == 0){
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
        } else if(hexOffset.length() == 1){
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
        } else if(hexOffset.length() == 2){
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back('0');
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
        } else{
          activeSection->hexCode->push_back(hexGPR2[0]);
          activeSection->hexCode->push_back(hexOffset[0]);
          activeSection->hexCode->push_back(hexOffset[1]);
          activeSection->hexCode->push_back(hexOffset[2]);
        }
       

        locationCounter += 4;
        
        break;
      }
      case 11: {
        // argument is a SYMBOL
        
        map<string, SymbolTableEntry*>::iterator iter = symbolTable->find(*arg);

        if(iter == symbolTable->end()){
          cout << "ERROR in BGT: Symbol " << *arg << " doesn't exist in the symbol table";
          exit(-1);
        }

        bool found = true;

        if(iter->second->section == activeSection->idSection){
          // SYMBOL is in the same section - we can use its value and put it in machine code
          // we don't need pool of literals/symbols here
          string hexOffset = toHex(iter->second->offset - locationCounter - pcOffset, true);
          
          // if(gpr1 signed> gpr2) pc <= pc + offset
          activeSection->hexCode->push_back('3'); // op code
          activeSection->hexCode->push_back('3'); // MMMM
          activeSection->hexCode->push_back('F'); // AAAA (pc)
          activeSection->hexCode->push_back(hexGPR1[0]); // BBBB (gpr1)
          if(hexOffset.length() == 0){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
          } else if(hexOffset.length() == 1){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
          } else if(hexOffset.length() == 2){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
          } else{
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
            activeSection->hexCode->push_back(hexOffset[2]);
          }
          

          locationCounter += 4;


        }else {
          // SYMBOL is NOT in the same section
          // RELOKACIONA TABELA

          // try to find symbol in the table of ltierals
          map<string, TableOfLiteralsEntry*>::iterator litTbl = activeSection->literalTable->find(*arg);

          if(litTbl == activeSection->literalTable->end()){
            // if not found add it
            TableOfLiteralsEntry* newEntry;
            if(iter->second->isAbsolute){
              newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 1, iter->second->valueOfAbsolute);
            } else {
              newEntry = new TableOfLiteralsEntry(activeSection->literalTableIndexCNT++, *arg, true, 0, "");
              found = false;
            }
            
            activeSection->literalTable->insert({*arg, newEntry});
            activeSection->literalsInsertionOrder->push_back(*arg);

          }

          // take that symbol from the table
          map<string, TableOfLiteralsEntry*>::iterator iter2 = activeSection->literalTable->find(*arg);


          int id = iter2->second->index;
          int offset = activeSection->size - (locationCounter + pcOffset) + (id-1)*4; // offset to the pool where symbol value will be
          string hexOffset = toHex(offset, true);

          // if(gpr1 signed> gpr2) pc <= mem[pc + offset] (literal)
          activeSection->hexCode->push_back('3'); // op code
          activeSection->hexCode->push_back('B'); // MMMM
          activeSection->hexCode->push_back('F'); // AAAA (pc)
          activeSection->hexCode->push_back(hexGPR1[0]); // BBBB (gpr1)
          if(hexOffset.length() == 0){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
          } else if(hexOffset.length() == 1){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
          } else if(hexOffset.length() == 2){
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back('0');
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
          } else{
            activeSection->hexCode->push_back(hexGPR2[0]);
            activeSection->hexCode->push_back(hexOffset[0]);
            activeSection->hexCode->push_back(hexOffset[1]);
            activeSection->hexCode->push_back(hexOffset[2]);
          }

          
          locationCounter += 4;


          if(!found){
            if(iter->second->type == 0){
              // LOCAL - defined in other section
              // u relokacionoj tabeli je offset locationCounter + pomeraj do bajta gde treba offset da se upise (u nasem slucaju offset do bazena literala u sekciji), 
              //vrednost je sekcija u kojoj se nalazi trazeni simbol a ugradjujemo (locationCounter simbola - pcOffset)

              int relocationTableOffset = activeSection->size + (id-1)*4;
              RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 1, iter->second->sectionName, iter->second->offset, activeSection->name);
              activeSection->relocationTable->push_back(newEntry);

            } else {
              // GLOBAL or EXTERN - defined in other section
              // u kod cemo ugraditi -pcOffset a u relokacionu tabelu cemo kao offset ugraditi bazen literala
              // vrednost je simbol koji se trazi

              int relocationTableOffset = activeSection->size + (id-1)*4;
              RelocationTableEntrance *newEntry = new RelocationTableEntrance(relocationTableOffset, 2, iter->second->label, 0, activeSection->name);
              activeSection->relocationTable->push_back(newEntry);

            }
          }
        }
        break;
      }
    }
  }

  if(!secondPass){
    locationCounter += 4;
  }

  if(locationCounter > 4096){
      cout << "ERROR in BGT: Section shouldn't be larger than 4096B";
      exit(-1);
  }
}


void printSymbolTable(){
  for(auto iter = symbolTable->begin(); iter != symbolTable->end(); iter++){
    iter->second->printSymbolTableEntrance();
  }
}

void printOutputTextFile(){
  ofstream outputTextFile;
  outputTextFile.open(outputFileName + ".txt");

  if(outputTextFile.is_open()){

    // print symbol table
    outputTextFile << "#.symtab" << endl;
    outputTextFile << "Num\t" << "Value\t\t" << "Type\t" << "Bind\t" << "Ndx\t\t" << "Name" << endl;
    string helpType;
    string helpBind;
    string helpNdx;
    string helpLabel;
    for(string elem: symbolTableInsertionOrder){
      if((*symbolTable)[elem]->type == 0){
        helpBind = "LOC ";
      } else {
        helpBind = "GLOB";
      }
      if((*symbolTable)[elem]->section == -1){
        helpType = "SCTN ";
        helpLabel = "." + (*symbolTable)[elem]->label;
        helpNdx = to_string((*symbolTable)[elem]->entranceIndex);
      } else if((*symbolTable)[elem]->section == 0){
        helpType = "NOTYP";
        helpNdx = "UND";
        helpLabel = (*symbolTable)[elem]->label;
      } else {
        helpType = "NOTYP";
        helpNdx = to_string((*symbolTable)[elem]->section);
        helpLabel = (*symbolTable)[elem]->label;
      }
      outputTextFile << (*symbolTable)[elem]->entranceIndex << "\t" << toHexPrint((*symbolTable)[elem]->offset) << " " << helpType << "\t" << helpBind << "\t" << helpNdx << "\t\t" << helpLabel << endl;
    }

    // print sections
    if(!allSections->empty()){
      for(auto iter = allSections->begin(); iter != allSections->end(); ++iter){
        outputTextFile << "#." + iter->second->name << endl;
        int cnt = 0;
        for(char c : (*iter->second->hexCode)){
          if(cnt % 2 == 1){
            if(cnt == 15){
              cnt = 0;
              outputTextFile << c << endl;
            } else {
              outputTextFile << c << " ";
              cnt++;
            }
          } else {
            outputTextFile << c;
            cnt++;
          }
        }
        outputTextFile << endl;
      }

    // print relocation tables
      
      for(auto iter = allSections->begin(); iter != allSections->end(); ++iter){
        if(!iter->second->relocationTable->empty()){
          outputTextFile << "#.rela." + iter->second->name << endl;
          outputTextFile << "Section" << "Offset\t" << "Type\t" << "Symbol\t\t" << "Addend" << endl;

          for(RelocationTableEntrance* elem : (*iter->second->relocationTable)){
            //string symbol = elem->value;
            outputTextFile << elem->currSection << "\t" << toHexPrint(elem->offset) << " " << elem->type << "\t\t" << elem->value <<  "\t" <<elem->addend << endl;
          }
        }
      }
      
    } 

    outputTextFile.close();
  }
}