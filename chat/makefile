# Leonard Law (#0428512)
# Spring 2013
# Program name: chat
# Purpose:
#   Class project for CS 239.

compiler = gcc
sources = src/main.c
flags = -Wall -std=c99
output = chat 

.PHONY : clean


all: default

default: $(sources)
	@$(compiler) $(sources) $(flags) -o $(output)

debug: $(sources)
	@$(compiler) $(sources) $(flags) -g -o $(output)

clean:
	@rm -f $(output)

