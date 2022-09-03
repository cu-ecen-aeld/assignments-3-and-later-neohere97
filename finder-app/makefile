# /***************************************************************************
#  * AESD Assignment 2
#  * Author: Chinmay Shalawadi 
#  * Institution: University of Colorado Boulder
#  * Mail id: chsh1552@colorado.edu
#  * References: Stack Overflow, Man Pages
#  ***************************************************************************/

make:
# Check if cross compile variable matches for ARM 
ifeq ($(CROSS_COMPILE),aarch64-none-linux-gnu-)
		aarch64-none-linux-gnu-gcc -Wall -Werror writer.c -o writer
else	
# Else compile for native system
		gcc -Wall -Werror writer.c -o writer
endif

# Clean to remove binaries 
clean:
	rm -f *.o *.out
	rm -f writer

