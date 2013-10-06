all: src/main.c
	gcc -Wall -std=c99 src/main.c -o supershell

debug: src/main.c
	gcc -Wall -std=c99 -g src/main.c -o supershell

test: all
	@echo "-----------------------------"
	@echo "Testing exit..."
	      ./supershell < ./tests/test_exit
	@echo
	@echo "-----------------------------"
	@echo "Testing ls..."
	      ./supershell < ./tests/test_ls
	@echo
	@echo "-----------------------------"
	@echo "Testing redirecting stdin..."
	      ./supershell < ./tests/test_redirect_stdin
	@echo
	@echo "-----------------------------"
	@echo "Testing redirecting stdin multiple times..."
	      ./supershell < ./tests/test_redirectmultiple_stdin
	@echo
	@echo "-----------------------------"
	@echo "Testing redirecting stdout..."
	      ./supershell < ./tests/test_redirect_stdout
	@echo
	@echo "-----------------------------"
	@echo "Testing redirecting both stdin and stdout..."
	      ./supershell < ./tests/test_redirect_mix
	@echo
	@echo "-----------------------------"
	@echo "Testing complete."
	@echo
