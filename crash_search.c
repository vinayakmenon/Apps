/*
 * crash_search
 *
 * It parses the Oops dump, deciphers each hex
 * value present under sections PC:, LR: etc and
 * categorizes them using the kernel virtual
 * memory layout. If values come from code section,
 * the source file and line number is printed. The
 * Virtual memory layout is picked by "crash search".
 * from the kernel log, but if the log is incomplete,
 * only source file with line number will be available.
 *
 * Author: Vinayak Menon <vinayakm.list@gmail.com>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*The program assumes that the maximum length
 *of a line in the crash dump will not exceed
 *160 chars
 */
#define MAX_INPUT_READ_BUF_SIZE 161
char input_read_buf[MAX_INPUT_READ_BUF_SIZE];
int number_of_sections = 0;
int virtual_mem_layout_found = 0;
char* input_file;
char* vmlinux_path;
int crash_search_start(FILE *input_fp, FILE *output_fp);
int check_for_string_in_next_line(char* input_read_buf, FILE *input_fp, FILE *output_fp, const char* search);

void show_help(void)
{
	printf("Usage: crash_search -i [path to crash file] -v [path to vmlinux]\n");
	printf("options: i,v,h\n");
	printf("i : path to the crash dump file\n");
	printf("v : path to the vmlinux file\n");
	printf("h : help\n");
	fflush(stdout);
}

int main(int argc, char *argv[])
{
	FILE *input_fp, *output_fp;
	int ret;
	int c, vm = 0, in = 0;

	while((c = getopt(argc, argv, ":v:i:h:")) != -1) {
		switch(c) {
			case 'v':
				vmlinux_path = optarg;
				vm = 1;
				break;
			case 'i':
				input_file = optarg;
				in = 1;
				break;
			case 'h':
				show_help();
				exit(2);
				break;
			case '?':
				printf("Unknown option\n");
				show_help();
				exit(2);
				break;
		}
	}
	
	if(!vm) {
		printf("vmlinux path missing\n");
		show_help();
		exit(2);
	}

	if(!in) {
		printf("crash dump file path missing\n");
		show_help();
		exit(2);
	}
		
	input_fp = fopen(input_file, "r");
	if(!input_fp) {
		printf("Error opening the input file %s\n", input_file);
		return -1;
	}

	/*output_fp is unused at present*/
	ret = crash_search_start(input_fp, output_fp);
	if(ret < 0) {
		printf("Halt\n");
		return -1;
	}
	printf("******************************END**************************************");
	return 0;
}

int crash_search_start(FILE *input_fp, FILE *output_fp)
{
#define INPUT_FP_ERR(x) {									\
				if(!feof(input_fp)) {						\
					fclose(input_fp);					\
					fclose(output_fp);   					\
					printf("Error reading from the input file, DP %d\n",x); \
					return -1;           					\
		     		} else								\
					return 1;						\
		       }									\
	

#define GEN_ERR(x) {				\
			fclose(input_fp);	\
			fclose(output_fp);	\
			printf("%s\n",x);	\
			return -1;		\
		   }				\

	int ret;
	long curr_position;
	/*Read each line of the input file till eof*/
	while(!(feof(input_fp))) {

		curr_position = ftell(input_fp);
		/*Find the Virtual memory layout*/
		ret = check_for_string_in_next_line(input_read_buf, input_fp, output_fp, "Virtual kernel memory layout:");
		if(ret < 0)
			return -1;
		else if(!ret) {
			/*Found the Virtual memory layout*/
			parse_virtual_memory_layout(input_read_buf, input_fp, output_fp);
		} else {
			 if(fseek(input_fp, curr_position, SEEK_SET)) {
                         	GEN_ERR("Error setting the input file position\n");
                         }
		}

		/*Step 1: Check if we can find "Flags:" in the string*/
		ret = check_for_string_in_next_line(input_read_buf, input_fp, output_fp, "Flags:");
		if(ret < 0)
			return -1;
		else if(!ret) {
			/*We got "Flag:"*/
			/*Get the current position, so that if we dont find what we need
			 *we can go back and start our search again
			 */
			curr_position = ftell(input_fp);

			/*Step 2: Lets go and check if we can find "Control:" in the next line*/
			ret = check_for_string_in_next_line(input_read_buf, input_fp, output_fp, "Control:");
			if(ret < 0)
				return -1;
			else if(!ret) {
				/*We got "Control:"*/
				curr_position = ftell(input_fp);
				/*Step 2: Lets go and check if we can find "PC:" in the curr line or next*/
				ret = check_for_string_in_next_line(input_read_buf, input_fp, output_fp, "PC:");
				if(ret < 0)
					return -1;
				else if(!ret) {
					/*We got PC:*/
				} else {
					curr_position = ftell(input_fp);
					/*Check for "PC:" again. Usually there is a blank space in Oops dump*/
					ret = check_for_string_in_next_line(input_read_buf, input_fp, output_fp, "PC:");
					if(ret < 0)
						return -1;
					else if(!ret) {
						start_oops_parsing(input_fp, output_fp, input_read_buf,curr_position);
					} else {
						if(fseek(input_fp, curr_position, SEEK_SET)) {
							GEN_ERR("Error setting the input file position\n");
						}
							/*We didnt get PC, continue, as it means that
                                                         *the "Flags:" and "Contro:" we had got didnt
                                                         *belong to the Oops dump
							 */
                                                         continue;
					}
				}
		
			} else {
				/*We found flag, but not control, which means the flag was
				 *not a part of oops dump*/
				if(fseek(input_fp, curr_position, SEEK_SET)) {
					GEN_ERR("Error setting the input file position\n");
				}
				continue;
				
			}	
		} else {
			/*We didnt find flag, continue*/
			continue;
		}
	}	
			
	return 0;
}

struct virt_mem {
	char name[10];
	unsigned long start;
	unsigned long end;
};

struct virt_mem *virt_mem_layout;

int parse_virtual_memory_layout(char* input_read_buf, FILE *input_fp, FILE *output_fp)
{
	int curr_position = ftell(input_fp);
	int count = 0;
	char* s;
	int loop_till = 0;
	int valid = 0;
	int loop_count = 0;

	do {
		count = 0;
		valid = 0;
		if(!(fgets(input_read_buf, MAX_INPUT_READ_BUF_SIZE, input_fp))) {
			INPUT_FP_ERR(6)
		} else {
	        	if((s = strtok(input_read_buf, " "))) {
				count++;
               			while((s = strtok(NULL, " "))) {
                       			count++;
					if(!strncmp("kB",s,2) || !strncmp("MB",s,2) || !strncmp("GB",s,2)) {
						valid = 1;
					}
               			}
       		 	}
		}
		if(!valid)
			break;

		loop_till++;
	} while(count >= 9);
	
	number_of_sections = loop_till;
	virt_mem_layout = (struct virt_mem*)malloc(loop_till*sizeof(struct virt_mem));
        
	if(fseek(input_fp, curr_position, SEEK_SET)) {
        	GEN_ERR("Error setting the input file position\n");
        }
	
	printf("**********Kernel Virtual Memory layout***************\n");
	printf("section,start,end\n");
	fflush(stdout);
	for(loop_count = 0; loop_count < loop_till; loop_count++) {
		count = 0;

                if(!(fgets(input_read_buf, MAX_INPUT_READ_BUF_SIZE, input_fp))) {
                        INPUT_FP_ERR(6)
                } else {
                        if((s = strtok(input_read_buf, " "))) {
                                count++;
                                while((s = strtok(NULL, " "))) {
                                        count++;
					if(count == 3)
						strcpy(virt_mem_layout[loop_count].name,s);
					else if(count == 5)
						virt_mem_layout[loop_count].start = strtoul(s, NULL, 0);
					else if(count == 7)
						virt_mem_layout[loop_count].end = strtoul(s, NULL, 0);
                                }
                        }
                }
		printf("%s,%x,%x\n",virt_mem_layout[loop_count].name,virt_mem_layout[loop_count].start,virt_mem_layout[loop_count].end);
		fflush(stdout);
		
	}
	printf("**********************************************************\n");
	fflush(stdout);
	virtual_mem_layout_found = 1;
}

int check_for_string_in_next_line(char* input_read_buf, FILE *input_fp, FILE *output_fp, const char* search)
{
	
	if(!(fgets(input_read_buf, MAX_INPUT_READ_BUF_SIZE, input_fp))) {
                        INPUT_FP_ERR(1)
        } else {
                if(strstr(input_read_buf, search)) {
			return 0;
		} else
			return 1;
	}
}

int parse_header(char* input_read_buf, FILE *input_fp, FILE *output_fp, int curr_position)
{
	char *s, *org;
	char sprint_buf[150];
	int count = 0;
	
	/*This is a crude way to check whether this is a header
	 *Here we just check the number of sequences seperated
	 *by delimiters. Change it some time with a good way.
	 */
	if((s = strtok(input_read_buf, " "))) {
		count++;
		while((s = strtok(NULL, " "))) {
			count++;
		}
	}

	/*Skip a newline in the Oops dump*/
	if((count == 1) || (count == 3)) {
		return 1;
	}
	else if(count != 4) {
		/*This is not a header*/
		return -1;
	}

	count = 0;
	/*strtok would have changed the pointer, read again*/
	if(fseek(input_fp, curr_position, SEEK_SET)) {
		GEN_ERR("Error setting the input file position\n");
	}

	if(!(fgets(input_read_buf, MAX_INPUT_READ_BUF_SIZE, input_fp))) {
		INPUT_FP_ERR(4)
	}

        if((s = strtok(input_read_buf, " "))) {
                count++;
                while((s = strtok(NULL, " "))) {
                        count++;
                        if(count == 3) {
                                printf("******************************%s***********************************\n",s);
				fflush(stdout);
			}
                        else if(count == 4) {
                                s[10] = '\0';
                                printf("%s\n",s);
				fflush(stdout);
                                sprintf(sprint_buf,"addr2line -e %s %s\0",vmlinux_path, s);
                                system(sprint_buf);
                        }

                }
        }
	
}

int parse_trace(FILE *input_fp, FILE *output_fp, char* input_read_buf)
{
	char *s;
	char sprint_buf[150];
	int count;
	unsigned long offset;
	int loop_count = 0;
	int curr_position = 0;
	int loop_till = 0;
	unsigned long val;
	int i;

	curr_position = ftell(input_fp);
	
	/*Crude method. Find a neat way some time*/
	do{
		loop_till++;
		count = 0;
		if(!(fgets(input_read_buf, MAX_INPUT_READ_BUF_SIZE, input_fp))) {
			INPUT_FP_ERR(5)
		} else {
			if((s = strtok(input_read_buf, " "))) {
				count++;
				while((s = strtok(NULL, " "))) {
					count++;
				}
			}
		}
	} while(count == 11);
	
        if(fseek(input_fp, curr_position, SEEK_SET)) {
                GEN_ERR("Error setting the input file position\n");
        }

        for(loop_count = 0; loop_count < (loop_till - 1); loop_count++) {
                count = 0;
                if(!(fgets(input_read_buf, MAX_INPUT_READ_BUF_SIZE, input_fp))) {
                        INPUT_FP_ERR(2)
                } else {
                        if((s = strtok(input_read_buf, " "))) {
                                count++;
                                while((s = strtok(NULL, " "))) {
                                        count++;
                                        if(count == 3) {
                                                s[4] = 0;
                                                sprintf(sprint_buf,"0x%s",s);
                                                offset = strtoul(sprint_buf, NULL, 0);
                                        }
                                        if((count > 3) && (count < 12)) {
                                                s[8] = '\0';
                                                printf("offset->%x,val->0x%s\n",offset + ((count - 4)*4),s);
						fflush(stdout);
						if(virtual_mem_layout_found) {
							sprintf(sprint_buf,"0x%s",s);
							val = strtoul(sprint_buf, NULL, 0);
							for(i=0; i < number_of_sections; i++) {
								if((val >= virt_mem_layout[i].start) && (val <= virt_mem_layout[i].end)) {
									if(!strncmp(virt_mem_layout[i].name,".text",5)) {
										sprintf(sprint_buf,"addr2line -e %s %s\0",vmlinux_path, s);
										system(sprint_buf);
									} else {
										printf("Either a value or pointer from %s\n", virt_mem_layout[i].name);
										fflush(stdout);
									}
								}
							}
						} else {
							sprintf(sprint_buf,"addr2line -e %s %s\0",vmlinux_path, s);
							system(sprint_buf);
						}
                                        }
                                }
                        }
                }
        }

	return 0;
}

int start_oops_parsing(FILE *input_fp, FILE* output_fp, char* input_read_buf, int curr_position)
{
	char* org = input_read_buf;
	int ret = 0;

	while(ret >= 0) {
		/*Parses headers like PC:, LR:, R1: etc*/
		ret = parse_header(input_read_buf, input_fp, output_fp, curr_position);
		if(ret == 1) {
			curr_position = ftell(input_fp);
			if(!(fgets(input_read_buf, MAX_INPUT_READ_BUF_SIZE, input_fp))) {
				INPUT_FP_ERR(3);
			}
			continue;
		} else if(ret < 0)
			break;

		/*Parse the addresses in the trace*/
		if(parse_trace(input_fp, output_fp, input_read_buf))
			return -1;
	}

	return 0;
}
