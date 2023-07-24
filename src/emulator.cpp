#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cmath>
#include "../inc/emulator.hpp"

#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>


using namespace std;

string inputFileName;

vector<char> memory;
registers regs;

volatile bool interruptFlag = false;


bool alreadySet = false;
bool final = false;
bool done = false;

struct itimerval interval;

struct termios oldt, newt;

int oldf;


void printOutput();

void interruptTimer(int signum){
  interruptFlag = true;
  
  //setitimer(ITIMER_REAL, &interval, nullptr);
}

void handleTimerInterrupt(){
  
  if((regs.status & 0x5) || regs.handler == 0){
    return;
  }

  // push STATUS
  memory[--regs.registers[sp]] = (regs.status & 0xff000000) >> 4*6;
  memory[--regs.registers[sp]] = (regs.status & 0x00ff0000) >> 4*4;
  memory[--regs.registers[sp]] = (regs.status & 0x0000ff00) >> 4*2;
  memory[--regs.registers[sp]] = (regs.status & 0x000000ff);

  // push PC
  memory[--regs.registers[sp]] = (regs.registers[pc] & 0xff000000) >> 4*6;
  memory[--regs.registers[sp]] = (regs.registers[pc] & 0x00ff0000) >> 4*4;
  memory[--regs.registers[sp]] = (regs.registers[pc] & 0x0000ff00) >> 4*2;
  memory[--regs.registers[sp]] = (regs.registers[pc] & 0x000000ff);

  regs.registers[pc] = regs.handler;
  regs.cause = 2;
  regs.status = regs.status ^(0x4);

}


void handleTerminalInterrupt(char c){
  if((regs.status & 0x6) || regs.handler == 0){
    fflush(stdin);
    return;
  }

  // push STATUS
  memory[--regs.registers[sp]] = (regs.status & 0xff000000) >> 4*6;
  memory[--regs.registers[sp]] = (regs.status & 0x00ff0000) >> 4*4;
  memory[--regs.registers[sp]] = (regs.status & 0x0000ff00) >> 4*2;
  memory[--regs.registers[sp]] = (regs.status & 0x000000ff);

  // push PC
  memory[--regs.registers[sp]] = (regs.registers[pc] & 0xff000000) >> 4*6;
  memory[--regs.registers[sp]] = (regs.registers[pc] & 0x00ff0000) >> 4*4;
  memory[--regs.registers[sp]] = (regs.registers[pc] & 0x0000ff00) >> 4*2;
  memory[--regs.registers[sp]] = (regs.registers[pc] & 0x000000ff);

  regs.registers[pc] = regs.handler;
  regs.cause = 3;
  regs.status = regs.status ^(0x4);

  // write char to term_in
  unsigned int address = stoul("FFFFFF04", nullptr, 16);
  memory[address] = c;
  address = stoul("FFFFFF05", nullptr, 16);
  memory[address] = 0;
  address = stoul("FFFFFF06", nullptr, 16);
  memory[address] = 0;
  address = stoul("FFFFFF07", nullptr, 16);
  memory[address] = 0;

  fflush(stdin);
}



int khbit(void){
  
  int ch;

  ch = getchar();

  if(ch != EOF){
    handleTerminalInterrupt(ch);
    return 1;
  }

  return 0;
}


int main(int argc, char* argv[]){

  if(argc < 2){
    cout << "ERROR: command line doesn't have name of the input file";
    exit(-1);
  }
  inputFileName = argv[1];
  
  unsigned long startAddress = stoul("40000000", nullptr, 16);
  

  memory.reserve(4294967296);
  

  ifstream inputHexFile(inputFileName);
  string binaryName = inputFileName.substr(0, inputFileName.length()-3) + "o";
  ifstream inputBinaryFile(binaryName, std::ios::binary);

  int cntLine = 0;
  if(inputHexFile.is_open() && inputBinaryFile.is_open()){
    string line;
    size_t temp;
    string beforeColon;
    string afterColon;
    unsigned long tmp;
    while(getline(inputHexFile, line)){
      temp = line.find(":");
      beforeColon = line.substr(0, temp);
      afterColon = line.substr(temp + 2);
      tmp = stoul(beforeColon, nullptr, 16);
      if(tmp >= startAddress){
        break;
      } else {
        // not found
        cntLine += 8;
      }
    }

    // move in binary
    char c;
    while(cntLine > 0){
      inputBinaryFile.read(reinterpret_cast<char*>(&c), sizeof(char));
      cntLine--;
    }

    // write to memory
    char c1, c2;
    
    // see how many bytes are after :
    istringstream iss(afterColon);
    string token;
    vector<string> tokens;

    while(getline(iss, token, ' ')){
      tokens.push_back(token);
    }

    for(int i = 0; i<tokens.size(); i++){
      inputBinaryFile.get(c1);
      inputBinaryFile.get(c2);
      char c1Char = static_cast<char>(c1);
      char c2Char = static_cast<char>(c2);
     
      string byte = string(1, c1Char) + c2Char;
      memory[tmp + i] = stoi(byte, nullptr, 16);
    }
    tokens.clear();

    while(getline(inputHexFile, line)){
      temp = line.find(":");
      beforeColon = line.substr(0, temp);
      afterColon = line.substr(temp + 2);
      tmp = stoul(beforeColon, nullptr, 16);
      
      istringstream iss(afterColon);
      while(getline(iss, token, ' ')){
        tokens.push_back(token);
      }

      for(int i = 0; i<tokens.size(); i++){
        inputBinaryFile.get(c1);
        inputBinaryFile.get(c2);
        char c1Char = static_cast<char>(c1);
        char c2Char = static_cast<char>(c2);
      
        string byte = string(1, c1Char) + c2Char;
        memory[tmp + i] = stoi(byte, nullptr, 16);
      }

      tokens.clear();
    }
  }
  
  inputHexFile.close();
  inputBinaryFile.close();
  
  ////////////////// INTERRUPT ROUTINE SETTINGS //////////////////
  
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  signal(SIGALRM, interruptTimer);
  
  ////////////////////////////////////////////////////////////////

  bool finished = false;
  uint8_t instructionDesc, regA, regB, regC;
  signed short offsetD;
  

  // initializing regs to 0 and pc to 40000000
  for(int i = 0; i<16; i++){
    regs.registers[i] = 0;
  }
  regs.registers[15] = startAddress;
  regs.cause = 0;
  regs.status = 0;
  regs.handler = 0;


  while(!finished){

    // CHECK REGISTER term_out and if it has contents cout and clear
    
    unsigned int c1 = ((memory[stoul("FFFFFF03", nullptr, 16)]) & 0xff) << 4*6 |
                              ((memory[stoul("FFFFFF02", nullptr, 16)]) & 0xff) <<  4*4 |
                              ((memory[stoul("FFFFFF01", nullptr, 16)]) & 0xff) << 4*2 |
                              ((memory[stoul("FFFFFF00", nullptr, 16)]) & 0xff);
    if(c1 != 0 ){
      memory[stoul("FFFFFF03", nullptr, 16)] = 0;
      memory[stoul("FFFFFF02", nullptr, 16)] = 0;
      memory[stoul("FFFFFF01", nullptr, 16)] = 0;
      memory[stoul("FFFFFF00", nullptr, 16)] = 0;
      
      putchar(c1);
      fflush(stdin);
    }

    // take value from tim_cfg
    
    unsigned int num = ((memory[stoul("FFFFFF13", nullptr, 16)]) & 0xff) << 4*6 |
                              ((memory[stoul("FFFFFF12", nullptr, 16)]) & 0xff) <<  4*4 |
                              ((memory[stoul("FFFFFF11", nullptr, 16)]) & 0xff) << 4*2 |
                              ((memory[stoul("FFFFFF10", nullptr, 16)]) & 0xff);


    switch(num){
      case 0: {
        if(!alreadySet){
          alreadySet = true;
          interval.it_value.tv_sec = 0;
          interval.it_value.tv_usec = 500000;
          interval.it_interval.tv_sec = 0;
          interval.it_interval.tv_usec = 500000;
          setitimer(ITIMER_REAL, &interval, nullptr);
        }
        break;
        
      }
      case 1: {
        if(alreadySet && !final){
          final = true;
          interval.it_value.tv_sec = 1;
          interval.it_value.tv_usec = 0;
          interval.it_interval.tv_sec = 1;
          interval.it_interval.tv_usec = 0;
          setitimer(ITIMER_REAL, &interval, nullptr);

        }
        break;
      }
      case 2: {
        if(alreadySet && !final){
          final = true;
          interval.it_value.tv_sec = 1;
          interval.it_value.tv_usec = 500000;
          interval.it_interval.tv_sec = 1;
          interval.it_interval.tv_usec = 500000;
          setitimer(ITIMER_REAL, &interval, nullptr);
        }
        break;
      }
      case 3: {
        if(alreadySet && !final){
          final = true;
          interval.it_value.tv_sec = 2;
          interval.it_value.tv_usec = 0;
          interval.it_interval.tv_sec = 2;
          interval.it_interval.tv_usec = 0;
          setitimer(ITIMER_REAL, &interval, nullptr);
        }
        break;
      }
      case 4: {
        if(alreadySet && !final){
          final = true;
          interval.it_value.tv_sec = 5;
          interval.it_value.tv_usec = 0;
          interval.it_interval.tv_sec = 5;
          interval.it_interval.tv_usec = 0;
          setitimer(ITIMER_REAL, &interval, nullptr);
        }
        break;
      }
      case 5: {
        if(alreadySet && !final){
          final = true;
          interval.it_value.tv_sec = 10;
          interval.it_value.tv_usec = 0;
          interval.it_interval.tv_sec = 10;
          interval.it_interval.tv_usec = 0;
          setitimer(ITIMER_REAL, &interval, nullptr);
        }
        break;
      }
      case 6: {
        if(alreadySet && !final){
          final = true;
          interval.it_value.tv_sec = 30;
          interval.it_value.tv_usec = 0;
          interval.it_interval.tv_sec = 30;
          interval.it_interval.tv_usec = 0;
          setitimer(ITIMER_REAL, &interval, nullptr);
        }
        break;
      }
      case 7: {
        if(alreadySet && !final){
          final = true;
          interval.it_value.tv_sec = 60;
          interval.it_value.tv_usec = 0;
          interval.it_interval.tv_sec = 60;
          interval.it_interval.tv_usec = 0;
          setitimer(ITIMER_REAL, &interval, nullptr);
        }
        break;
      }
    }


    instructionDesc = memory[regs.registers[pc]];
    regs.registers[pc] += 1;

    switch(instructionDesc){
      case HALT: {
        regs.registers[pc] += 3;
        finished = true;
        break;
      }
      case INT:{
        // software interrupt

        regs.registers[pc] += 3;

        // push STATUS
        memory[--regs.registers[sp]] = (regs.status & 0xff000000) >> 4*6;
        memory[--regs.registers[sp]] = (regs.status & 0x00ff0000) >> 4*4;
        memory[--regs.registers[sp]] = (regs.status & 0x0000ff00) >> 4*2;
        memory[--regs.registers[sp]] = (regs.status & 0x000000ff);

        // push PC
        memory[--regs.registers[sp]] = (regs.registers[pc] & 0xff000000) >> 4*6;
        memory[--regs.registers[sp]] = (regs.registers[pc] & 0x00ff0000) >> 4*4;
        memory[--regs.registers[sp]] = (regs.registers[pc] & 0x0000ff00) >> 4*2;
        memory[--regs.registers[sp]] = (regs.registers[pc] & 0x000000ff);

        regs.cause = 4;
        regs.status = regs.status &(~0x1);
        regs.registers[pc] = regs.handler;
        
        break;
      }
      case CALL0:{

        //push PC
        memory[--regs.registers[sp]] = ((regs.registers[pc] + 3) & 0xff000000) >> 4*6;
        memory[--regs.registers[sp]] = ((regs.registers[pc] + 3) & 0x00ff0000) >> 4*4;
        memory[--regs.registers[sp]] = ((regs.registers[pc] + 3) & 0x0000ff00) >> 4*2;
        memory[--regs.registers[sp]] = ((regs.registers[pc] + 3) & 0x000000ff);

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[pc] = regs.registers[regA] + regs.registers[regB] + offsetD;


        break;
      }
      case CALL1:{

         //push PC
        memory[--regs.registers[sp]] = ((regs.registers[pc] + 3) & 0xff000000) >> 4*6;
        memory[--regs.registers[sp]] = ((regs.registers[pc] + 3) & 0x00ff0000) >> 4*4;
        memory[--regs.registers[sp]] = ((regs.registers[pc] + 3) & 0x0000ff00) >> 4*2;
        memory[--regs.registers[sp]] = ((regs.registers[pc] + 3) & 0x000000ff);

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[pc] = ((memory[regs.registers[regA] + regs.registers[regB] + offsetD + 3]) & 0xff) << 4*6 |
                              ((memory[regs.registers[regA] + regs.registers[regB] + offsetD + 2]) & 0xff) <<  4*4 |
                              ((memory[regs.registers[regA] + regs.registers[regB] + offsetD + 1]) & 0xff) << 4*2 |
                              ((memory[regs.registers[regA] + regs.registers[regB] + offsetD + 0]) & 0xff);

        
        break;
      }
      case JMP0:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[pc] = regs.registers[regA] + offsetD;

        
        break;
      }
      case JMP1:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[pc] = ((memory[regs.registers[regA] + offsetD + 3]) & 0xff) << 4*6 |
                              ((memory[regs.registers[regA] + offsetD + 2]) & 0xff) <<  4*4 |
                              ((memory[regs.registers[regA] + offsetD + 1]) & 0xff) << 4*2 |
                              ((memory[regs.registers[regA] + offsetD + 0]) & 0xff);

        
        break;
      }
      case BEQ0:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        if(regs.registers[regB] == regs.registers[regC]){
          regs.registers[pc] = regs.registers[regA] + offsetD;
        }

        
        break;
      }
      case BEQ1:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        if(regs.registers[regB] == regs.registers[regC]){
          regs.registers[pc] = ((memory[regs.registers[regA] + offsetD + 3]) & 0xff) << 4*6 |
                              ((memory[regs.registers[regA] + offsetD + 2]) & 0xff) <<  4*4 |
                              ((memory[regs.registers[regA] + offsetD + 1]) & 0xff) << 4*2 |
                              ((memory[regs.registers[regA] + offsetD + 0]) & 0xff);
        }

        
        break;
      }
      case BNE0:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        if(regs.registers[regB] != regs.registers[regC]){
          regs.registers[pc] = regs.registers[regA] + offsetD;
        }
        
        break;
      }
      case BNE1:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        if(regs.registers[regB] != regs.registers[regC]){
          regs.registers[pc] = ((memory[regs.registers[regA] + offsetD + 3]) & 0xff) << 4*6 |
                              ((memory[regs.registers[regA] + offsetD + 2]) & 0xff) <<  4*4 |
                              ((memory[regs.registers[regA] + offsetD + 1]) & 0xff) << 4*2 |
                              ((memory[regs.registers[regA] + offsetD + 0]) & 0xff);
        }

        
        break;
      }
      case BGT0:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        if((signed)regs.registers[regB] > (signed)regs.registers[regC]){
          regs.registers[pc] = regs.registers[regA] + offsetD;
        }

        
        break;
      }
      case BGT1:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        if((signed)regs.registers[regB] > (signed)regs.registers[regC]){
          regs.registers[pc] = ((memory[regs.registers[regA] + offsetD + 3]) & 0xff) << 4*6 |
                              ((memory[regs.registers[regA] + offsetD + 2]) & 0xff) <<  4*4 |
                              ((memory[regs.registers[regA] + offsetD + 1]) & 0xff) << 4*2 |
                              ((memory[regs.registers[regA] + offsetD + 0]) & 0xff);
        }

        
        break;
      }
      case XCHG:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        int temp = regs.registers[regB];
        regs.registers[regB] = regs.registers[regC];
        regs.registers[regC] = temp;

        //finished = true;
        break;
      }
      case ADD:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = regs.registers[regB] + regs.registers[regC];

        
        break;
      }
      case SUB:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = regs.registers[regB] - regs.registers[regC];

        
        break;
      }
      case MUL:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = regs.registers[regB] * regs.registers[regC];

        
        break;
      }
      case DIV:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = regs.registers[regB] / regs.registers[regC];

        
        break;
      }
      case NOT:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = ~regs.registers[regB];

        
        break;
      }
      case AND:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0);

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = regs.registers[regB] & regs.registers[regC];

        
        break;
      }
      case OR:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = regs.registers[regB] | regs.registers[regC];

        
        break;
      }
      case XOR:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = regs.registers[regB] ^ regs.registers[regC];

        
        break;
      }
      case SHL:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = regs.registers[regB] << regs.registers[regC];

        
        break;
      }
      case SHR:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = regs.registers[regB] >> regs.registers[regC];

        
        break;
      }
      case ST0:{
        
        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        memory[regs.registers[regA] + regs.registers[regB] + offsetD + 3] = (regs.registers[regC] & 0xff000000) >> 4*6;
        memory[regs.registers[regA] + regs.registers[regB] + offsetD + 2] = (regs.registers[regC] & 0x00ff0000) >> 4*4;
        memory[regs.registers[regA] + regs.registers[regB] + offsetD + 1] = (regs.registers[regC] & 0x0000ff00) >> 4*2;
        memory[regs.registers[regA] + regs.registers[regB] + offsetD] = (regs.registers[regC] & 0x000000ff);

        
        break;
      }
      case ST1:{
        // PUSH

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] -= offsetD; // MAYBE CHANGE THIS TO +

        memory[regs.registers[regA] + 3] = (regs.registers[regC] & 0xff000000) >> 4*6;
        memory[regs.registers[regA] + 2] = (regs.registers[regC] & 0x00ff0000) >> 4*4;
        memory[regs.registers[regA] + 1] = (regs.registers[regC] & 0x0000ff00) >> 4*2;
        memory[regs.registers[regA]] = (regs.registers[regC] & 0x000000ff);
        

        break;
      }
      case ST2:{
        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;


        unsigned int value = ((memory[regs.registers[regA] + regs.registers[regB] + offsetD + 3]) & 0xff) << 4*6 |
                              ((memory[regs.registers[regA] + regs.registers[regB] + offsetD + 2]) & 0xff) <<  4*4 |
                              ((memory[regs.registers[regA] + regs.registers[regB] + offsetD + 1]) & 0xff) << 4*2 |
                              ((memory[regs.registers[regA] + regs.registers[regB] + offsetD + 0]) & 0xff);

        memory[value + 3] = (regs.registers[regC] & 0xff000000) >> 4*6;
        memory[value + 2] = (regs.registers[regC] & 0x00ff0000) >> 4*4;
        memory[value + 1] = (regs.registers[regC] & 0x0000ff00) >> 4*2;
        memory[value] = (regs.registers[regC] & 0x000000ff);
        

        
        break;
      }
      case LD0:{
        // CSRRD

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]&0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        if(regB == 0){
          regs.registers[regA] = regs.status;
        } else if(regB == 1){
          regs.registers[regA] = regs.handler;
        } else {
          regs.registers[regA] = regs.cause;
        }

        
        break;
      }
      case LD1:{
        //reg = reg + offset

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]&0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = regs.registers[regB] + offsetD;

        
        break;
      }
      case LD2:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 3]) & 0xff) << 4*6 |
                              ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 2]) & 0xff) <<  4*4 |
                              ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 1]) & 0xff) << 4*2 |
                              ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 0]) & 0xff);

       
        //cout << to_string(regA) << ": " << regs.registers[regA] << endl;
        
        break;
      }
      case LD3:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        regs.registers[regA] = ((memory[regs.registers[regB] + 3]) & 0xff) << 4*6 |
                              ((memory[regs.registers[regB] + 2]) & 0xff) <<  4*4 |
                              ((memory[regs.registers[regB] + 1]) & 0xff) << 4*2 |
                              ((memory[regs.registers[regB] + 0]) & 0xff);

        regs.registers[regB] += offsetD;

        
        break;
      }
      case LD4:{
        // CSRWR

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]&0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        if(regA == 0){
          regs.status = regs.registers[regB];
        } else if(regA == 1){
          regs.handler = regs.registers[regB];
        } else {
          regs.cause = regs.registers[regB];
        }

        
        break;
      }
      case LD5:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]&0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        if(regA == 0){
          regs.status = regs.registers[regB] | offsetD;
        } else if(regA == 1){
          regs.handler = regs.registers[regB] | offsetD;
        } else {
          regs.cause = regs.registers[regB] | offsetD;
        }

        //finished = true;
        break;
      }
      case LD6:{

        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        if(regA == 0){
          regs.status = ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 3]) & 0xff) << 4*6 |
                          ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 2]) & 0xff) <<  4*4 |
                          ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 1]) & 0xff) << 4*2 |
                          ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 0]) & 0xff);
        } else if(regA == 1){
          regs.handler = ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 3]) & 0xff) << 4*6 |
                          ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 2]) & 0xff) <<  4*4 |
                          ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 1]) & 0xff) << 4*2 |
                          ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 0]) & 0xff);
        } else {
          regs.cause = ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 3]) & 0xff) << 4*6 |
                        ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 2]) & 0xff) <<  4*4 |
                        ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 1]) & 0xff) << 4*2 |
                        ((memory[regs.registers[regB] + regs.registers[regC] + offsetD + 0]) & 0xff);
        }


        break;
      }
      case LD7:{
        
        regA = (memory[regs.registers[pc]]& 0xf0) >> 4;
        regB = (memory[regs.registers[pc]]& 0x0f);
        regs.registers[pc] += 1;

        regC = (memory[regs.registers[pc]]& 0xf0) >> 4;

        offsetD = (memory[regs.registers[pc]]& 0x0f) << 8 | memory[regs.registers[pc] + 1] & 0xff;
        if(offsetD > 2048){
          // two's complement 
          offsetD ^= 0x0fff;
          offsetD += 1;
          offsetD = -offsetD;
        }
        regs.registers[pc] += 2;

        if(regA == 0){
          regs.status = ((memory[regs.registers[regB] + 3]) & 0xff) << 4*6 |
                          ((memory[regs.registers[regB] + 2]) & 0xff) <<  4*4 |
                          ((memory[regs.registers[regB] + 1]) & 0xff) << 4*2 |
                          ((memory[regs.registers[regB] + 0]) & 0xff);
        } else if(regA == 1){
          regs.handler = ((memory[regs.registers[regB] + 3]) & 0xff) << 4*6 |
                          ((memory[regs.registers[regB] + 2]) & 0xff) <<  4*4 |
                          ((memory[regs.registers[regB] + 1]) & 0xff) << 4*2 |
                          ((memory[regs.registers[regB] + 0]) & 0xff);
        } else {
          regs.cause = ((memory[regs.registers[regB] + 3]) & 0xff) << 4*6 |
                        ((memory[regs.registers[regB] + 2]) & 0xff) <<  4*4 |
                        ((memory[regs.registers[regB] + 1]) & 0xff) << 4*2 |
                        ((memory[regs.registers[regB] + 0]) & 0xff);
        }

        regs.registers[regB] += offsetD;

        //finished = true;
        break;
      }
    }

    if(khbit()){
      
    }

    if(interruptFlag){
      interruptFlag = false;
      handleTimerInterrupt();
    }

   
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  printOutput();
}

void printOutput(){
  cout << endl;
  cout << "Emulated processor executed halt instruction" << endl;
  cout << "Emulated processor state:" << endl;

  int cnt = 0;
  int i = 0;
  for(int elem: regs.registers){
    ostringstream oss;
    oss << setw(8) << setfill('0') << hex << elem;
    if(cnt == 3){
      cout << "r" << i << "=0x";
      cout << oss.str() << endl;
      cnt = 0;
    } else {
      cnt++;
      cout << "r" << i << "=0x";
      cout << oss.str() << "\t";
    }
    i++;
  }

}

