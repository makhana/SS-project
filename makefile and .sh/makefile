CC = g++
CFLAGS = -d
BISON = bison
FLEX = flex


all: emulator linker asembler


emulator: src/emulator.cpp | linker
		$(CC) -g src/emulator.cpp -o emulator

linker: src/linker.cpp | asembler
		$(CC) -g src/linker.cpp -o linker

asembler: src/main.cpp src/assembler.cpp parser.tab.c lex.yy.c
		$(CC) -Iinc -g src/main.cpp src/assembler.cpp parser.tab.c lex.yy.c -o asembler

parser.tab.c: misc/parser.y
		$(BISON) $(CFLAGS) misc/parser.y

lex.yy.c: misc/lexer.l
		$(FLEX) misc/lexer.l

clean:
	rm -f lex.yy.c parser.tab.c parser.tab.h *.o $(objects) *.txt $(objects) *.hex $(objects)

