%code requires{
  #include <stdio.h>
  #include <string>
  #include <iostream>
  #include "../inc/assembler.hpp"
  extern FILE *yyin;
  int yyerror(const char* s);
  int yylex();
  extern bool secondPass;
  extern int locationCounter;
  extern Section* activeSection;
  using namespace std;
}
%{
%}

%union {
  int num;
  string* strType;
  MultipleArguments* vectorArguments;
}

%token EOL COLON DOLLAR PERCENTAGE LEFTBRACKET RIGHTBRACKET PLUS COMMA MINUS
%token GLOBALDIR EXTERNDIR SECTIONDIR WORDDIR SKIPDIR ASCIIDIR EQUDIR ENDDIR
%token HALT INT IRET CALL RET JMP BEQ BNE BGT PUSH POP XCHG ADD SUB MUL DIV NOT AND OR XOR SHL SHR LD ST CSRRD CSRWR

%token <strType> R0
%token <strType> R1
%token <strType> R2
%token <strType> R3
%token <strType> R4
%token <strType> R5
%token <strType> R6
%token <strType> R7
%token <strType> R8
%token <strType> R9
%token <strType> R10
%token <strType> R11
%token <strType> R12
%token <strType> R13
%token <strType> R14
%token <strType> R15

%token <strType> STATUS
%token <strType> HANDLER
%token <strType> CAUSE

%token <strType> SYMBOL
%token <strType> LITERAL
%token <strType> STRING



%type <strType> register
%type <strType> programRegister


%type <vectorArguments> symbolList
%type <vectorArguments> symbolOrLiteralList
%type <vectorArguments> operand
%type <vectorArguments> operandJump

%type <vectorArguments> expression


%%

input: {}
| line input {}
;
  
line: EOL {}

/* label: */
| SYMBOL COLON EOL {assemblerAddLabel($1);}

/* label: directive parameters EOL */
| SYMBOL COLON GLOBALDIR symbolList EOL {assemblerAddLabel($1); assemblerGlobalDIR($4);}
| SYMBOL COLON EXTERNDIR symbolList EOL {assemblerAddLabel($1); assemblerExternDIR($4);}
| SYMBOL COLON SECTIONDIR SYMBOL EOL {assemblerAddLabel($1); assemblerSectionDIR($4);}
| SYMBOL COLON WORDDIR symbolOrLiteralList EOL {assemblerAddLabel($1); assemblerWordDIr($4);}
| SYMBOL COLON SKIPDIR LITERAL EOL {assemblerAddLabel($1); assemblerSkipDIR($4);}
| SYMBOL COLON ASCIIDIR STRING EOL {assemblerAddLabel($1); assemblerASCIIDir($4);}
| SYMBOL COLON EQUDIR SYMBOL COMMA expression EOL {assemblerAddLabel($1); assemblerEQUDir($4, $6);}
| SYMBOL COLON ENDDIR EOL {assemblerAddLabel($1); assemblerENDDIR(); return -2;}

/* directive parameters EOL */
| GLOBALDIR symbolList EOL {assemblerGlobalDIR($2);}
| EXTERNDIR symbolList EOL {assemblerExternDIR($2);}
| SECTIONDIR SYMBOL EOL {assemblerSectionDIR($2);}
| WORDDIR symbolOrLiteralList EOL {assemblerWordDIr($2);}
| SKIPDIR LITERAL EOL {assemblerSkipDIR($2);}
| ASCIIDIR STRING EOL {assemblerASCIIDir($2);}
| EQUDIR SYMBOL COMMA expression EOL {assemblerEQUDir($2, $4);}
| ENDDIR {assemblerENDDIR(); return -2;}
| ENDDIR EOL {assemblerENDDIR(); return -2;}

/* label: instruction parameters EOL */
| SYMBOL COLON HALT EOL {assemblerAddLabel($1); assemblerHALT();}
| SYMBOL COLON INT EOL {assemblerAddLabel($1); assemblerINT();}
| SYMBOL COLON IRET EOL {assemblerAddLabel($1); assemblerIRET();}
| SYMBOL COLON CALL operandJump EOL {assemblerAddLabel($1); assemblerCALL($4);}
| SYMBOL COLON RET EOL {assemblerAddLabel($1); assemblerRET();}
| SYMBOL COLON JMP operandJump EOL {assemblerAddLabel($1); assemblerJMP($4);}
| SYMBOL COLON BEQ PERCENTAGE register COMMA PERCENTAGE register COMMA operandJump EOL {assemblerAddLabel($1); assemblerBEQ($5, $8, $10);}
| SYMBOL COLON BNE PERCENTAGE register COMMA PERCENTAGE register COMMA operandJump EOL {assemblerAddLabel($1); assemblerBNE($5, $8, $10);}
| SYMBOL COLON BGT PERCENTAGE register COMMA PERCENTAGE register COMMA operandJump EOL {assemblerAddLabel($1); assemblerBGT($5, $8, $10);}
| SYMBOL COLON PUSH PERCENTAGE register EOL {assemblerAddLabel($1); assemblerPUSH($5);}
| SYMBOL COLON POP PERCENTAGE register EOL {assemblerAddLabel($1); assemblerPOP($5);}
| SYMBOL COLON XCHG PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerXCHG($5, $8);}
| SYMBOL COLON ADD PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerADD($5, $8);}
| SYMBOL COLON SUB PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerSUB($5, $8);}
| SYMBOL COLON MUL PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerMUL($5, $8);}
| SYMBOL COLON DIV PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerDIV($5, $8);}
| SYMBOL COLON NOT PERCENTAGE register EOL {assemblerAddLabel($1); assemblerNOT($5);}
| SYMBOL COLON AND PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerAND($5, $8);}
| SYMBOL COLON OR PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerOR($5, $8);}
| SYMBOL COLON XOR PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerXOR($5, $8);}
| SYMBOL COLON SHL PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerSHL($5, $8);}
| SYMBOL COLON SHR PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerSHR($5, $8);}
| SYMBOL COLON LD operand COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerLD($4, $7);}
| SYMBOL COLON ST PERCENTAGE register COMMA operand EOL {assemblerAddLabel($1); assemblerST($5, $7);}
| SYMBOL COLON CSRRD PERCENTAGE programRegister COMMA PERCENTAGE register EOL {assemblerAddLabel($1); assemblerCSRRD($5, $8);}
| SYMBOL COLON CSRWR PERCENTAGE register COMMA PERCENTAGE programRegister EOL {assemblerAddLabel($1); assemblerCSRWR($5, $8);}

/* instruction parameters EOL */
| HALT EOL {assemblerHALT();}
| INT EOL {assemblerINT();}
| IRET EOL {assemblerIRET();}
| CALL operandJump EOL {assemblerCALL($2);}
| RET EOL {assemblerRET();}
| JMP operandJump EOL {assemblerJMP($2);}
| BEQ PERCENTAGE register COMMA PERCENTAGE register COMMA operandJump EOL {assemblerBEQ($3, $6, $8);}
| BNE PERCENTAGE register COMMA PERCENTAGE register COMMA operandJump EOL {assemblerBNE($3, $6, $8);}
| BGT PERCENTAGE register COMMA PERCENTAGE register COMMA operandJump EOL {assemblerBGT($3, $6, $8);}
| PUSH PERCENTAGE register EOL {assemblerPUSH($3);}
| POP PERCENTAGE register EOL {assemblerPOP($3);}
| XCHG PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerXCHG($3, $6);}
| ADD PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerADD($3, $6);}
| SUB PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerSUB($3, $6);}
| MUL PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerMUL($3, $6);}
| DIV PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerDIV($3, $6);}
| NOT PERCENTAGE register EOL {assemblerNOT($3);}
| AND PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerAND($3, $6);}
| OR PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerOR($3, $6);}
| XOR PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerXOR($3, $6);}
| SHL PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerSHL($3, $6);}
| SHR PERCENTAGE register COMMA PERCENTAGE register EOL {assemblerSHR($3, $6);}
| LD operand COMMA PERCENTAGE register EOL {assemblerLD($2, $5);}
| ST PERCENTAGE register COMMA operand EOL {assemblerST($3, $5);}
| CSRRD programRegister COMMA PERCENTAGE register EOL {assemblerCSRRD($2, $5);}
| CSRWR PERCENTAGE register COMMA programRegister EOL {assemblerCSRWR($3, $5);}
;

operand: DOLLAR LITERAL {$$ = new MultipleArguments($2, 1, 10);}
| DOLLAR SYMBOL {$$ = new MultipleArguments($2, 0, 11);}
| LITERAL {$$ = new MultipleArguments($1, 1, 12);}
| SYMBOL {$$ = new MultipleArguments($1, 0, 13);}
| PERCENTAGE register {$$ = new MultipleArguments($2, 2, 14);}
| LEFTBRACKET PERCENTAGE register RIGHTBRACKET {$$ = new MultipleArguments($3, 2, 15);}
| LEFTBRACKET PERCENTAGE register PLUS LITERAL RIGHTBRACKET {$$ = new MultipleArguments($3, 2, 16); $$->argumentName->push_back($5); $$->argumentType->push_back(1);}
| LEFTBRACKET PERCENTAGE register PLUS SYMBOL RIGHTBRACKET {$$ = new MultipleArguments($3, 2, 17); $$->argumentName->push_back($5); $$->argumentType->push_back(0);}
;

operandJump: LITERAL {$$ = new MultipleArguments($1, 1, 10);}
| SYMBOL {$$ = new MultipleArguments($1, 0, 11);}
;

expression: LITERAL {$$ = new MultipleArguments($1, 1, 0);}
| SYMBOL {$$ = new MultipleArguments($1, 1, 1);}
| LITERAL PLUS LITERAL {$$ = new MultipleArguments($1, 1, 2); $$->argumentName->push_back($3);}
| LITERAL MINUS LITERAL {$$ = new MultipleArguments($1, 1, 3); $$->argumentName->push_back($3);}
| SYMBOL PLUS SYMBOL {$$ = new MultipleArguments($1, 1, 4); $$->argumentName->push_back($3);}
| SYMBOL MINUS SYMBOL {$$ = new MultipleArguments($1, 1, 5); $$->argumentName->push_back($3);}
; 

symbolOrLiteralList: SYMBOL { $$ = new MultipleArguments($1, 0, 0);}
| LITERAL { $$ = new MultipleArguments($1, 1, 0);}
| symbolOrLiteralList COMMA SYMBOL {$1->argumentName->push_back($3); $1->argumentType->push_back(0); $$ = $1;}
| symbolOrLiteralList COMMA LITERAL {$1->argumentName->push_back($3); $1->argumentType->push_back(1); $$ = $1;}
;

symbolList: SYMBOL {$$ = new MultipleArguments($1, 0, 0);}
| symbolList COMMA SYMBOL {$1->argumentName->push_back($3); $1->argumentType->push_back(0); $$ = $1;}
;

register: R0 {$$ = $1;}
| R1 {$$ = $1;}
| R2 {$$ = $1;}
| R3 {$$ = $1;}
| R4 {$$ = $1;}
| R5 {$$ = $1;}
| R6 {$$ = $1;}
| R7 {$$ = $1;}
| R8 {$$ = $1;}
| R9 {$$ = $1;}
| R10 {$$ = $1;}
| R11 {$$ = $1;}
| R12 {$$ = $1;}
| R13 {$$ = $1;}
| R14 {$$ = $1;}
| R15 {$$ = $1;}
;

programRegister: STATUS {$$ = $1;}
| HANDLER {$$ = $1;}
| CAUSE {$$ = $1;}
;

%%

int parserMain(int argc, char* argv[]){
  FILE *fp = NULL;

  if(argc == 2){
    fp = fopen(argv[1], "r");
  } else {
    fp = fopen(argv[3], "r");
  }
  //fp = fopen("proba1.s", "r");

  if(fp == NULL){
    perror("Failed to open specified file");
    return -1;
  } else {
    yyin = fp;
  }

  while(yyparse()!= -2);

  if(fp != NULL){
    fclose(fp);
  }

  secondPass = true;
  locationCounter = 0;
  activeSection = nullptr;


  if(argc == 2){
    fp = fopen(argv[1], "r");
  } else {
    fp = fopen(argv[3], "r");
  }
  //fp = fopen("proba1.s", "r");

  if(fp == NULL){
    perror("Failed to open specified file");
    return -1;
  } else {
    yyin = fp;
  }

  while(yyparse()!= -2);

  if(fp != NULL){
    fclose(fp);
  }

  return 0;
}

int yyerror(const char* s){
  printf("%s\n", s);
  return 0;
}