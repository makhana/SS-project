#define HALT 0x00
#define INT 0x10

//#define IRET

#define CALL1 0x21
#define CALL0 0x20

//#define RET 

#define JMP0 0x30
#define JMP1 0x38
#define BEQ0 0x31
#define BEQ1 0x39
#define BNE0 0x32
#define BNE1 0x3A
#define BGT0 0x33
#define BGT1 0x3B

#define XCHG 0x40

#define ADD 0x50
#define SUB 0x51
#define MUL 0x52
#define DIV 0x53

#define NOT 0x60
#define AND 0x61
#define OR 0x62
#define XOR 0x63

#define SHL 0x70
#define SHR 0x71

#define ST0 0x80
#define ST1 0x81
#define ST2 0x82

#define LD0 0x90
#define LD1 0x91
#define LD2 0x92
#define LD3 0x93
#define LD4 0x94
#define LD5 0x95
#define LD6 0x96
#define LD7 0x97

struct registers{
  unsigned int registers[16];
  int status;
  int handler;
  int cause;
};

enum{r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15};
enum{sp=14, pc=15};

