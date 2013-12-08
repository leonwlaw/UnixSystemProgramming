# Leonard Law (#0428512)
# Spring 2013
# Program name: chat
# Purpose:
#   Class project for CS 239.

compiler = gcc
flags = -Wall -std=c99 -g

client_target = client
clientsources = src/client.c

server_target = server
serversources = src/server.c

all: $(client_target) $(server_target)

$(client_target): $(clientsources)
	@$(compiler) $(clientsources) $(flags) -o $(client_target)

$(server_target): $(serversources)
	@$(compiler) $(serversources) $(flags) -o $(server_target)

clean:
	@rm -f $(client_target) $(server_target)

