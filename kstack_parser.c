/*
 * Kernel stack parser
 *
 * Author: Vinayak Menon <vinayakm.list@gmail.com>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


char* input_file = "./kstack.bin";
char* output_file = "./kstack.dump";
char exec_buf[100];

unsigned long input_read_buf=0;
int main()
{
	FILE *input_fp, *output_fp;

	input_fp = fopen(input_file, "rb");
	if(!input_fp) {
		printf("Error opening the input file %s\n", input_file);
		return -1;
	}

        output_fp = fopen(output_file, "w+");
        if(!output_fp) {
		printf("Error opening the output file %s\n", output_file);
		fclose(input_fp);
		return -1;
	}
	
	while(!(feof(input_fp)) && !(ferror(input_fp))) {	
		if(!(fread(&input_read_buf, 4, 1, input_fp))) {
			if(!feof(input_fp))
				printf("Error reading file\n");
			goto out;
		}
		sprintf(exec_buf,"addr2line -e ./vmlinux 0x%x\0", input_read_buf);
		printf("0x%x\n",input_read_buf);
		fflush(stdout);
		system(exec_buf);
		fflush(stdout);
	}
out:
	fclose(input_fp);
	fclose(output_fp);
}


