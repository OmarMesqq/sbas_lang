VALGRIND_LOG := /tmp/valgrind.log
OUTPUT := /tmp/sbas
TEST_OUTPUT := /tmp/sbas_test

debug:
	gcc -g -no-pie -Wall -Wextra main.c sbas.c utils.c assembler.c linker.c -o $(OUTPUT) -lm

release:
	gcc -O3 -Wall -Wextra  main.c sbas.c utils.c assembler.c linker.c -o $(OUTPUT) -lm

test:
	gcc -g -Wall -Wextra run_tests.c sbas.c utils.c assembler.c linker.c -o $(TEST_OUTPUT)

memleak-check: test
	@valgrind -s --leak-check=full --track-origins=yes --show-leak-kinds=all /tmp/sbas_test 2> $(VALGRIND_LOG)
	@grep -Fq "All heap blocks were freed -- no leaks are possible" $(VALGRIND_LOG) && \
	grep -Fq "ERROR SUMMARY: 0 errors from 0 contexts" $(VALGRIND_LOG) && \
	echo "✅ No leaks or errors detected." || \
	(echo "❌ Memory/resource leaks or errors found!"; cat $(VALGRIND_LOG); exit 1)

clean:
	rm -f $(VALGRIND_LOG) $(OUTPUT) $(TEST_OUTPUT)
