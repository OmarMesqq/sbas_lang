VALGRIND_LOG := /tmp/valgrind.log
OUTPUT := /tmp/sbas
TEST_OUTPUT := /tmp/sbas_test

debug:
	gcc -g -Wall -Wextra -Wa,--execstack main.c peqcomp.c -o $(OUTPUT)

release:
	gcc -O3 -Wall -Wextra -Wa,--execstack main.c peqcomp.c -o $(OUTPUT)

test:
	gcc -g -Wall -Wextra -Wa,--execstack run_tests.c peqcomp.c -o $(TEST_OUTPUT)

memleak-check: test
	@valgrind -s --leak-check=full --track-origins=yes --show-leak-kinds=all /tmp/sbas_test 2> $(VALGRIND_LOG)
	@grep -Fq "All heap blocks were freed -- no leaks are possible" $(VALGRIND_LOG) && \
	grep -Fq "ERROR SUMMARY: 0 errors from 0 contexts" $(VALGRIND_LOG) && \
	echo "✅ No leaks or errors detected." || \
	(echo "❌ Memory/resource leaks or errors found!"; cat $(VALGRIND_LOG); exit 1)

clean:
	rm -f $(VALGRIND_LOG) $(OUTPUT) $(TEST_OUTPUT)
