all:
	gcc -Wall -std=c99 main.c -o supershell

debug:
	gcc -Wall -std=c99 -g main.c -o supershell

test:
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
	@echo "Testing redirecting stdout multiple times..."
	      ./supershell < ./tests/test_redirectmultiple_stdout
	@echo
	@echo "-----------------------------"
	@echo "Testing redirecting both stdin and stdout..."
	      ./supershell < ./tests/test_redirect_mix
	@echo
	@echo "-----------------------------"
	@echo "Testing piping with 2 commands..."
	      ./supershell < ./tests/test_pipe_simple
	@echo
	@echo "-----------------------------"
	@echo "Testing complete."
	@echo
