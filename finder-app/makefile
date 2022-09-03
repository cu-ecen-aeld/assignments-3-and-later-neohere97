make:
ifeq ($(CROSS_COMPILE),aarch64-none-linux-gnu-)
		aarch64-none-linux-gnu-gcc -Wall -Werror writer.c -o writer
else	
		gcc -Wall -Werror writer.c -o writer
endif

clean:
	rm -f *.o *.out
	rm -f writer

