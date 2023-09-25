gcc ./src/echo_server.c ./src/parse.c  ./test/lex.yy.c ./test/y.tab.c  -o ./test/server.o
gcc ./src/echo_client.c ./src/parse.c  ./test/lex.yy.c ./test/y.tab.c  -o ./test/client.o