gcc lexer.c -c
gcc symbol_table.c -c
gcc parser.c -c
gcc type_checker.c -c
gcc evaluator.c -c
gcc lib.c -c
gcc simplifier.c -c
gcc compiler.c lexer.o symbol_table.o parser.o type_checker.o evaluator.o lib.o simplifier.o -o set_algebra.exe
set_algebra sample.txt
pause
del *.o