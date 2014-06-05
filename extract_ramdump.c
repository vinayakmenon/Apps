/*
 * Ramdump extractor.
 * Generates debug information if binary RAM image
 * and System.map is given as input.
 * Author: Vinayak Menon <vinayakm.list@gmail.com>
 */

#include <stdio.h>
#include <getopt.h>
#include <windows.h>


#define VERSION "1.1"

/*****FOR MEMINFO********/

enum zone_stat_item {
        NR_FREE_PAGES,
        NR_LRU_BASE,
        NR_INACTIVE_ANON = NR_LRU_BASE,
        NR_ACTIVE_ANON,
        NR_INACTIVE_FILE,
        NR_ACTIVE_FILE,
        NR_UNEVICTABLE,
        NR_MLOCK,
        NR_ANON_PAGES,
        NR_FILE_MAPPED,

        NR_FILE_PAGES,
        NR_FILE_DIRTY,
        NR_WRITEBACK,
        NR_SLAB_RECLAIMABLE,
        NR_SLAB_UNRECLAIMABLE,
        NR_PAGETABLE,
        NR_KERNEL_STACK,

        NR_UNSTABLE_NFS,
        NR_BOUNCE,
        NR_VMSCAN_WRITE,
        NR_WRITEBACK_TEMP,
        NR_ISOLATED_ANON,
        NR_ISOLATED_FILE,
        NR_SHMEM,
        NR_DIRTIED,
        NR_WRITTEN,
 		NR_FREE_CMA_PAGES,
		NR_CMA_ANON,
		NR_CMA_FILE,
		NR_LRU_CMA_BASE,
		NR_CMA_INACTIVE_ANON = NR_LRU_CMA_BASE,
		NR_CMA_ACTIVE_ANON,
		NR_CMA_INACTIVE_FILE,
		NR_CMA_ACTIVE_FILE,
		NR_CMA_UNEVICTABLE,
		NR_CONTIG_PAGES,
        NR_ANON_TRANSPARENT_HUGEPAGES,
        NR_VM_ZONE_STAT_ITEMS };

#define LRU_BASE 0
#define LRU_ACTIVE 1
#define LRU_FILE 2

enum lru_list {
        LRU_INACTIVE_ANON = LRU_BASE,
        LRU_ACTIVE_ANON = LRU_BASE + LRU_ACTIVE,
        LRU_INACTIVE_FILE = LRU_BASE + LRU_FILE,
        LRU_ACTIVE_FILE = LRU_BASE + LRU_FILE + LRU_ACTIVE,
        LRU_UNEVICTABLE,
        NR_LRU_LISTS
};

#define VM_BUF_SIZE 31

/*****END OF MEMINFO DEFINES******/

#define __AC(X,Y)       (X##Y)
#define _AC(X,Y)        __AC(X,Y)

#define PAGE_SHIFT              12
#define PAGE_SIZE               (_AC(1,UL) << PAGE_SHIFT)
#define PAGE_MASK               (~(PAGE_SIZE-1))

#define RAM_START 0x82000000
#define PAGE_OFFSET 0xc0000000
#define PHYS_OFFSET 0x82000000

#define __phys_to_virt(x)       ((x) - PHYS_OFFSET + PAGE_OFFSET)
#define __virt_to_phys(x)       ((x) - PAGE_OFFSET + PHYS_OFFSET)
#define __va(x)                 ((unsigned int)__phys_to_virt((unsigned long)(x)))
#define __pa(x)                 __virt_to_phys((unsigned long)(x))

#define OFFSETOF_PRIO       0x18
#define OFFSETOF_STATICPRIO 0x1c
#define OFFSETOF_NORMALPRIO 0x20
#define OFFSETOF_TASKS      0x1c8UL
#define OFFSETOF_MM         0x1d0
#define OFFSETOF_COMM       0x2d4
#define OFFSETOF_RSSSTAT    0x13c
#define OFFSETOF_PID        0x1f8
#define OFFSETOF_TID        0x1fc
#define OFFSETOF_MINFLT     0x298
#define OFFSETOF_MAJFLT     0x29c
#define OFFSETOF_SIGNAL     0x30c
#define OFFSETOF_OOMADJ     0x1E4
#define OFFSETOF_THREADGROUP 0x250
#define OFFSETOF_KSTACK      0x4
#define OFFSETOF_PREEMPTCOUNT 0x4
#define OFFSETOF_KSTATIRQS    0x20

#define TASK_COMM_LEN 16

#define TASK_RUNNING            0
#define TASK_INTERRUPTIBLE      1
#define TASK_UNINTERRUPTIBLE    2
#define __TASK_STOPPED          4
#define __TASK_TRACED           8
#define TASK_DEAD               64
#define TASK_WAKEKILL           128
#define TASK_WAKING             256

#define NR_MM_COUNTERS 4

struct mm_rss_stat {
        unsigned long count[NR_MM_COUNTERS];
} mm_rss;

unsigned char* task_state[] = {
         "TASK_RUNNING",
         "TASK_INTERRUPTIBLE",
         "TASK_UNINTERRUPTIBLE",
         "TASK_STOPPED",
         "TASK_TRACED",
         "TASK_DEAD",
         "TASK_WAKEKILL",
         "TASK_WAKING"
};

FILE* ramdump_fp;
FILE* systemmap_fp;
FILE* output_fp;
FILE* output_stack_fp;

unsigned char* ramdump_file_path;
unsigned char* systemmap_file_path;
unsigned char* output_file_path = "./task.txt";
unsigned char* output_irq_file_path = "./irq_desc.txt";
unsigned char* output_meminfo_file_path = "./meminfo.txt";
unsigned char* output_smap_pgtbl_file_path = "./page_table_system_map.txt";
unsigned char* output_virt_layout_file_path = "./kernel_virtual_memory_layout.txt";
unsigned char* output_cache_chain_file_path = "./slabinfo_and_cache_chain.txt";
unsigned char* output_search_result_file_path = "./search_val.txt";

int Display_thread(unsigned int address);
int Extract_meminfo(void);
int Extract_virt_mem_layout(void);
int Extract_smap_pgtbl(void);
unsigned int do_pg_tbl_wlkthr_logical(unsigned int address);
int Validate_sections(void);
int do_virt_to_phy(unsigned int address);
int Decode_cache_chain_and_slab_info(void);
unsigned int do_pg_tbl_wlkthr_non_logical(unsigned int address);
void show_locations(unsigned int address);
int Extract_pagetypeinfo(void);
int Extract_buddyinfo(void);
int Extract_zoneinfo(void);
int Extract_node_uma(void);

void show_help(void)
{
	printf("extract_ramdump, version %s\n\n", VERSION);
        printf("Usage: extract_ramdump -r [path to ramdump file] -m [path to system map file] -o [path to output file]\n");
        printf("The output files will generated in the path given to -o\n");
        printf("To create System.map from vmlinux, do \"nm -n vmlinux | grep -v '\\( [aNUw] \\)\\|\\(__crc_\\)\\|\\( \\$[adt]\\)'\"\n");
        printf("options: r,m,v,a,s,o,h\n");
        printf("r : path to the ramdump file\n");
        printf("m : path to the system map file\n");
        printf("o : path to the output folder\n");
        printf("v : Along with r and m options, use -v, to validate sections (text,bss,data)\n");
        printf("a : Along with options r and m, a [virt addr], displays the physical address with all attributes\n");
        printf("s : searches for the word provided as argument, in the ramdump, and outputs the location in search_val.txt\n");
        printf("h : help\n");
        fflush(stdout);
}

int read_char_from_ramdump(FILE *ramdump_fp, unsigned int phy_offset, char *read_char)
{
		unsigned int curr_position = 0;

//Add some kind of error checking here.
		curr_position = (phy_offset - RAM_START);
		if(fseek(ramdump_fp, curr_position, 0)) {
				printf("%s: error setting the ramdump file position\n",__func__);
				return -1;
		}

		if(!(fread(read_char, 1, 1, ramdump_fp))) {
				printf("%s: error reading from ramdump file\n",__func__);
				return -1;
		}

		return 0;
}

int read_uchar_from_ramdump(FILE *ramdump_fp, unsigned int phy_offset, unsigned char *read_uchar)
{
		unsigned int curr_position = 0;

//Add some kind of error checking here.
		curr_position = (phy_offset - RAM_START);
		if(fseek(ramdump_fp, curr_position, 0)) {
				printf("%s: error setting the ramdump file position\n",__func__);
				return -1;
		}

		if(!(fread(read_uchar, 1, 1, ramdump_fp))) {
				printf("%s: error reading from ramdump file\n",__func__);
				return -1;
		}

		return 0;
}

int read_short_from_ramdump(FILE *ramdump_fp, unsigned int phy_offset, short *read_short)
{
		unsigned int curr_position = 0;

//Add some kind of error checking here.
		curr_position = (phy_offset - RAM_START);
		if(fseek(ramdump_fp, curr_position, 0)) {
				printf("%s: error setting the ramdump file position\n",__func__);
				return -1;
		}

		if(!(fread(read_short, 2, 1, ramdump_fp))) {
				printf("%s: error reading from ramdump file\n",__func__);
				return -1;
		}

		return 0;
}

int read_ushort_from_ramdump(FILE *ramdump_fp, unsigned int phy_offset, unsigned short *read_ushort)
{
		unsigned int curr_position = 0;

//Add some kind of error checking here.
		curr_position = (phy_offset - RAM_START);
		if(fseek(ramdump_fp, curr_position, 0)) {
				printf("%s: error setting the ramdump file position\n",__func__);
				return -1;
		}

		if(!(fread(read_ushort, 2, 1, ramdump_fp))) {
				printf("%s: error reading from ramdump file\n",__func__);
				return -1;
		}

		return 0;
}

int read_int_from_ramdump(FILE *ramdump_fp, unsigned int phy_offset, int *read_int)
{
		unsigned int curr_position = 0;

//Add some kind of error checking here.
		curr_position = (phy_offset - RAM_START);
		if(fseek(ramdump_fp, curr_position, 0)) {
				printf("%s: error setting the ramdump file position\n",__func__);
				return -1;
		}

		if(!(fread(read_int, 4, 1, ramdump_fp))) {
				printf("%s: error reading from ramdump file\n",__func__);
				return -1;
		}

		return 0;
}

int read_uint_from_ramdump(FILE *ramdump_fp, unsigned int phy_offset, unsigned int *read_uint)
{
		unsigned int curr_position = 0;

//Add some kind of error checking here.
		curr_position = (phy_offset - RAM_START);
		if(fseek(ramdump_fp, curr_position, 0)) {
				printf("%s: error setting the ramdump file position\n",__func__);
				return -1;
		}

		if(!(fread(read_uint, 4, 1, ramdump_fp))) {
				printf("%s: error reading from ramdump file\n",__func__);
				return -1;
		}

		return 0;
}

int read_buf_from_ramdump(FILE *ramdump_fp, unsigned int phy_offset, unsigned int bytes, char* buf)
{
		unsigned int curr_position = 0;

//Add some kind of error checking here.
		curr_position = (phy_offset - RAM_START);
		if(fseek(ramdump_fp, curr_position, 0)) {
				printf("%s: error setting the ramdump file position\n",__func__);
				return -1;
		}

		if(!(fread(buf, bytes, 1, ramdump_fp))) {
				printf("%s: error reading from ramdump file\n",__func__);
				return -1;
		}

		return 0;
}

unsigned int get_addr_from_smap(char* symbol_to_find, int size)
{
		unsigned char smap_buf[150];
		unsigned int address;
		char* symbol;

#define SYSMAP_LINE_SIZE 149

		//reset system map fp
		if(fseek(systemmap_fp, 0, 0)) {
				printf("Error setting the system map file position\n");
				return -1;
		}

        while(!feof(systemmap_fp) && !ferror(systemmap_fp)) {
               if(!(fgets(smap_buf, SYSMAP_LINE_SIZE, systemmap_fp))) {
                    printf("Error reading from system map file\n");
                    return 0;
               }
               address = strtoul(strtok(smap_buf," "), NULL, 16);
               while(symbol =  strtok(NULL," ")) {
                    if(!strncmp(symbol,symbol_to_find,size)) {
						  if (symbol[size] != '\n')
						  	continue;
                          return address;
					}
               }
        }

		//reset system map fp
		if(fseek(systemmap_fp, 0, 0)) {
				printf("Error setting the system map file position\n");
				return -1;
		}
}

int main(int argc, char *argv[])
{

        int rm_flag = 0;
        int sm_flag = 0;
        int validate_flag = 0;
        int virt_flag = 0;
        int c;
        unsigned int input_read_buf=0;
        unsigned int mm_start=0;
        unsigned int curr_position = 0;
        unsigned int init_proc_address;
        unsigned int proc;
        unsigned char comm_buf[TASK_COMM_LEN];
        int signed_read_buf=0;
        unsigned char kstack_name_buf[100];
        char* kstack_buf;
        int pid, tid;
        int count = 0;
        unsigned int kstack_start = 0;
        unsigned int address;
        unsigned int virtual_address;
        unsigned int search_flag = 0;
        unsigned int search_val = 0;
        unsigned char* working_directory = ".";

        while((c = getopt(argc, argv, ":r:m:a:s:o:vh")) != -1) {
                switch(c) {
                        case 'r':
                                ramdump_file_path = optarg;
                                rm_flag = 1;
                                break;
                        case 'm':
                                systemmap_file_path = optarg;
                                sm_flag = 1;
                                break;
                        case 'o':
                                working_directory = optarg;
                                break;
                        case 'v':
                        		validate_flag = 1;
                        		break;
                        case 'a':
                                virtual_address = strtoul(optarg, NULL, 16);
                                virt_flag = 1;
                                break;
                        case 's':
                                search_val = strtoul(optarg, NULL, 16);
                                search_flag = 1;
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

        if(!rm_flag) {
                printf("ramdump file path missing\n");
                show_help();
                exit(2);
        }

        if(!sm_flag) {
                printf("system map file path missing\n");
                show_help();
                exit(2);
        }

        SetCurrentDirectory(working_directory);

        ramdump_fp = fopen(ramdump_file_path, "rb");
        if(!ramdump_fp) {
                printf("Error opening the ramdump file %s\n", ramdump_file_path);
                return -1;
        }

        systemmap_fp = fopen(systemmap_file_path, "r");
        if(!systemmap_fp) {
                printf("Error opening the system map file %s\n", systemmap_file_path);
                return -1;
        }


		if (validate_flag) {
			Validate_sections();
			return 0;
		}

		if (virt_flag) {
			do_virt_to_phy(virtual_address);
			return 0;
		}

		if (search_flag) {
			show_locations(search_val);
			return 0;
		}
        output_fp = fopen(output_file_path, "w");
        if(!output_fp) {
                printf("Error opening the output file %s\n", output_file_path);
                return -1;
        }

        CreateDirectory ("kstacks_per_task", NULL);
        CreateDirectory ("cpu_context_per_task", NULL);

        init_proc_address = (unsigned int)get_addr_from_smap("init_task", 9);
        proc = init_proc_address;

        //printf("COMM\t\t\tPID\tTID\tSTATE\t\t\t\tFLAGS\tPRIO\tSTATIC PRIO\tNORMAL PRIO\tMM_RSS[0]\tMM_RSS[1]\tMM_RSS[2]\tMM_RSS[3]\n");
        fprintf(output_fp,"%20s%20s%8s%8s%26s%15s%8s%17s%17s%20s%20s%20s%20s%15s%15s%15s%15s%15s%15s\n","COMM", "TASK_STRUCT", "PID",
                            "TID","STATE","FLAGS","PRIO","STATIC_PRIO","NORMAL_PRIO","MM_RSS[0]","MM_RSS[1]",
                            "MM_RSS[2]","MM_RSS[3]","min_flt","maj_flt","oom_adj","kstack_start","kstack_end",
                            "preempt_count");
        do {
            //COMM
            if(!read_buf_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_COMM), TASK_COMM_LEN, comm_buf))
            	fprintf(output_fp,"\n\n%20s",comm_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

			//task_struct
            fprintf(output_fp,"%20x",proc);

            //PID
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_PID), &input_read_buf)) {
            	fprintf(output_fp,"%8d",input_read_buf);
            	pid = input_read_buf;
            } else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //TID
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_TID), &input_read_buf)) {
            	fprintf(output_fp,"%8d",input_read_buf);
            	tid = input_read_buf;
            } else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

#define OFFSETOF_STATE      0x0

            //STATE
            if(read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_STATE), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

#undef OFFSETOF_STATE

            switch(input_read_buf) {
                    case TASK_RUNNING:
                         fprintf(output_fp,"%26s",task_state[0]);
                         break;
                    case TASK_INTERRUPTIBLE:
                         fprintf(output_fp,"%26s",task_state[1]);
                         break;
                    case TASK_UNINTERRUPTIBLE:
                         fprintf(output_fp,"%26s",task_state[2]);
                         break;
                    case __TASK_STOPPED:
                         fprintf(output_fp,"%26s",task_state[3]);
                         break;
                    case __TASK_TRACED:
                         fprintf(output_fp,"%26s",task_state[4]);
                         break;
                    case TASK_DEAD:
                         fprintf(output_fp,"%26s",task_state[5]);
                         break;
                    case TASK_WAKEKILL:
                         fprintf(output_fp,"%26s",task_state[6]);
                         break;
                    case TASK_WAKING:
                         fprintf(output_fp,"%26s",task_state[7]);
                         break;
                    default:
                         fprintf(output_fp,"%26s","??");
                         break;
            }

#define OFFSETOF_FLAGS      0xc

            //FLAGS
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_FLAGS), &input_read_buf))
            	fprintf(output_fp,"%15x",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

#undef OFFSETOF_FLAGS

            //PRIO
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_PRIO), &input_read_buf))
            	fprintf(output_fp,"%8d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //STATICPRIO
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_STATICPRIO), &input_read_buf))
            	fprintf(output_fp,"%15d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //NORMALPRIO
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_NORMALPRIO), &input_read_buf))
            	fprintf(output_fp,"%15d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //MM
            if(read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_MM), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

            if(input_read_buf) {
                   	mm_start = input_read_buf;
                   	//RSS
            		if(!read_buf_from_ramdump(ramdump_fp, __pa(mm_start + OFFSETOF_RSSSTAT), sizeof(struct mm_rss_stat), (char*)&mm_rss))
            			fprintf(output_fp,"%20ld %20ld %20ld %20ld", mm_rss.count[0], mm_rss.count[1], mm_rss.count[2], mm_rss.count[3]);
            		else {
						printf("ERROR:%d",__LINE__);
						return -1;
					}
            } else {
                   fprintf(output_fp,"%20s %20s %20s %20s","N/A","N/A","N/A","N/A");
            }

            //min_flt
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_MINFLT), &input_read_buf))
            	fprintf(output_fp,"%15d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //maj_flt
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_MAJFLT), &input_read_buf))
            	fprintf(output_fp,"%15d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //signal
            if(read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_SIGNAL), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

            //oom_adj
            if(!read_int_from_ramdump(ramdump_fp, __pa(input_read_buf + OFFSETOF_OOMADJ), &signed_read_buf))
            	fprintf(output_fp,"%15d",signed_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //stack start
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_KSTACK), &input_read_buf))
            	fprintf(output_fp,"%15x",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

			kstack_start = input_read_buf;

            //end of kstack -> start + 8k
            fprintf(output_fp,"%15x",input_read_buf + 8192);

#define KSTACK_SIZE 8192

			for(count=0; count < TASK_COMM_LEN; count++) {
				if (((comm_buf[count] < 48) || ((comm_buf[count] > 57) && (comm_buf[count] < 65))
				|| ((comm_buf[count] > 90) && (comm_buf[count] < 97)) || (comm_buf[count] > 122)) && (comm_buf[count] != '\0'))
					comm_buf[count] = 95; //replace with '_'
			}

			sprintf(kstack_name_buf,"kstacks_per_task\\kstack_%d_%d_%s.bin",pid,tid,comm_buf);
        	output_stack_fp = fopen(kstack_name_buf, "wb");
        	if(!output_stack_fp) {
                printf("Error opening the output file for kstack file %s\n", kstack_name_buf);
                return -1;
        	}

			kstack_buf = (char*)malloc(KSTACK_SIZE);

        	if(read_buf_from_ramdump(ramdump_fp, __pa(input_read_buf), KSTACK_SIZE, kstack_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

			if(!(fwrite(kstack_buf, KSTACK_SIZE, 1, output_stack_fp))) {
				printf("%s: error writing kstack for %d,%d\n",pid,tid);
				return -1;
			}

			fclose(output_stack_fp);
			free(kstack_buf);

#undef KSTACK_SIZE

#define OFFSETOF_CPUCONTEXT 0x1c

			sprintf(kstack_name_buf,"cpu_context_per_task\\cpu_context_%d_%d_%s.txt",pid,tid,comm_buf);
        	output_stack_fp = fopen(kstack_name_buf, "w");
        	if(!output_stack_fp) {
                printf("Error opening the output file for cpu context file %s\n", kstack_name_buf);
                return -1;
        	}

//printf("0x%x,0x%x,0x%x\n",kstack_start,OFFSETOF_CPUCONTEXT,address);
        	for(count = 0; count < 10; count++) {

			            if(!read_uint_from_ramdump(ramdump_fp, __pa(kstack_start + OFFSETOF_CPUCONTEXT + (4 * count)), &input_read_buf)) {
			            	if (count < 6)
			            		fprintf(output_stack_fp,"r%d\t\t0x%x\n",count+4, input_read_buf);
			            	else if (count == 6)
			            		fprintf(output_stack_fp,"%s\t\t0x%x\n","sl", input_read_buf);
			            	else if (count == 7)
			            		fprintf(output_stack_fp,"%s\t\t0x%x\n","fp", input_read_buf);
			            	else if (count == 8) {
			            		fprintf(output_stack_fp,"%s\t\t0x%x\n","sp", input_read_buf);
								if(read_uint_from_ramdump(ramdump_fp, __pa(input_read_buf), &input_read_buf)) {
									printf("ERROR:%d",__LINE__);
									//return -1;
								}
								fprintf(output_stack_fp,"%s\t\t0x%x\n","*sp", input_read_buf);
							}
			            	else if (count == 9)
			            		fprintf(output_stack_fp,"%s\t\t0x%x\n","pc", input_read_buf);

			            } else {
							printf("ERROR:%d",__LINE__);
							return -1;
						}
			}

			fclose(output_stack_fp);

#undef OFFSETOF_CPUCONTEXT

            //preempt_count
            if(!read_uint_from_ramdump(ramdump_fp, __pa(kstack_start + OFFSETOF_PREEMPTCOUNT), &input_read_buf))
            	fprintf(output_fp,"%15x",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //thread_group
            if(read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_THREADGROUP), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}


            //printf("0x%x,0x%x\n",proc,input_read_buf);
            if((input_read_buf - OFFSETOF_THREADGROUP) != proc) {
                    if(Display_thread(input_read_buf))
                          printf("Error parsing threads\n");

            }

            //task->next
            if(read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_TASKS), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}
            proc = (unsigned int)input_read_buf - OFFSETOF_TASKS; //see #define next_task(p) in kernel
            //printf("\nDB3\n");
            //printf("\nproc->0x%x\n",proc);
            //return 0;

        } while(init_proc_address != proc);

        //close the task file
        fclose(output_fp);

        if (Extract_irq_desc())
        	printf("Failed to extract irq desc..but continuing\n");

        if (Extract_meminfo())
        	printf("Failed to extract meminfo..but continuing\n");

        if (Extract_virt_mem_layout())
        	printf("Failed to virt kern mem layout..but continuing\n");

       	if (Extract_smap_pgtbl())
       		printf("Failed to extract smap pgtbl..but continuing\n");

       	if (Decode_cache_chain_and_slab_info())
       		printf("Failed to extract cache chain..but continuing\n");

       	if (Extract_node_uma())
       		printf("Failed to extract uma node..but continuing\n");

       	if (Extract_zoneinfo())
       		printf("Failed to extract zone info..but continuing\n");

       	if (Extract_buddyinfo())
       		printf("Failed to extract buddy info..but continuing\n");

       	if (Extract_pagetypeinfo())
       		printf("Failed to extract pagetype info..but continuing\n");

        fclose(ramdump_fp);

        fclose(systemmap_fp);

        fclose(output_fp);

        return 0;
}

int Extract_node_uma(void)
{
	unsigned char* output_node_uma_file_path = "./node_uma.txt";
	unsigned int address = 0;
	unsigned int input_read_buf=0;

#define ZONE_NAME_SIZE 7

	char name_buf[ZONE_NAME_SIZE + 1];
	int i;

	//reset system map fp
	if(fseek(systemmap_fp, 0, 0)) {
			printf("Error setting the system map file position\n");
			return -1;
	}

	output_fp = fopen(output_node_uma_file_path, "w");
	if(!output_fp) {
			printf("Error opening the output file for %s\n", output_node_uma_file_path);
			return -1;
	}

	fprintf(output_fp,"UMA Node (contig_page_data)\n");
	fprintf(output_fp,"---------------------------\n");


	address = get_addr_from_smap("contig_page_data", 16);

#define OFFSETOF_NRZONES 0x70c
#define OFFSETOF_NODESTARTPFN 0x71c
#define OFFSETOF_NODEPRESENTPAGES 0x720
#define OFFSETOF_NODESPANNEDPAGES 0x724
#define OFFSETOF_NODEID 0x728
#define OFFSETOF_KSWAPDMAXORDER 0x738
#define OFFSETOF_CLASSZONEIDX 0x73c

	//nr_zones
	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NRZONES), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"nr_zones: %d\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NODESTARTPFN), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"node_start_pfn: %d, node_start_address:0x%x\n", input_read_buf, input_read_buf << PAGE_SHIFT);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NODEPRESENTPAGES), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"node_present_pages: %d\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NODESPANNEDPAGES), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"node_spanned_pages: %d\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NODEID), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"node_id: %d\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_KSWAPDMAXORDER), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"kswapd_max_order: %d\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_CLASSZONEIDX), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"classzone_idx: %d\n", input_read_buf);

	fclose(output_fp);

	return 0;

}

int Extract_zoneinfo(void)
{

	unsigned char* output_buddy_info_file_path = "./zoneinfo.txt";
	unsigned int address = 0;
	unsigned int input_read_buf=0;

#define ZONE_NAME_SIZE 7
	char name_buf[ZONE_NAME_SIZE + 1];
	int i;

	//reset system map fp
	if(fseek(systemmap_fp, 0, 0)) {
			printf("Error setting the system map file position\n");
			return -1;
	}

	output_fp = fopen(output_buddy_info_file_path, "w");
	if(!output_fp) {
			printf("Error opening the output file for %s\n", output_buddy_info_file_path);
			return -1;
	}

	fprintf(output_fp,"Buddyinfo:\n");
	fprintf(output_fp,"----------\n");


	address = get_addr_from_smap("contig_page_data", 16);

#define OFFSETOF_NODEZONES 0x0
#define OFFSETOF_NAME 0x374
#define OFFSETOF_NODEID 0x728


	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NODEID), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"Node: %d\n", input_read_buf);

//Extract zone
#define OFFSETOF_WMARK_MIN 0x0
#define OFFSETOF_WMARK_LOW 0x4
#define OFFSETOF_WMARK_HIGH 0x8
#define OFFSETOF_PERCPU_DRIFT_MARK 0xc
#define OFFSETOF_LOWMEM_RESERVE_1 0x10
#define OFFSETOF_LOWMEM_RESERVE_2 0x14
#define OFFSETOF_ALL_UNRECLAIMABLE 0x1c
#define OFFSETOF_MIN_CMA_PAGES 0x20

	fprintf(output_fp,"ZONE INFO\n");
	fprintf(output_fp,"---------\n");

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NAME), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	//read the zone name
	if(read_buf_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(input_read_buf), ZONE_NAME_SIZE, name_buf)) {
		printf("ERROR:%d",__LINE__);
		return -1;
	}

	fprintf(output_fp,"ZONE: %s\n\n", name_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_WMARK_MIN), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"WMARK_MIN= %d\n\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_WMARK_LOW), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"WMARK_LOW= %d\n\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_WMARK_HIGH), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"WMARK_HIGH= %d\n\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_PERCPU_DRIFT_MARK), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"percpu_drift_mark= %d\n\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_LOWMEM_RESERVE_1), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"lowmem_reserve[0]= %d\n\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_LOWMEM_RESERVE_2), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"lowmem_reserve[1]= %d\n\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_ALL_UNRECLAIMABLE), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"all_unreclaimable= %d\n\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_MIN_CMA_PAGES), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"min_cma_pages= %d\n\n", input_read_buf);

#define MAX_ORDER 11
#define OFFSETOF_NRCMAFREE 0x24
#define OFFSETOF_COMPACT_CONSIDERED 0x290
#define OFFSETOF_COMPACT_DEFER_SHIFT 0x294

	for (i = 0; i < 11; i++) {
		if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NRCMAFREE + (4*i)), &input_read_buf)) {
			printf("ERROR:%d\n",__LINE__);
			return -1;
		}

		fprintf(output_fp,"nr_cma_free[order=%d]= %d\n\n", i, input_read_buf);
	}

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_COMPACT_CONSIDERED), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"compact_considered= %d\n\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_COMPACT_DEFER_SHIFT), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"compact_defer_shift= %d\n\n", input_read_buf);

#define OFFSETOF_PAGES_SCANNED 0x2cc

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_PAGES_SCANNED), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"pages_scanned= %d\n\n", input_read_buf);

#define OFFSETOF_FLAGS 0x2d0

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_FLAGS), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}
#undef OFFSETOF_FLAGS

	if (input_read_buf == 0)
		fprintf(output_fp,"flags= ZONE_RECLAIM_LOCKED\n\n");
	else if (input_read_buf == 1)
		fprintf(output_fp,"flags= ZONE_OOM_LOCKED\n\n");
	else if (input_read_buf == 2)
		fprintf(output_fp,"flags= ZONE_CONGESTED\n\n");
	else
		fprintf(output_fp,"flags= ZONE_NORMAL\n\n");

#define OFFSETOF_VMSTAT 0x2d4
//do for vmstat later, as at present meminfo gives the details.


//Curently we have 2 zones. Normal and Movable (unused). But we are going to worry here only about the Normal zone.

#define OFFSETOF_INACTIVE_RATIO 0x354

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_INACTIVE_RATIO), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"inactive_ratio= %d\n\n", input_read_buf);

#define OFFSETOF_PARENTNODE 0x364
#define OFFSETOF_ZONE_START_PFN 0x368
#define OFFSETOF_SPANNED_PAGES 0x36c
#define OFFSETOF_PRESENT_PAGES 0x370

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_PARENTNODE), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"parent node= 0x%x\n\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_ZONE_START_PFN), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"zone_start_pfn= %d, 0x%x\n\n", input_read_buf, (input_read_buf << PAGE_SHIFT));

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_SPANNED_PAGES), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"spanned_pages= %d\n\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_PRESENT_PAGES), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"present_pages= %d\n\n", input_read_buf);

	fprintf(output_fp,"----------------------\n");

	fclose(output_fp);

	return 0;

}

int Extract_buddyinfo(void)
{

	unsigned char* output_buddy_info_file_path = "./buddyinfo.txt";
	unsigned int address = 0;
	unsigned int input_read_buf=0;

#define ZONE_NAME_SIZE 7
	char name_buf[ZONE_NAME_SIZE + 1];
	int i;

	//reset system map fp
	if(fseek(systemmap_fp, 0, 0)) {
			printf("Error setting the system map file position\n");
			return -1;
	}

	output_fp = fopen(output_buddy_info_file_path, "w");
	if(!output_fp) {
			printf("Error opening the output file for %s\n", output_buddy_info_file_path);
			return -1;
	}

	fprintf(output_fp,"Buddyinfo:\n");
	fprintf(output_fp,"----------\n");


	address = get_addr_from_smap("contig_page_data", 16);

#define OFFSETOF_NODEZONES 0x0
#define OFFSETOF_NAME 0x374
#define OFFSETOF_NODEID 0x728


	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NODEID), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"Node: %d\n", input_read_buf);

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NAME), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	//read the zone name
	if(read_buf_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(input_read_buf), ZONE_NAME_SIZE, name_buf)) {
		printf("ERROR:%d",__LINE__);
		return -1;
	}

	fprintf(output_fp,"ZONE: %s\n\n", name_buf);

	fprintf(output_fp,"%8s%8s\n","order","nr_free");
	fprintf(output_fp,"----------------------\n");

#define MAX_ORDER 11
#define OFFSETOF_NRFREE 0x80
#define OFFSETOF_NEXT_NRFEE 0x34

	for (i = 0; i < MAX_ORDER; ++i) {
//zone->free_area[order].nr_free

		if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NRFREE + (i * OFFSETOF_NEXT_NRFEE)), &input_read_buf)) {
			printf("ERROR:%d\n",__LINE__);
			return -1;
		}

		fprintf(output_fp,"%8d%8d\n", i, input_read_buf);
	}

	fprintf(output_fp,"----------------------\n");

	fclose(output_fp);

	return 0;

}

#define MIGRATE_TYPES 6

static char * const migratetype_names[MIGRATE_TYPES] = {
        "Unmovable",
        "Reclaimable",
        "Movable",
        "Reserve",
        "CMA",
        "Isolate",
};

int Extract_pagetypeinfo(void)
{
	unsigned char* output_pagetype_info_file_path = "./pagetypeinfo.txt";
	unsigned int address = 0;
	unsigned int input_read_buf=0;

#define ZONE_NAME_SIZE 7
	char name_buf[ZONE_NAME_SIZE + 1];
	unsigned int i, j;
	unsigned int freecount = 0;
	unsigned int head;
	unsigned int start_pfn, end_pfn;
	unsigned int count[MIGRATE_TYPES] = { 0, };

#define ARCH_PFN_OFFSET 0
#define __pfn_to_page(pfn,mem_map)      (mem_map + ((pfn) - ARCH_PFN_OFFSET))

	//reset system map fp
	if(fseek(systemmap_fp, 0, 0)) {
			printf("Error setting the system map file position\n");
			return -1;
	}

	output_fp = fopen(output_pagetype_info_file_path, "w");
	if(!output_fp) {
			printf("Error opening the output file for %s\n", output_pagetype_info_file_path);
			return -1;
	}

	fprintf(output_fp,"Pagetypeinfo:\n");
	fprintf(output_fp,"-------------\n");


	address = get_addr_from_smap("contig_page_data", 16);

#define OFFSETOF_NODEZONES 0x0
#define OFFSETOF_NAME 0x374
#define OFFSETOF_NODEID 0x728
#define MAX_ORDER 11

	fprintf(output_fp,"%-43s ","Free pages count per migrate type at order");

	for (i = 0; i < MAX_ORDER; ++i) {
		fprintf(output_fp,"%6d", i);
	}

	fprintf(output_fp,"\n");

	for (i=0; i < MIGRATE_TYPES ; ++i) {

		if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NODEID), &input_read_buf)) {
			printf("ERROR:%d\n",__LINE__);
			return -1;
		}

		fprintf(output_fp,"Node %4d, ", input_read_buf);

		if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_NAME), &input_read_buf)) {
			printf("ERROR:%d\n",__LINE__);
			return -1;
		}

#undef OFFSETOF_NAME
		//read the zone name
		if(read_buf_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(input_read_buf), ZONE_NAME_SIZE, name_buf)) {
			printf("ERROR:%d",__LINE__);
			return -1;
		}

		fprintf(output_fp,"zone %8s, ", name_buf);

		fprintf(output_fp,"type %12s ", migratetype_names[i]);

#define OFFSETOF_FREELIST 0x50
#define OFFSETOF_NEXT_FREELIST 0x34
#define OFFSETOF_NEXT_NEXT 0x08

		for (j=0; j < MAX_ORDER; ++j) {

			freecount = 0;

			if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_FREELIST + (j * OFFSETOF_NEXT_FREELIST) + (i * OFFSETOF_NEXT_NEXT)), &input_read_buf)) {
				printf("ERROR:%d\n",__LINE__);
				return -1;
			}

			head = input_read_buf;

			if (!head) {
				fprintf(output_fp,"%7lu ", freecount);
				fprintf(output_fp,"\n");
				continue;
			}

			if(read_uint_from_ramdump(ramdump_fp, __pa(input_read_buf), &input_read_buf)) {
				printf("ERROR:%d\n",__LINE__);
				return -1;
			}

			while(head != input_read_buf) {

//zone->free_area->free_list[MIGRATE_TPE]
				if(read_uint_from_ramdump(ramdump_fp, __pa(input_read_buf), &input_read_buf)) {
					printf("ERROR:%d\n",__LINE__);
					return -1;
				}
				freecount++;

			}

			fprintf(output_fp,"%5lu ", freecount);
		}

		fprintf(output_fp,"\n");
	}


	fprintf(output_fp,"----------------------\n");

//Assuming even holes have valid backing memmap.

	fclose(output_fp);

	return 0;
}

void show_locations(unsigned int address)
{
	int i = 0;
	unsigned int input_read_buf=0;

	output_fp = fopen(output_search_result_file_path, "w");
	if(!output_fp) {
			printf("Error opening the output file for %s\n", output_search_result_file_path);
			return;
	}

	while(!feof(ramdump_fp) && !ferror(ramdump_fp)) {
            if(read_uint_from_ramdump(ramdump_fp, RAM_START + i, &input_read_buf)) {
            	//printf("ERROR:%d",__LINE__);
            	return;
			}
			if (input_read_buf == address) {
			//	if (((RAM_START + i) % 64) == 52) {
					fprintf(output_fp,"0x%x\n",RAM_START + i);
			//	}
			}

			i = i+4;
	}

	fclose(output_fp);
}

int Decode_cache_chain_and_slab_info(void)
{
	unsigned int address = 0;
	unsigned int list_head = 0;
	unsigned int input_read_buf=0;
	unsigned int input_read_buf2=0;
	unsigned int temp;
	unsigned long active_objs = 0;
	unsigned long active_slabs = 0;
	unsigned long num_slabs = 0, free_objects = 0, shared_avail = 0;
	unsigned long num_objs;

#define KMEMCACHE_NAME_SIZE	20
	char name_buf[KMEMCACHE_NAME_SIZE + 1];

	//reset system map fp
	if(fseek(systemmap_fp, 0, 0)) {
			printf("Error setting the system map file position\n");
			return -1;
	}

	output_fp = fopen(output_cache_chain_file_path, "w");
	if(!output_fp) {
			printf("Error opening the output file for %s\n", output_cache_chain_file_path);
			return -1;
	}

	fprintf(output_fp,"Cache chain\n");
	fprintf(output_fp,"--------------\n");


	address = get_addr_from_smap("cache_chain", 11);

	list_head = address;
	fprintf(output_fp,"Cache chain address: 0x%x\n",address);
	fprintf(output_fp,"---------------------------------\n");

	//address first next of cache chain
	if(read_uint_from_ramdump(ramdump_fp, __pa(address), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	fprintf(output_fp,"--------------\n");

	address = input_read_buf;


	do {

		fprintf(output_fp,"next: 0x%x\n", address);

		//get the container i.e. struct kmem_cache from next.
#define OFFSETOF_KMEMCACHE_NEXT 0x44

		//kmemcache struct start of next
		address = address - OFFSETOF_KMEMCACHE_NEXT;

		fprintf(output_fp,"struct kmemcache address: 0x%x\n", address);

#define OFFSETOF_KEMEMCACHE_NAME 0x40

	//read the name address.
		if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_KEMEMCACHE_NAME), &input_read_buf)) {
			printf("ERROR:%d\n",__LINE__);
			return -1;
		}

		//read the name
		if(read_buf_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(input_read_buf), KMEMCACHE_NAME_SIZE, name_buf)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}
		else
			fprintf(output_fp,"name: %s\n", name_buf);

//slabinfo

#define OFFSETOF_NODELIST 0x4c

//l3
		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(address + OFFSETOF_NODELIST), &input_read_buf)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		if (!input_read_buf)
			goto next;

		temp = input_read_buf;

#define OFFSETOF_SLABSFULL 0x8

//slabs_full

		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(temp + OFFSETOF_SLABSFULL), &input_read_buf)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		temp = temp + OFFSETOF_SLABSFULL;

#define OFFSETOF_NUM 0x1c

		//parse through the slab full list
		while (temp != input_read_buf) {

			if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(address + OFFSETOF_NUM), &input_read_buf2)) {
				printf("ERROR:%d",__LINE__);
				//return -1;
			}

			active_objs += input_read_buf2;
			active_slabs++;

			if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(input_read_buf), &input_read_buf)) {
				printf("ERROR:%d",__LINE__);
				//return -1;
			}

		}

//slabs_partial

		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(address + OFFSETOF_NODELIST), &input_read_buf)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		if (!input_read_buf)
			goto next;

		temp = input_read_buf;

#define OFFSETOF_SLABSPARTIAL 0x0

//next
		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(temp + OFFSETOF_SLABSPARTIAL), &input_read_buf)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		temp = temp + OFFSETOF_SLABSPARTIAL;

		//parse through the slab partial list
		while (temp != input_read_buf) {

//slab->inuse
#define OFFSETOF_INUSE 0x10

			if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(input_read_buf + OFFSETOF_INUSE), &input_read_buf2)) {
				printf("ERROR:%d",__LINE__);
				//return -1;
			}

			//active_objs += input_read_buf2;
			active_slabs++;

			if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(input_read_buf), &input_read_buf)) {
				printf("ERROR:%d",__LINE__);
				//return -1;
			}

		}

//slabs_free

		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(address + OFFSETOF_NODELIST), &input_read_buf)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		if (!input_read_buf)
			goto next;

		temp = input_read_buf;

#define OFFSETOF_SLABSFREE 0x10

		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(temp + OFFSETOF_SLABSFREE), &input_read_buf)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		temp = temp + OFFSETOF_SLABSFREE;

		//parse through the slab partial list
		while (temp != input_read_buf) {

			num_slabs++;

			if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(input_read_buf), &input_read_buf)) {
				printf("ERROR:%d",__LINE__);
				//return -1;
			}

		}

#define OFFSETOF_FREEOBJECTS 0x18

		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(address + OFFSETOF_NODELIST), &input_read_buf)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		if (!input_read_buf)
			goto next;

		temp = input_read_buf;

		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(temp + OFFSETOF_FREEOBJECTS), &input_read_buf)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		free_objects += input_read_buf;

#define OFFSETOF_SHARED 0x24

		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(temp + OFFSETOF_SHARED), &input_read_buf)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}
#undef OFFSETOF_SHARED

		if (input_read_buf) {

#define OFFSETOF_SHAREDAVAIL 0x0
//cachep->nodelist->shared->avail

				if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(input_read_buf + OFFSETOF_SHAREDAVAIL), &input_read_buf)) {
					printf("ERROR:%d",__LINE__);
					//return -1;
				}

				shared_avail += input_read_buf;
		}

		num_slabs += active_slabs;

		//cachep->num
		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(address + OFFSETOF_NUM), &input_read_buf2)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		fprintf(output_fp,"\n\nSlabinfo:\n");

		num_objs = num_slabs * input_read_buf2;

		fprintf(output_fp,"active_objs: %d\n",active_objs);
		fprintf(output_fp,"num_objs: %d\n",num_objs);
		fprintf(output_fp,"cache->num: %d\n",input_read_buf2);

#define OFFSETOF_BUFFERSIZE 0x10

		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(address + OFFSETOF_BUFFERSIZE), &input_read_buf2)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		fprintf(output_fp,"cache->buffer_size: %d\n",input_read_buf2);

#define OFFSETOF_LIMIT 0x08
#define OFFSETOF_BATCHCOUNT 0x04
#define OFFSETOF_SHARED 0x0c

		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(address + OFFSETOF_LIMIT), &input_read_buf2)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		fprintf(output_fp,"cache->limit: %d\n",input_read_buf2);

		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(address + OFFSETOF_BATCHCOUNT), &input_read_buf2)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}

		fprintf(output_fp,"cache->batchcount: %d\n",input_read_buf2);

		if(read_uint_from_ramdump(ramdump_fp, do_pg_tbl_wlkthr_non_logical(address + OFFSETOF_SHARED), &input_read_buf2)) {
			printf("ERROR:%d",__LINE__);
			//return -1;
		}
#undef OFFSETOF_SHARED

		fprintf(output_fp,"cache->shared: %d\n",input_read_buf2);

		fprintf(output_fp,"active_slabs: %d\n",active_slabs);
		fprintf(output_fp,"num_slabs: %d\n",num_slabs);
		fprintf(output_fp,"shared_avail: %d\n",shared_avail);

next:
		//find next
		address = address + OFFSETOF_KMEMCACHE_NEXT;

		//read the name address.
		if(read_uint_from_ramdump(ramdump_fp, __pa(address), &input_read_buf)) {
			printf("ERROR:%d\n",__LINE__);
			return -1;
		}

		fprintf(output_fp,"---------------------------------\n");

		address = input_read_buf;

	} while(address != list_head);

	return 0;

}

int Extract_virt_mem_layout(void)
{
	//reset system map fp
	if(fseek(systemmap_fp, 0, 0)) {
			printf("Error setting the system map file position\n");
			return -1;
	}

	output_fp = fopen(output_virt_layout_file_path, "w");
	if(!output_fp) {
			printf("Error opening the output file for %s\n", output_virt_layout_file_path);
			return -1;
	}

	fprintf(output_fp,"Virtual kernel memory layout\n");
	fprintf(output_fp,"............................\n");
	fprintf(output_fp,"Note: Those virtual memory ranges not listed here are macors in the kernel. Get it from there !!\n");

	fprintf(output_fp,".init : 0x%x - 0x%x\n",get_addr_from_smap("__init_begin", 12), get_addr_from_smap("__init_end", 10));
	fprintf(output_fp,".text : 0x%x - 0x%x\n",get_addr_from_smap("_text", 5), get_addr_from_smap("_etext", 6));
	fprintf(output_fp,".data : 0x%x - 0x%x\n",get_addr_from_smap("_sdata", 6), get_addr_from_smap("_edata", 6));
	fprintf(output_fp,".bss : 0x%x - 0x%x\n",get_addr_from_smap("__bss_start", 11), get_addr_from_smap("__bss_stop", 10));

	fclose(output_fp);

	return 0;
}

int do_virt_to_phy(unsigned int address)
{
	unsigned int input_read_buf=0;
	unsigned int pgd, pa_fld, pa_sld, pa, saved_fld;

	pgd = get_addr_from_smap("swapper_pg_dir", 14);
	pgd = __pa(pgd);

	printf("Virtual address: 0x%x\n",address);
	printf("PGD: 0x%x\n",pgd);

	pa_fld = (pgd & 0xFFFFC000);
	pa_fld |= ((address & 0xFFF00000) >> 18);

	printf("Level 1 descriptor: 0x%x\n",pa_fld);

	if(read_uint_from_ramdump(ramdump_fp, pa_fld, &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	printf("Content of Level 1 descriptor: 0x%x\n",input_read_buf);

	if (!(input_read_buf & 0x3)) { //[1:0]->00
		printf("Level 1 indicates FAULT\n");
		return 0;
	}

	if ((input_read_buf & 0x2) && (input_read_buf & 0x1)) {//[1:0]->11
		printf("Level 1 indicates RESERVED\n");
		return 0;
	}

	if ((input_read_buf & 0x2) && (!(input_read_buf & 0x1))) {//[1:0]->10, section or super section

		if (input_read_buf & (0x1 << 18)) {//super section
			printf("SUPER SECTION(16MB)\n");
			pa = (input_read_buf & 0xFF000000);
			pa |= (address & 0x0FFFFFF);
			printf("Physical address: 0x%x\n", pa);

			printf("Attributes:\n");
			printf("-----------\n");

			//shareable
			if (input_read_buf & (0x1 << 16))
				printf("Shareable\n");
			else
				printf("Non-Shareable\n");

			if (!(input_read_buf & (0x1 << 15))) {
				if ((!(input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
					printf("P:no access,U:no access\n");
				else if ((!(input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
					printf("P:R/W,U:no access\n");
				else if (((input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
					printf("P:R/W,U:RO\n");
				else if (((input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
					printf("P:R/W,U:R/W\n");
			} else {
				if ((!(input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
					printf("RESERVED\n");
				else if ((!(input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
					printf("P:RO,U:no access\n");
				else if (((input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
					printf("P:RO,U:RO\n");
				else if (((input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
					printf("P:RO,U:RO\n");
			}

			//XN
			if (input_read_buf & (0x1 << 4)) {
				printf("NO EXEC\n");
			} else {
				printf("EXEC\n");
			}

			//nG
			if (input_read_buf & (0x1 << 17)) {
				printf("nG set\n");
			} else {
				printf("nG not set\n");
			}

			//NS
			if (input_read_buf & (0x1 << 19)) {
				printf("non-secure, if secure page table\n");
			} else {
				printf("secure, if secure page table\n");
			}

			return 0;

		} else { //section
			printf("SECTION(1MB)\n");
			pa = (input_read_buf & 0xFFF00000);
			pa |= (address & 0x00FFFFF);
			printf("Physical address: 0x%x\n",pa);

			//shareable
			if (input_read_buf & (0x1 << 16))
				printf("Shareable\n");
			else
				printf("Non-Shareable\n");

			if (!(input_read_buf & (0x1 << 15))) {
				if ((!(input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
					printf("P:no access,U:no access\n");
				else if ((!(input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
					printf("P:R/W,U:no access\n");
				else if (((input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
					printf("P:R/W,U:RO\n");
				else if (((input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
					printf("P:R/W,U:R/W\n");
			} else {
				if ((!(input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
					printf("RESERVED\n");
				else if ((!(input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
					printf("P:RO,U:no access\n");
				else if (((input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
					printf("P:RO,U:RO\n");
				else if (((input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
					printf("P:RO,U:RO\n");
			}

			//XN
			if (input_read_buf & (0x1 << 4)) {
				printf("NO EXEC\n");
			} else {
				printf("EXEC\n");
			}

			//nG
			if (input_read_buf & (0x1 << 17)) {
				printf("nG set\n");
			} else {
				printf("nG not set\n");
			}

			//NS
			if (input_read_buf & (0x1 << 19)) {
				printf("non-secure, if secure page table\n");
			} else {
				printf("secure, if secure page table\n");
			}

			return 0;
		}

	}

	saved_fld = input_read_buf;

	//Large or small page
	pa_sld = (input_read_buf & 0xFFFFFC00);
	pa_sld |= ((address & 0x000FF000) >> 10);

	printf("Level 2 descriptor: 0x%x\n",pa_sld);

	if(read_uint_from_ramdump(ramdump_fp, pa_sld, &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	printf("Content of Level 2 descriptor: 0x%x\n",input_read_buf);

	if (input_read_buf & 0x2) { //Small page
		printf("SMALL PAGE(4k)\n");
		pa =  input_read_buf & 0xFFFFF000;
		pa |= address & 0x00000FFF;
	} else { //Large page
		printf("LARGE PAGE(64k)\n");
		pa =  input_read_buf & 0xFFFF0000;
		pa |= address & 0x0000FFFF;
	}

	printf("Physical address: 0x%x\n",pa);

	//shareable
	if (input_read_buf & (0x1 << 10))
		printf("Shareable\n");
	else
		printf("Non-Shareable\n");


	if (!(input_read_buf & (0x1 << 9))) {
		if ((!(input_read_buf & (0x1 << 5))) && (!(input_read_buf & (0x1 << 4))))
			printf("P:no access,U:no access\n");
		else if ((!(input_read_buf & (0x1 << 5))) && ((input_read_buf & (0x1 << 4))))
			printf("P:R/W,U:no access\n");
		else if (((input_read_buf & (0x1 << 5))) && (!(input_read_buf & (0x1 << 4))))
			printf("P:R/W,U:RO\n");
		else if (((input_read_buf & (0x1 << 5))) && ((input_read_buf & (0x1 << 4))))
			printf("P:R/W,U:R/W\n");
	} else {
		if ((!(input_read_buf & (0x1 << 5))) && (!(input_read_buf & (0x1 << 4))))
			printf("RESERVED\n");
		else if ((!(input_read_buf & (0x1 << 5))) && ((input_read_buf & (0x1 << 4))))
			printf("P:RO,U:no access\n");
		else if (((input_read_buf & (0x1 << 5))) && (!(input_read_buf & (0x1 << 4))))
			printf("P:RO,U:RO\n");
		else if (((input_read_buf & (0x1 << 5))) && ((input_read_buf & (0x1 << 4))))
			printf("P:RO,U:RO\n");
	}

	if (input_read_buf & 0x2) { //Small page
		//XN
		if (input_read_buf & (0x1 << 1)) {
			printf("NO EXEC\n");
		} else {
			printf("EXEC\n");
		}
	} else {
		//XN
		if (input_read_buf & (0x1 << 15)) {
			printf("NO EXEC\n");
		} else {
			printf("EXEC\n");
		}
	}

	//nG
	if (input_read_buf & (0x1 << 11)) {
		printf("nG set\n");
	} else {
		printf("nG not set\n");
	}


	//NS
	if (input_read_buf & (0x1 << 3)) {
		printf("non-secure, if secure page table\n");
	} else {
		printf("secure, if secure page table\n");
	}

	return 0;
}

int Validate_sections(void)
{
	unsigned int text_start = 0;
	unsigned int text_end = 0;
	unsigned int data_start = 0;
	unsigned int data_end = 0;
	unsigned int bss_start = 0;
	unsigned int bss_end = 0;
	unsigned int address = 0;

	//reset system map fp
	if(fseek(systemmap_fp, 0, 0)) {
			printf("Error setting the system map file position\n");
			return -1;
	}

	text_start = get_addr_from_smap("_text", 5);
	text_end = get_addr_from_smap("_etext", 6);
	data_start = get_addr_from_smap("_sdata", 6);
	data_end = get_addr_from_smap("_edata", 6);
	bss_start = get_addr_from_smap("__bss_start", 11);
	bss_end = get_addr_from_smap("__bss_stop", 10);

	printf("Validating text section...\n");

	for(address = text_start; address <= text_end; address = address + 4) {
		do_pg_tbl_wlkthr_logical(address);
	}

	printf("Validating data section...\n");

	for(address = data_start; address <= data_end; address = address + 4) {
		do_pg_tbl_wlkthr_logical(address);
	}

	printf("Validating bss section...\n");

	for(address = bss_start; address <= bss_end; address = address + 4) {
		do_pg_tbl_wlkthr_logical(address);
	}

	printf("Done...\n");

	return 0;
}

//for both logical and non-logical virtual addresses.
unsigned int do_pg_tbl_wlkthr_non_logical(unsigned int address)
{
	unsigned int input_read_buf=0;
	unsigned int pgd, pa_fld, pa_sld, pa, saved_fld;

	pgd = get_addr_from_smap("swapper_pg_dir", 14);
	pgd = __pa(pgd);

	pa_fld = (pgd & 0xFFFFC000);
	pa_fld |= ((address & 0xFFF00000) >> 18);

	if(read_uint_from_ramdump(ramdump_fp, pa_fld, &input_read_buf)) {
		printf("ERROR detected in reading at address 0x%x:%d\n",address, __LINE__);
		return -1;
	}

	if (!(input_read_buf & 0x3)) { //[1:0]->00
		printf("FAULT detected in FLD of address 0x%x\n",address);
		return -1;
	}

	if ((input_read_buf & 0x2) && (input_read_buf & 0x1)) {//[1:0]->11
		printf("RESERVED detected in FLD of address 0x%x\n",address);
		return -1;
	}

	if ((input_read_buf & 0x2) && (!(input_read_buf & 0x1))) {//[1:0]->10, section or super section

		if (input_read_buf & (0x1 << 18)) {//super section

			pa = (input_read_buf & 0xFF000000);
			pa |= (address & 0x0FFFFFF);

			return pa;

		} else { //section

			pa = (input_read_buf & 0xFFF00000);
			pa |= (address & 0x00FFFFF);

			return pa;
		}

	}

	//Large or small page
	pa_sld = (input_read_buf & 0xFFFFFC00);
	pa_sld |= ((address & 0x000FF000) >> 10);

	if(read_uint_from_ramdump(ramdump_fp, pa_sld, &input_read_buf)) {
		printf("ERROR in reading for address 0x%x:%d\n",address, __LINE__);
		return -1;
	}

	if (input_read_buf & 0x2) { //Small page
		pa =  input_read_buf & 0xFFFFF000;
		pa |= address & 0x00000FFF;

	} else { //Large page
		pa =  input_read_buf & 0xFFFF0000;
		pa |= address & 0x0000FFFF;
	}

	return pa;

}

//use this function only for logical addresses.
unsigned int do_pg_tbl_wlkthr_logical(unsigned int address)
{
	unsigned int input_read_buf=0;
	unsigned int pgd, pa_fld, pa_sld, pa, saved_fld;

	pgd = get_addr_from_smap("swapper_pg_dir", 14);
	pgd = __pa(pgd);

	pa_fld = (pgd & 0xFFFFC000);
	pa_fld |= ((address & 0xFFF00000) >> 18);

	if(read_uint_from_ramdump(ramdump_fp, pa_fld, &input_read_buf)) {
		printf("ERROR detected in reading at address 0x%x:%d\n",address, __LINE__);
		return -1;
	}

	if (!(input_read_buf & 0x3)) { //[1:0]->00
		printf("FAULT detected in FLD of address 0x%x\n",address);
		return -1;
	}

	if ((input_read_buf & 0x2) && (input_read_buf & 0x1)) {//[1:0]->11
		printf("RESERVED detected in FLD of address 0x%x\n",address);
		return -1;
	}

	if ((input_read_buf & 0x2) && (!(input_read_buf & 0x1))) {//[1:0]->10, section or super section

		if (input_read_buf & (0x1 << 18)) {//super section

			pa = (input_read_buf & 0xFF000000);
			pa |= (address & 0x0FFFFFF);
			if (__pa(address) != pa) {
				printf("Physical address mismatch detected in super section at address 0x%x\n",address);
				return -1;
			}

			return pa;

		} else { //section

			pa = (input_read_buf & 0xFFF00000);
			pa |= (address & 0x00FFFFF);

			if (__pa(address) != pa) {
				printf("Physical address mismatch detected in section at address 0x%x\n",address);
				return -1;
			}

			return pa;
		}

	}

	//Large or small page
	pa_sld = (input_read_buf & 0xFFFFFC00);
	pa_sld |= ((address & 0x000FF000) >> 10);

	if(read_uint_from_ramdump(ramdump_fp, pa_sld, &input_read_buf)) {
		printf("ERROR in reading for address 0x%x:%d\n",address, __LINE__);
		return -1;
	}

	if (input_read_buf & 0x2) { //Small page
		pa =  input_read_buf & 0xFFFFF000;
		pa |= address & 0x00000FFF;
		if (__pa(address) != pa) {
			printf("Physical address mismatch detected in small page at address 0x%x\n",address);
			return -1;
		}

	} else { //Large page
		pa =  input_read_buf & 0xFFFF0000;
		pa |= address & 0x0000FFFF;
		if (__pa(address) != pa) {
			printf("Physical address mismatch detected in large page at address 0x%x\n",address);
			return -1;
		}
	}

	return pa;

}

int Extract_smap_pgtbl(void)
{
	unsigned int address;
	unsigned char smap_buf[150];
	char* symbol;
	unsigned int input_read_buf=0;
	unsigned int pgd, pa_fld, pa_sld, pa, saved_fld;

#define SYSMAP_LINE_SIZE 149

	//reset system map fp
	if(fseek(systemmap_fp, 0, 0)) {
			printf("Error setting the system map file position\n");
			return -1;
	}

	output_fp = fopen(output_smap_pgtbl_file_path, "w");
	if(!output_fp) {
			printf("Error opening the output file for smap pgtbl%s\n", output_smap_pgtbl_file_path);
			return -1;
	}

	fprintf(output_fp,"%20s%20s%20s%20s%20s%20s%20s%20s%20s%20s%20s%20s%20s\n","VA", "PGD", "FLD",
	"*FLD", "MEM_SECTION", "SLD", "*SLD", "PA", "STATUS", "SHAREABLE",
	"PERMISSION", "EXEC", "GLOBAL");

	pgd = get_addr_from_smap("swapper_pg_dir", 14);
	pgd = __pa(pgd);

	//reset system map fp
	if(fseek(systemmap_fp, 0, 0)) {
			printf("Error setting the system map file position\n");
			return -1;
	}

    while(!feof(systemmap_fp) && !ferror(systemmap_fp)) {

	   if(!(fgets(smap_buf, SYSMAP_LINE_SIZE, systemmap_fp))) {
			return 0;
	   }
	   address = strtoul(strtok(smap_buf," "), NULL, 16);
	   while(symbol =  strtok(NULL," ")) {
			if(symbol[0] == '$')
				  address = 9999; //invalid
	   }

	   if (address < 0xc0000000)
	   		continue;

	   if (address != 9999) {

	   		fprintf(output_fp,"%20x",address);
	   		fprintf(output_fp,"%20x",pgd);

	   		pa_fld = (pgd & 0xFFFFC000);
	   		pa_fld |= ((address & 0xFFF00000) >> 18);

	   		fprintf(output_fp,"%20x",pa_fld);

            if(read_uint_from_ramdump(ramdump_fp, pa_fld, &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

			fprintf(output_fp,"%20x",input_read_buf);

			if (!(input_read_buf & 0x3)) { //[1:0]->00
				fprintf(output_fp,"%20s","FAULT");
				continue;
			}

			if ((input_read_buf & 0x2) && (input_read_buf & 0x1)) {//[1:0]->11
				fprintf(output_fp,"%20s","RESERVED");
				continue;
			}

			if ((input_read_buf & 0x2) && (!(input_read_buf & 0x1))) {//[1:0]->10, section or super section

				if (input_read_buf & (0x1 << 18)) {//super section
					fprintf(output_fp,"%20s","SUPER SECTION(16MB)");
					fprintf(output_fp,"%20s","NA");
					fprintf(output_fp,"%20s","NA");
					pa = (input_read_buf & 0xFF000000);
					pa |= (address & 0x0FFFFFF);
					if (__pa(address) != pa)
						fprintf(output_fp,"%20s\n","ERROR");
					else
						fprintf(output_fp,"%20s\n","OK");

					//shareable
					if (input_read_buf & (0x1 << 16))
						fprintf(output_fp,"%20s","yes");
					else
						fprintf(output_fp,"%20s","no");

					if (!(input_read_buf & (0x1 << 15))) {
						if ((!(input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:no access,U:no access");
						else if ((!(input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:R/W,U:no access");
						else if (((input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:R/W,U:RO");
						else if (((input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:R/W,U:R/W");
					} else {
						if ((!(input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","RESERVED");
						else if ((!(input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:RO,U:no access");
						else if (((input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:RO,U:RO");
						else if (((input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:RO,U:RO");
					}

					//XN
					if (input_read_buf & (0x1 << 4)) {
						fprintf(output_fp,"%20s","NO");
					} else {
						fprintf(output_fp,"%20s","YES");
					}

					//nG
					if (input_read_buf & (0x1 << 17)) {
						fprintf(output_fp,"%20s\n","NO");
					} else {
						fprintf(output_fp,"%20\ns","YES");
					}

#if 0
					//NS
					if (input_read_buf & (0x1 << 19)) {
						fprintf(output_fp,"%20s\n","non-secure");
					} else {
						fprintf(output_fp,"%20s\n","secure");
					}
#endif

					continue;

				} else { //section
					fprintf(output_fp,"%20s","SECTION(1MB)");
					pa = (input_read_buf & 0xFFF00000);
					pa |= (address & 0x00FFFFF);
					fprintf(output_fp,"%20s","NA");
					fprintf(output_fp,"%20s","NA");
					fprintf(output_fp,"%20x",pa);
					if (__pa(address) != pa)
						fprintf(output_fp,"%20s","ERROR");
					else
						fprintf(output_fp,"%20s","OK");

					//shareable
					if (input_read_buf & (0x1 << 16))
						fprintf(output_fp,"%20s","yes");
					else
						fprintf(output_fp,"%20s","no");

					if (!(input_read_buf & (0x1 << 15))) {
						if ((!(input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:no access,U:no access");
						else if ((!(input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:R/W,U:no access");
						else if (((input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:R/W,U:RO");
						else if (((input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:R/W,U:R/W");
					} else {
						if ((!(input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","RESERVED");
						else if ((!(input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:RO,U:no access");
						else if (((input_read_buf & (0x1 << 11))) && (!(input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:RO,U:RO");
						else if (((input_read_buf & (0x1 << 11))) && ((input_read_buf & (0x1 << 10))))
							fprintf(output_fp,"%20s","P:RO,U:RO");
					}

					//XN
					if (input_read_buf & (0x1 << 4)) {
						fprintf(output_fp,"%20s","NO");
					} else {
						fprintf(output_fp,"%20s","YES");
					}

					//nG
					if (input_read_buf & (0x1 << 17)) {
						fprintf(output_fp,"%20s\n","NO");
					} else {
						fprintf(output_fp,"%20s\n","YES");
					}

#if 0
					//NS
					if (input_read_buf & (0x1 << 19)) {
						fprintf(output_fp,"%20s\n","non-secure");
					} else {
						fprintf(output_fp,"%20s\n","secure");
					}
#endif
					continue;
				}

			}

			saved_fld = input_read_buf;

			//Large or small page
			pa_sld = (input_read_buf & 0xFFFFFC00);
			pa_sld |= ((address & 0x000FF000) >> 10);

			fprintf(output_fp,"%20x",pa_sld);

            if(read_uint_from_ramdump(ramdump_fp, pa_sld, &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

			fprintf(output_fp,"%20x",input_read_buf);

			if (input_read_buf & 0x2) { //Small page
				fprintf(output_fp,"%20s","SMALL PAGE(4k)");
				pa =  input_read_buf & 0xFFFFF000;
				pa |= address & 0x00000FFF;
			} else { //Large page
				fprintf(output_fp,"%20s","LARGE PAGE(64k)");
				pa =  input_read_buf & 0xFFFF0000;
				pa |= address & 0x0000FFFF;
			}
			fprintf(output_fp,"%20x",pa);
			if (__pa(address) != pa)
				fprintf(output_fp,"%20s","ERROR");
			else
				fprintf(output_fp,"%20s","OK");

					//shareable
					if (input_read_buf & (0x1 << 10))
						fprintf(output_fp,"%20s","yes");
					else
						fprintf(output_fp,"%20s","no");

					if (!(input_read_buf & (0x1 << 9))) {
						if ((!(input_read_buf & (0x1 << 5))) && (!(input_read_buf & (0x1 << 4))))
							fprintf(output_fp,"%20s","P:no access,U:no access");
						else if ((!(input_read_buf & (0x1 << 5))) && ((input_read_buf & (0x1 << 4))))
							fprintf(output_fp,"%20s","P:R/W,U:no access");
						else if (((input_read_buf & (0x1 << 5))) && (!(input_read_buf & (0x1 << 4))))
							fprintf(output_fp,"%20s","P:R/W,U:RO");
						else if (((input_read_buf & (0x1 << 5))) && ((input_read_buf & (0x1 << 4))))
							fprintf(output_fp,"%20s","P:R/W,U:R/W");
					} else {
						if ((!(input_read_buf & (0x1 << 5))) && (!(input_read_buf & (0x1 << 4))))
							fprintf(output_fp,"%20s","RESERVED");
						else if ((!(input_read_buf & (0x1 << 5))) && ((input_read_buf & (0x1 << 4))))
							fprintf(output_fp,"%20s","P:RO,U:no access");
						else if (((input_read_buf & (0x1 << 5))) && (!(input_read_buf & (0x1 << 4))))
							fprintf(output_fp,"%20s","P:RO,U:RO");
						else if (((input_read_buf & (0x1 << 5))) && ((input_read_buf & (0x1 << 4))))
							fprintf(output_fp,"%20s","P:RO,U:RO");
					}

					if (input_read_buf & 0x2) { //Small page
						//XN
						if (input_read_buf & 0x1) {
							fprintf(output_fp,"%20s","NO");
						} else {
							fprintf(output_fp,"%20s","YES");
						}
					} else {
						//XN
						if (input_read_buf & (0x1 << 15)) {
							fprintf(output_fp,"%20s","NO");
						} else {
							fprintf(output_fp,"%20s","YES");
						}
					}

					//nG
					if (input_read_buf & (0x1 << 11)) {
						fprintf(output_fp,"%20s\n","NO");
					} else {
						fprintf(output_fp,"%20s\n","YES");
					}

#if 0
					//NS
					if (saved_fld & (0x1 << 3)) {
						fprintf(output_fp,"%20s\n","non-secure");
					} else {
						fprintf(output_fp,"%20s\n","secure");
					}
#endif

	  		}
    }

	fclose(output_fp);

	return 0;
}

int Extract_irq_desc(void)
{

#define NO_OF_IRQS 492
#define IRQ_DESC_SIZE 0x60

		unsigned int address;
        int irqs = 0;
        unsigned int input_read_buf=0;
        unsigned int input_read_buf2=0;
        char name_buf[15];

        //reset system map fp
        if(fseek(systemmap_fp, 0, 0)) {
                printf("Error setting the system map file position\n");
                return -1;
        }

        output_fp = fopen(output_irq_file_path, "w");
        if(!output_fp) {
                printf("Error opening the output file for irq desc%s\n", output_irq_file_path);
                return -1;
        }

		address = get_addr_from_smap("irq_desc", 8);

		fprintf(output_fp,"%s","Bit masks for state_use_accessors\n");
		fprintf(output_fp,"%s","IRQD_TRIGGER_MASK               = 0xf\n");
		fprintf(output_fp,"%s","IRQD_SETAFFINITY_PENDING        = (1 <<  8)\n");
		fprintf(output_fp,"%s","IRQD_NO_BALANCING               = (1 << 10)\n");
		fprintf(output_fp,"%s","IRQD_PER_CPU                    = (1 << 11)\n");
		fprintf(output_fp,"%s","IRQD_AFFINITY_SET               = (1 << 12)\n");
		fprintf(output_fp,"%s","IRQD_LEVEL                      = (1 << 13)\n");
		fprintf(output_fp,"%s","IRQD_WAKEUP_STATE               = (1 << 14)\n");
		fprintf(output_fp,"%s","IRQD_MOVE_PCNTXT                = (1 << 15)\n");
		fprintf(output_fp,"%s","IRQD_IRQ_DISABLED               = (1 << 16)\n");
		fprintf(output_fp,"%s","IRQD_IRQ_MASKED                 = (1 << 17)\n");
		fprintf(output_fp,"%s","IRQD_IRQ_INPROGRESS             = (1 << 18)\n");
		fprintf(output_fp,"%s","-------------------------------------------\n");

        fprintf(output_fp,"%15s%15s%20s%15s%20s%20s\n","IRQ_NUMBER","KSTAT_IRQS", "STATE_USE_ACCESSORS", "CHIP-NAME", "HANDLER", "DEV_NAME");

#define OFFSETOF_SUA 0x8
#define OFFSETOF_CHIP 0x0c
#define OFFSETOF_ACTION 0x28
#define OFFSETOF_NAME 0x24

        for(irqs=0; irqs < NO_OF_IRQS; irqs++) {

             //irq
            if(!read_uint_from_ramdump(ramdump_fp, __pa(address + (irqs * IRQ_DESC_SIZE)), &input_read_buf))
            	fprintf(output_fp,"\n%15d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            if(read_uint_from_ramdump(ramdump_fp, __pa(address + (irqs * IRQ_DESC_SIZE) + OFFSETOF_KSTATIRQS), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

            //kstat_irqs
            if(!read_uint_from_ramdump(ramdump_fp, __pa(input_read_buf), &input_read_buf))
            	fprintf(output_fp,"%15d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //state_use_accessors
            if(!read_uint_from_ramdump(ramdump_fp, __pa(address + (irqs * IRQ_DESC_SIZE) + OFFSETOF_SUA), &input_read_buf))
            	fprintf(output_fp,"%20x",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            if(read_uint_from_ramdump(ramdump_fp, __pa(address + (irqs * IRQ_DESC_SIZE) + OFFSETOF_CHIP), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

            if(read_uint_from_ramdump(ramdump_fp, __pa(input_read_buf), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}
//printf("0x%x\n",input_read_buf);
            //irq_name
            if(!read_buf_from_ramdump(ramdump_fp, __pa(input_read_buf), 14, &name_buf))
            	fprintf(output_fp,"%15s",name_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            if(read_uint_from_ramdump(ramdump_fp, __pa(address + (irqs * IRQ_DESC_SIZE) + OFFSETOF_ACTION), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

			if(!input_read_buf) {
				fprintf(output_fp,"%20s","NA");
				fprintf(output_fp,"%20s\n","NA");
			} else {

			    if(!read_uint_from_ramdump(ramdump_fp, __pa(input_read_buf), &input_read_buf2))
            		fprintf(output_fp,"%20x",input_read_buf2);
            	else {
					printf("ERROR:%d",__LINE__);
					return -1;
				}

            	if(read_uint_from_ramdump(ramdump_fp, __pa(input_read_buf + OFFSETOF_NAME), &input_read_buf2)) {
            		printf("ERROR:%d",__LINE__);
            		return -1;
				}

//printf("0x%x,0x%x\n", input_read_buf2, __pa(input_read_buf2));
				if (__pa(input_read_buf2) < RAM_START) {
					fprintf(output_fp,"%20s\n","NA"); //dynamic allocation. change this later. Do a page table WT and get the name.
					continue;
				}

			    if(!read_buf_from_ramdump(ramdump_fp, __pa(input_read_buf2), 14, &name_buf))
            		fprintf(output_fp,"%20s\n",name_buf);
            	else {
					printf("ERROR:%d",__LINE__);
					return -1;
				}
			}
        }

        fclose(output_fp);
        return 0;
}

int Extract_meminfo(void)
{
		unsigned int address;
        int irqs = 0;
        unsigned int input_read_buf=0;
        unsigned long vm_buf[VM_BUF_SIZE];

        //reset system map fp
        if(fseek(systemmap_fp, 0, 0)) {
                printf("Error setting the system map file position\n");
                return -1;
        }

        output_fp = fopen(output_meminfo_file_path, "w");
        if(!output_fp) {
                printf("Error opening the output file for meminfo desc%s\n", output_file_path);
                return -1;
        }

		address = get_addr_from_smap("vm_stat", 7);
        if(read_buf_from_ramdump(ramdump_fp, __pa(address), (VM_BUF_SIZE * 4), vm_buf)) {
            printf("ERROR:%d",__LINE__);
            return -1;
		}

        fprintf(output_fp,"Meminfo:\n");
        fprintf(output_fp,"--------\n");
        fprintf(output_fp,"Memfree:%d pages --> %f Mb\n", vm_buf[0], ((float)vm_buf[0] * 4)/1024);
        fprintf(output_fp,"Buffers + Cached:%d pages --> %f Mb\n",
                        vm_buf[NR_FILE_PAGES],
                        ((float)vm_buf[NR_FILE_PAGES] * 4)/1024);
        fprintf(output_fp,"Active:%d pages --> %f Mb\n", vm_buf[NR_LRU_BASE + LRU_ACTIVE_ANON]
                        + vm_buf[NR_LRU_BASE + LRU_ACTIVE_FILE],
                        (((float)vm_buf[NR_LRU_BASE + LRU_ACTIVE_ANON]
                         + vm_buf[NR_LRU_BASE + LRU_ACTIVE_FILE]) * 4)/1024);
        fprintf(output_fp,"Inactive:%d pages --> %f Mb\n", vm_buf[NR_LRU_BASE + LRU_INACTIVE_ANON]
                        + vm_buf[NR_LRU_BASE + LRU_INACTIVE_FILE],
                        (((float)vm_buf[NR_LRU_BASE + LRU_INACTIVE_ANON]
                         + vm_buf[NR_LRU_BASE + LRU_INACTIVE_FILE]) * 4)/1024);
        fprintf(output_fp,"Active(anon):%d pages --> %f Mb\n",
                        vm_buf[NR_LRU_BASE + LRU_ACTIVE_ANON],
                        ((float)vm_buf[NR_LRU_BASE + LRU_ACTIVE_ANON] * 4)/1024);
        fprintf(output_fp,"Inactive(anon):%d pages --> %f Mb\n",
                        vm_buf[NR_LRU_BASE + LRU_INACTIVE_ANON],
                        ((float)vm_buf[NR_LRU_BASE + LRU_INACTIVE_ANON] * 4)/1024);
        fprintf(output_fp,"Active(file):%d pages --> %f Mb\n",
                        vm_buf[NR_LRU_BASE + LRU_ACTIVE_FILE],
                        ((float)vm_buf[NR_LRU_BASE + LRU_ACTIVE_FILE] * 4)/1024);
        fprintf(output_fp,"Inactive(file):%d pages --> %f Mb\n",
                        vm_buf[NR_LRU_BASE + LRU_INACTIVE_FILE],
                        ((float)vm_buf[NR_LRU_BASE + LRU_INACTIVE_FILE] * 4)/1024);
        fprintf(output_fp,"Unevictable:%d pages --> %f Mb\n",
                        vm_buf[NR_LRU_BASE + LRU_UNEVICTABLE],
                        ((float)vm_buf[NR_LRU_BASE + LRU_UNEVICTABLE] * 4)/1024);
        fprintf(output_fp,"Mlocked:%d pages --> %f Mb\n",
                        vm_buf[NR_MLOCK],
                        ((float)vm_buf[NR_MLOCK] * 4)/1024);
        fprintf(output_fp,"Dirty:%d pages --> %f Mb\n",
                        vm_buf[NR_FILE_DIRTY],
                        ((float)vm_buf[NR_FILE_DIRTY] * 4)/1024);
        fprintf(output_fp,"Writeback:%d pages --> %f Mb\n",
                        vm_buf[NR_WRITEBACK],
                        ((float)vm_buf[NR_WRITEBACK] * 4)/1024);
        fprintf(output_fp,"AnonPages:%d pages --> %f Mb\n",
                        vm_buf[NR_ANON_PAGES],
                        ((float)vm_buf[NR_ANON_PAGES] * 4)/1024);
        fprintf(output_fp,"Mapped:%d pages --> %f Mb\n",
                        vm_buf[NR_FILE_MAPPED],
                        ((float)vm_buf[NR_FILE_MAPPED] * 4)/1024);
        fprintf(output_fp,"Shmem:%d pages --> %f Mb\n",
                        vm_buf[NR_SHMEM],
                        ((float)vm_buf[NR_SHMEM] * 4)/1024);
        fprintf(output_fp,"Slab:%d pages --> %f Mb\n",
                        vm_buf[NR_SLAB_RECLAIMABLE] + vm_buf[NR_SLAB_UNRECLAIMABLE],
                        ((float)vm_buf[NR_SLAB_RECLAIMABLE] + vm_buf[NR_SLAB_UNRECLAIMABLE] * 4)/1024);
        fprintf(output_fp,"SReclaimable:%d pages --> %f Mb\n",
                        vm_buf[NR_SLAB_RECLAIMABLE],
                        ((float)vm_buf[NR_SLAB_RECLAIMABLE] * 4)/1024);
        fprintf(output_fp,"SUnreclaim:%d pages --> %f Mb\n",
                        vm_buf[NR_SLAB_UNRECLAIMABLE],
                        ((float)vm_buf[NR_SLAB_UNRECLAIMABLE] * 4)/1024);
#define THREAD_SIZE 8192
        fprintf(output_fp,"KernelStack:%d pages --> %f Mb\n",
                        vm_buf[NR_KERNEL_STACK] + THREAD_SIZE / 1024,
                        (((float)vm_buf[NR_KERNEL_STACK] + THREAD_SIZE / 1024) * 4)/1024);
        fprintf(output_fp,"PageTables:%d pages --> %f Mb\n",
                        vm_buf[NR_PAGETABLE],
                        ((float)vm_buf[NR_PAGETABLE] * 4)/1024);
        fprintf(output_fp,"Bounce:%d pages --> %f Mb\n",
                        vm_buf[NR_BOUNCE],
                        ((float)vm_buf[NR_BOUNCE] * 4)/1024);
        fprintf(output_fp,"WritebackTmp:%d pages --> %f Mb\n",
                        vm_buf[NR_WRITEBACK_TEMP],
                        ((float)vm_buf[NR_WRITEBACK_TEMP] * 4)/1024);
        fprintf(output_fp,"CmaFree:%d pages --> %f Mb\n",
                        vm_buf[NR_FREE_CMA_PAGES],
                        ((float)vm_buf[NR_FREE_CMA_PAGES] * 4)/1024);
        fprintf(output_fp,"CmaA(active):%d pages --> %f Mb\n",
                        vm_buf[NR_CMA_ACTIVE_ANON],
                        ((float)(vm_buf[NR_CMA_ACTIVE_ANON])* 4)/1024);
        fprintf(output_fp,"CmaA(inactive):%d pages --> %f Mb\n",
                        vm_buf[NR_CMA_INACTIVE_ANON],
                        ((float)vm_buf[NR_CMA_INACTIVE_ANON] * 4)/1024);
        fprintf(output_fp,"CmaF(active):%d pages --> %f Mb\n",
                        vm_buf[NR_CMA_ACTIVE_FILE],
                        ((float)vm_buf[NR_CMA_ACTIVE_FILE] * 4)/1024);
        fprintf(output_fp,"CmaF(inactive):%d pages --> %f Mb\n",
                        vm_buf[NR_CMA_INACTIVE_FILE],
                        ((float)vm_buf[NR_CMA_INACTIVE_FILE] * 4)/1024);
        fprintf(output_fp,"CmaUnevictable:%d pages --> %f Mb\n",
                        vm_buf[NR_CMA_UNEVICTABLE],
                        ((float)vm_buf[NR_CMA_UNEVICTABLE] * 4)/1024);
        fprintf(output_fp,"ContigAlloc:%d pages --> %f Mb\n",
                        vm_buf[NR_CONTIG_PAGES],
                        ((float)vm_buf[NR_CONTIG_PAGES] * 4)/1024);

        fprintf(output_fp,"------------------------------------------------\n");

        fclose(output_fp);

		return 0;
}

int Display_thread(unsigned int thread_add)
{
        unsigned int input_read_buf=0;
        unsigned int mm_start=0;
        unsigned int curr_position = 0;
        unsigned int init_proc_address;
        unsigned int proc;
        unsigned char comm_buf[TASK_COMM_LEN];
        char* symbol;
        int signed_read_buf=0;
        unsigned char kstack_name_buf[100];
        char* kstack_buf;
        int pid, tid;
        int count = 0;
        unsigned int kstack_start = 0;
		unsigned int address = 0;

        init_proc_address = thread_add -  OFFSETOF_THREADGROUP;
        proc = init_proc_address;
        fprintf(output_fp,"\nThread group----->");

        do {
            //COMM
            if(!read_buf_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_COMM), TASK_COMM_LEN, comm_buf))
            	fprintf(output_fp,"\n\n%20s",comm_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

			//task_struct
            fprintf(output_fp,"%20x",proc);

            //PID
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_PID), &input_read_buf)) {
            	fprintf(output_fp,"%8d",input_read_buf);
            	pid = input_read_buf;
            } else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //TID
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_TID), &input_read_buf)) {
            	fprintf(output_fp,"%8d",input_read_buf);
            	tid = input_read_buf;
            } else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

#define OFFSETOF_STATE      0x0

            //STATE
            if(read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_STATE), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

#undef OFFSETOF_STATE

            switch(input_read_buf) {
                    case TASK_RUNNING:
                         fprintf(output_fp,"%26s",task_state[0]);
                         break;
                    case TASK_INTERRUPTIBLE:
                         fprintf(output_fp,"%26s",task_state[1]);
                         break;
                    case TASK_UNINTERRUPTIBLE:
                         fprintf(output_fp,"%26s",task_state[2]);
                         break;
                    case __TASK_STOPPED:
                         fprintf(output_fp,"%26s",task_state[3]);
                         break;
                    case __TASK_TRACED:
                         fprintf(output_fp,"%26s",task_state[4]);
                         break;
                    case TASK_DEAD:
                         fprintf(output_fp,"%26s",task_state[5]);
                         break;
                    case TASK_WAKEKILL:
                         fprintf(output_fp,"%26s",task_state[6]);
                         break;
                    case TASK_WAKING:
                         fprintf(output_fp,"%26s",task_state[7]);
                         break;
                    default:
                         fprintf(output_fp,"%26s","??");
                         break;
            }

#define OFFSETOF_FLAGS      0xc
            //FLAGS
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_FLAGS), &input_read_buf))
            	fprintf(output_fp,"%15x",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}
#undef OFFSETOF_FLAGS

            //PRIO
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_PRIO), &input_read_buf))
            	fprintf(output_fp,"%8d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //STATICPRIO
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_STATICPRIO), &input_read_buf))
            	fprintf(output_fp,"%15d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //NORMALPRIO
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_NORMALPRIO), &input_read_buf))
            	fprintf(output_fp,"%15d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //MM
            if(read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_MM), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

            if(input_read_buf) {
                   	mm_start = input_read_buf;
                   	//RSS
            		if(!read_buf_from_ramdump(ramdump_fp, __pa(mm_start + OFFSETOF_RSSSTAT), sizeof(struct mm_rss_stat), &mm_rss))
            			fprintf(output_fp,"%20ld %20ld %20ld %20ld", mm_rss.count[0], mm_rss.count[1], mm_rss.count[2], mm_rss.count[3]);
            		else {
						printf("ERROR:%d",__LINE__);
						return -1;
					}
            } else {
                   fprintf(output_fp,"%20s %20s %20s %20s","N/A","N/A","N/A","N/A");
            }

            //min_flt
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_MINFLT), &input_read_buf))
            	fprintf(output_fp,"%15d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //maj_flt
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_MAJFLT), &input_read_buf))
            	fprintf(output_fp,"%15d",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //signal
            if(read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_SIGNAL), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

            //oom_adj
            if(!read_int_from_ramdump(ramdump_fp, __pa(input_read_buf + OFFSETOF_OOMADJ), &signed_read_buf))
            	fprintf(output_fp,"%15d",signed_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //stack start
            if(!read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_KSTACK), &input_read_buf))
            	fprintf(output_fp,"%15x",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

			kstack_start = input_read_buf;

            //end of kstack -> start + 8k
            fprintf(output_fp,"%15x",input_read_buf + 8192);

#define KSTACK_SIZE 8192

			for(count=0; count < TASK_COMM_LEN; count++) {
				if (((comm_buf[count] < 48) || ((comm_buf[count] > 57) && (comm_buf[count] < 65))
				|| ((comm_buf[count] > 90) && (comm_buf[count] < 97)) || (comm_buf[count] > 122)) && (comm_buf[count] != '\0'))
					comm_buf[count] = 95; //replace with '_'
			}

			sprintf(kstack_name_buf,"kstacks_per_task\\kstack_%d_%d_%s.bin",pid,tid,comm_buf);
        	output_stack_fp = fopen(kstack_name_buf, "wb");
        	if(!output_stack_fp) {
                printf("Error opening the output file for kstack file %s\n", kstack_name_buf);
                return -1;
        	}

			kstack_buf = (char*)malloc(KSTACK_SIZE);

        	if(read_buf_from_ramdump(ramdump_fp, __pa(kstack_start), KSTACK_SIZE, kstack_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}

			if(!(fwrite(kstack_buf, KSTACK_SIZE, 1, output_stack_fp))) {
				printf("%s: error writing kstack for %d,%d\n",pid,tid);
				return -1;
			}

			fclose(output_stack_fp);
			free(kstack_buf);

#undef KSTACK_SIZE

#define OFFSETOF_CPUCONTEXT 0x1c

			sprintf(kstack_name_buf,"cpu_context_per_task\\cpu_context_%d_%d_%s.txt",pid,tid,comm_buf);
        	output_stack_fp = fopen(kstack_name_buf, "w");
        	if(!output_stack_fp) {
                printf("Error opening the output file for cpu context file %s\n", kstack_name_buf);
                return -1;
        	}

        	for(count = 0; count < 10; count++) {

			            if(!read_uint_from_ramdump(ramdump_fp, __pa(kstack_start + OFFSETOF_CPUCONTEXT + (4 * count)), &input_read_buf)) {
			            	if (count < 6)
			            		fprintf(output_stack_fp,"r%d\t\t0x%x\n",count+4, input_read_buf);
			            	else if (count == 6)
			            		fprintf(output_stack_fp,"%s\t\t0x%x\n","sl", input_read_buf);
			            	else if (count == 7)
			            		fprintf(output_stack_fp,"%s\t\t0x%x\n","fp", input_read_buf);
			            	else if (count == 8)
			            		fprintf(output_stack_fp,"%s\t\t0x%x\n","sp", input_read_buf);
			            	else if (count == 9)
			            		fprintf(output_stack_fp,"%s\t\t0x%x\n","pc", input_read_buf);

			            } else {
							printf("ERROR:%d",__LINE__);
							return -1;
						}
			}

			fclose(output_stack_fp);

#undef OFFSETOF_CPUCONTEXT

            //preempt_count
            if(!read_uint_from_ramdump(ramdump_fp, __pa(kstack_start + OFFSETOF_PREEMPTCOUNT), &input_read_buf))
            	fprintf(output_fp,"%15x",input_read_buf);
            else {
				printf("ERROR:%d",__LINE__);
				return -1;
			}

            //task->next
            if(read_uint_from_ramdump(ramdump_fp, __pa(proc + OFFSETOF_THREADGROUP), &input_read_buf)) {
            	printf("ERROR:%d",__LINE__);
            	return -1;
			}
            proc = (unsigned int)input_read_buf - OFFSETOF_THREADGROUP; //see #define next_task(p) in kernel
            //printf("\nDB3\n");
            //printf("\nproc->0x%x\n",proc);
            //return 0;

        } while(init_proc_address != proc);

        return 0;
}
