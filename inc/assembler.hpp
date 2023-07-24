#include "helperStructs.hpp"


void assemblerAddLabel(string* label);

void assemblerGlobalDIR(MultipleArguments* arguments);
void assemblerExternDIR(MultipleArguments* arguments);
void assemblerSectionDIR(string* name);
void assemblerWordDIr(MultipleArguments* arguments);
void assemblerSkipDIR(string *value);
void assemblerASCIIDir(string *argument);

//odradjen samo za literale - ispraviti kasnije za sve expressione
void assemblerEQUDir(string *symbol, MultipleArguments* arguments);

void assemblerENDDIR();


// INSTRUCTIONS without operands
void assemblerHALT();
void assemblerINT();
void assemblerIRET();
void assemblerRET();
void assemblerPUSH(string* reg);
void assemblerPOP(string* reg);
void assemblerXCHG(string* reg1, string* reg2);
void assemblerADD(string* reg1, string* reg2);
void assemblerSUB(string* reg1, string* reg2);
void assemblerMUL(string* reg1, string* reg2);
void assemblerDIV(string* reg1, string* reg2);
void assemblerNOT(string* reg);
void assemblerAND(string* reg1, string* reg2);
void assemblerOR(string* reg1, string* reg2);
void assemblerXOR(string* reg1, string* reg2);
void assemblerSHL(string* reg1, string* reg2);
void assemblerSHR(string* reg1, string* reg2);
void assemblerCSRRD(string* programReg, string* reg);
void assemblerCSRWR(string* programReg, string* reg);

// JUMP instructions
void assemblerJMP(MultipleArguments* arguments);
void assemblerBEQ(string* reg1, string* reg2, MultipleArguments* arguments);
void assemblerBNE(string* reg1, string* reg2, MultipleArguments* arguments);
void assemblerBGT(string* reg1, string* reg2, MultipleArguments* arguments);

// LD and ST
void assemblerLD(MultipleArguments* arguments, string* reg);
void assemblerST(string* reg, MultipleArguments* arguments);

// CALL
void assemblerCALL(MultipleArguments* arguments);

void printSymbolTable();