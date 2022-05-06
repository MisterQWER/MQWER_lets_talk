all: lets-talk.c list.c
	gcc -g -Wall -o lets-talk lets-talk.c list.c -lpthread -ggdb3
clean: 
	$(RM) lets-talk
