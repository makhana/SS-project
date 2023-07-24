#include "../inc/helperStructs.hpp"
#include <string>
#include <map>
#include <vector>
#include <sstream>

using namespace std;


extern int parserMain(int argc, char* argv[]);


int SymbolTableEntry::entranceIndexID = 1;
map<string, SymbolTableEntry*> *symbolTable = new map<string, SymbolTableEntry*>();

map<int, Section*> *allSections = new map<int, Section*>();



vector<string>* startingMachineCode = new vector<string> ();
int startingMachineCodeLength = 0;

Section* activeSection = nullptr;
int locationCounter = 0;
bool secondPass = false;

string outputFileName;

int main (int argc, char* argv[]){
  if(argc == 2){
    outputFileName = argv[1];
    outputFileName = outputFileName.substr(0, outputFileName.length() - 2) + ".o";
  } else {
    outputFileName = argv[2];
  }
  
  return parserMain(argc, argv);
}