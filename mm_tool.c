/*
 * 
 * MM tool - Takes binary RAM image and System.map
 * file as input and generates useful memory related
 * information in the form of files in the current
 * folder.
 *
 * Author: Vinayak.Menon <vinayakm.list@gmail.com>
 */

#include <stdio.h>
#include <getopt.h>
#include <windows.h>

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

#define TASK_COMM_LEN 16

FILE* ramdump_fp;
FILE* systemmap_fp;
FILE* output_fp;

unsigned char* ramdump_file_path;
unsigned char* systemmap_file_path;
unsigned char* output_meminfo_file_path = "./vmstat.txt";
unsigned char* output_smap_pgtbl_file_path = "./page_table_system_map.txt";
unsigned char* output_virt_layout_file_path = "./kernel_virtual_memory_layout.txt";
unsigned char* output_cache_chain_file_path = "./cache_chain.txt";

int Extract_vmstat(void);
int Extract_virt_mem_layout(void);
int Extract_smap_pgtbl(void);
unsigned int do_pg_tbl_wlkthr_logical(unsigned int address);
int do_virt_to_phy(unsigned int address);
int Decode_cache_chain(void);
unsigned int do_pg_tbl_wlkthr_non_logical(unsigned int address);

void show_help(void)
{
        printf("Usage: mm_tool -r [path to ramdump file] -m [path to system map file]\n");
        printf("The output files will generated in the current folder\n");
        printf("To create System.map from vmlinux, do \"nm vmlinux > System.map\"\n");
        printf("options: r,m,a\n");
        printf("r : path to the ramdump file\n");
        printf("m : path to the system map file\n");
        printf("a : Along with options r and m, a [virt addr], displays the physical address with all attributes\n");
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
        int virt_flag = 0;
        int c;
        unsigned int input_read_buf=0;
        unsigned int virtual_address;

        while((c = getopt(argc, argv, ":r:m:a:h:")) != -1) {
                switch(c) {
                        case 'r':
                                ramdump_file_path = optarg;
                                rm_flag = 1;
                                break;
                        case 'm':
                                systemmap_file_path = optarg;
                                sm_flag = 1;
                                break;
                        case 'a':
                                virtual_address = strtoul(optarg, NULL, 16);
                                virt_flag = 1;
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

		if (virt_flag) {
			do_virt_to_phy(virtual_address);
			return 0;
		}

        if (Extract_vmstat())
        	printf("Failed to extract meminfo..but continuing\n");

        if (Extract_virt_mem_layout())
        	printf("Failed to virt kern mem layout..but continuing\n");

       	if (Extract_smap_pgtbl())
       		printf("Failed to extract smap pgtbl..but continuing\n");

       	if (Decode_cache_chain())
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


/*
Node:
-----
In the case of NUMA, memory can be arranged into banks. Depending on the distance
from the processor, the cost of accessing these memory differs.
e.g. A bank of memory may be assigned to each CPU or a bank of memory situated near
devices for DMA.
Each of these bank is called a node.
A node is represented in Linux by the struct pglist_data (referenced by pg_data_t),
even if the arch is UMA.
In a NUMA system, each of the node is kept in a array, pgdta_list.
see arch/ia64/mm/discontig.c. for an example
Defined in include/linux/mmzone.h
and defined only if CONFIG_DISCONTIGMEM is set.

For UMA archs, only one pglist_data struct called contig_page_data is used. defined in mm/bootmem.c.

Definitions of the major elements of pglist_data.

node_zones-->The array that hold the data structures of the zones
in the node.
node_zonelists-->**COME BACK** The order of zones allocations are prefered from when space is
not available in the current zone. See build_zonelists() in mm/page_alloc.c.
The kernel always tries to allocate memory for a process, on the node associated with the CPU
were the process is currently running. However this is not always possible, because the node may be full.
In such situations kernel uses the fallback list provided by the node. This list is represented by
struct zonelist.The further back an entry is on the list, the less suitable it is.

node_mem_map-->pointer to an array of page instances used to describe all physical
pages of the node. It includes the pages of all zones in the node.

nr_zones-->Number of zones in this node. 1 <= nr_zones <= 3.

node_start_pfn-->starting PFN of the node. The PFN is global and thus it is NOT just unique to this
node. This value is NOT always zero in a UMA system. For example if kernel's RAM region starts at
0x82000000, the node_start_pfn will be 532480. (532480 * 4 * 1024 = 0x82000000).

node_present_pages-->total number of physical pages.Number of page frames in the zone.

node_spanned_pages-->total size of the physical page range, including holes.This need not be same as node_present_pages.
There can be holes in the zone that are not backed by a real page frame. **COME BACK to understand what this holes are,
and how they are formed.**

node_id-->id of the node.starts at zero.

classzone_idx-->ZONE TYPE (why this is there inside a node structure ?)
bdata-->During the system boot, the kernel needs memory even before memory management
has been initialized (memory must also be reserved to initialize memory management). to resolve this problem,
the kernel uses the boot memory allocator. bdata points to the instance of the data structure that characterizes
the boot memory allocator.

struct pglist_data __refdata contig_page_data = {
        .bdata = &bootmem_node_data[0]
};
EXPORT_SYMBOL(contig_page_data);

bootmem_data_t bootmem_node_data[MAX_NUMNODES] __initdata;

 *
 * node_bootmem_map is a map pointer - the bits represent all physical
 * memory pages (including holes) on the node.
 *
typedef struct bootmem_data {
        unsigned long node_min_pfn;
        unsigned long node_low_pfn;
        void *node_bootmem_map;
        unsigned long last_end_off;
        unsigned long hint_idx;
        struct list_head list;
} bootmem_data_t;


kswapd_wait-->wait quueue for the swap daemon needed when swapping frames out of the zone.kswapd points to
the task structure of the swap daemon responsible for the zone.

kswapd_max_order-->used in the implementation of the swapping subsystem to define the size of the area to be freed. ** COME BACK **.

ZONE_DMA: used when there are devices that are not able to do DMA to all
of addressable memory (ZONE_NORMAL).Then we carve out the portion of memory that
is needed for these devices. The range is arch specific.

ZONE_DMA32: Only for x86_64. because it supports devices that are
only able to do DMA to the lower 16M but also 32 bit devices that
can only do DMA areas below 4G.

ZONE_NORMAL:Normal addressable memory is in ZONE_NORMAL.
DMA operations can be performed on pages in ZONE_NORMAL if the DMA devices support
transfers to all addressable memory.

ZONE_HIGHMEM: A memory area that is only addressable by the kernel through
mapping portions into its own address space.This is for example
used by ARM to allow the kernel to address the memory beyond
arm_lowmem_limit. The kernel will set up special mappings
for each page that the kernel needs to access.

ZONE_MOVABLE:
**COME BACK** Required to to prevent fragmentation of physical memory.

*/


/*
Zone:
-----
Each node is divided into a number of blocks called zones.
Represents ranges within memory.
Each zone is represented by struct zone. (include/linux/mmzone.h)

enum zone_watermarks {
        WMARK_MIN,
        WMARK_LOW,
        WMARK_HIGH,
        NR_WMARK
};

watermark[NR_WMARK]-->zone watermarks
When available memory in the system is low, the pageout daemon kswapd
is woken up to start freeing pages. If the pressure is high, the process will free up memory
synchronously, sometimes refered to as direct reclaim path. The watermarks helps to track
how much pressure a zone is under.
WMARK_LOW-->When free pages reach this, kswapd is woken up by the buddy allocator to start
freeing pages. The value is twice the value of WMARK_MIN by default (**CHECK THIS**).
WMARK_MIN-->If this value is reached, the allocator will do kswapd work in a synchronous fashion,
known as direct reclaim path.
WMARK_HIGH-->Once kswapd is woken up, it will not consider the zone to be balanced until free pages reach this number.
Once it reaches, kswapd goes to sleep.

**CHEK THIS** Calculation of pages_min in free_area_init_core() during memory init.

percpu_drift_mark-->When free pages are below this point, additional steps are taken when reading the number of
free pages to avoid per-cpu counter drift allowing watermarks to be breached. ** COME BACK **

lowmem_reserve-->***COME BACK**.
Several pages for each memory zone, that are reserved for critical allocations that must not fail under any
circumstances. Each zone contributes  accordint to its importance. ** COME BACK to fill the algorithm used.**
        *
        * We don't know if the memory that we're going to allocate will be freeable
        * or/and it will be released eventually, so to avoid totally wasting several
        * GB of ram we must reserve some of the lower zone memory (otherwise we risk
        * to run OOM on the lower zones despite there's tons of freeable ram
        * on the higher zones). This array is recalculated at runtime if the
        * sysctl_lowmem_reserve_ratio sysctl changes.
        *

pageset-->an array to implement per-CPU hot-n-cold page lists. The kernel uses these lists to
store fresh pages that can be used to satisfy implementations. However, they are distinguished
by their cache status: Pages that are most likely still cache-hot and can therefore be quickly
accessed are separated from cache-cold pages. ** COME BACK **.


free_area-->Array of data structures of the same name used to implement the buddy system.
Each array stands for contiguous memory areas of fixed size.

A page is regarded as active by the kernel if it is accessed frequently. The distiction is used during a swap out.
if possible, frequently used pages should be kept intact, but superfluous inactive pages can be swapped out without
impunity.

**COME BACK** active and inactive lists are removed in the latest kernels.
enum lru_list {
        LRU_INACTIVE_ANON = LRU_BASE,
        LRU_ACTIVE_ANON = LRU_BASE + LRU_ACTIVE,
        LRU_INACTIVE_FILE = LRU_BASE + LRU_FILE,
        LRU_ACTIVE_FILE = LRU_BASE + LRU_FILE + LRU_ACTIVE,
        LRU_UNEVICTABLE,
        NR_LRU_LISTS
};
        struct zone_lru {
                struct list_head list;
        } lru[NR_LRU_LISTS];
This is how active, inactive, file etc lists are maintained in the latest kernel.

pages_scanned-->specifies how many pages were "unsuccessfully" ** CHECK THIS if it successful or unsuccesful ** scanned since the last time
a page was swapped out.

flags--> None of the below flags being set is the NORMAL condition of the zone.
typedef enum {
        ZONE_RECLAIM_LOCKED,            * prevents concurrent reclaim On SMP systems, multiple CPUs could be tempted to reclaim a zone concurrently.*
        								* This flag prevents that. If a CPU is reclaiming a zone, this flag is set.
        ZONE_OOM_LOCKED,                * zone is in OOM killer zonelist. The flag prevents multiple CPUs from getting into their way in this case. *
        ZONE_CONGESTED,                 * zone has many dirty pages backed by
                                        * a congested BDI
                                        *
} zone_flags_t;

all_unreclaimable-->state that can occur when the kernel tries to reuse some pages of the zone, but this is not possible at all because
all pages are pinned.For instance, a userspace application could have used the mlock system call to instruct the kernel that pages must
not be removed from physical memory,for example, by swapping them out.Such a page is said to be pinned.If all pages in a
zone suffer this fate, the zone is unreclaimable, and the flag is set. To waste no time, the swapping daemon scans zones of this kind very
briefly when it is looking for pages to reclaim.

vm_stat-->statistical information about the zone.

wait_table, wait_table_hash_nr_entries, wait_table_bits-->implement a wait queue for processes waiting for a page to become available.
Processes queue up in a line to wait for some condition. When this condition becomes true, they are notified by the kernel and can
resume their work.

		 *
         * wait_table           -- the array holding the hash table
         * wait_table_hash_nr_entries   -- the size of the hash table array
         * wait_table_bits      -- wait_table_size == (1 << wait_table_bits)
         *
         * The purpose of all these is to keep track of the people
         * waiting for a page to become available and make them
         * runnable again when possible. The trouble is that this
         * consumes a lot of space, especially when so few things
         * wait on pages at a given time. So instead of using
         * per-page waitqueues, we use a waitqueue hash table.
         *
         * The bucket discipline is to sleep on the same queue when
         * colliding and wake all in that wait queue when removing.
         * When something wakes, it must check to be sure its page is
         * truly available, a la thundering herd. The cost of a
         * collision is great, but given the expected load of the
         * table, they should be so rare as to be outweighed by the
         * benefits from the saved space.
         *
         * __wait_on_page_locked() and unlock_page() in mm/filemap.c, are the
         * primary users of these fields, and in mm/page_alloc.c
         * free_area_init_core() performs the initialization of them.
         *
** COME BACK **

zone_pgdat-->points to the corresponding instance of pg_list_data.

zone_start_pfn--> is the index of the first page frame of the zone.

name-->name of the zone. (Normal, DMA, Highmem).

spanned_pages-->total size, including holes

present_pages-->amount of memory (excluding holes)

inactive_ratio-->The target ratio of ACTIVE_ANON to INACTIVE_ANON pages on this zone's LRU.  Maintained by the pageout code. ** COME BACK **

compact_considered, compact_defer_shift-->if CONFIG_COMPACTION enabled.
         * On compaction failure, 1<<compact_defer_shift compactions
         * are skipped before trying again. The number attempted since
         * last failure is tracked with compact_considered.


**CMA items -- Check if they are really there in CMA patches or are they our modifications. COME BACK **

min_cma_pages-->
         * CMA needs to increase watermark levels during the allocation
         * process to make sure that the system is not starved.
nr_cma_free[MAX ORDER]-->
         * This tells us how many free MIGRATE_CMA pages exist on the free
         * list per-order. This will help us determine the watermark checks
         * so the reclaim happens faster if we are running out of
         * non-MIGRATE_CMA pages that kernel needs.

MAX_ORDER is not a CMA terminology, but the max order supported by buddy allocator.
it is 11 or value of CONFIG_FORCE_MAX_ZONEORDER.
*/

/*
Zone Watermarks
---------------
Before calculating watermarks, the kernel first determines the minimum memory space required for
critical allocations.
This value is nonlinear wrt the available RAM.
cal also be controlled through /proc/sys/vm/min_free_kbytes.

The watermarks are filled by init_per_zone_wmark_min() in mm/page_alloc.c.

min_free_kbytes = 4 * sqrt(lowmem_kbytes)
The reason for the formula are:
1) For small machines the value should be small, and for bigger ones we want it large.
2) But the relationship with size of RAM is not linear. Because as I understand, the usage of reserve memory
doesnt increase linearly with the RAM size. Say we have a RAM of 100GB, it doesnt mean the resources in the system
need a proportional reserve of memory.

int __meminit init_per_zone_wmark_min(void)
{
        unsigned long lowmem_kbytes;

		== lowmem_kbytes = nr_free_buffer_pages() * 4 ==
		== free pages are calculated as the sum of pages greater than high watermark for all zones in the zonelist. ==
        lowmem_kbytes = nr_free_buffer_pages() * (PAGE_SIZE >> 10);

        min_free_kbytes = int_sqrt(lowmem_kbytes * 16);
        if (min_free_kbytes < 128)
                min_free_kbytes = 128;
        if (min_free_kbytes > 65536)
                min_free_kbytes = 65536;

        == watermarks depend on the min_free_kbytes ==
        setup_per_zone_wmarks();
        refresh_zone_stat_thresholds();
        setup_per_zone_lowmem_reserve();
        setup_per_zone_inactive_ratio();
        return 0;
}
module_init(init_per_zone_wmark_min);

*/


/*
Page:
-----
Each physical page frame is represented by struct page.
In case CONFIG_DISCONTIGMEM is "not" set, all the page structs are
stored in the mem_map array. Otherwise they can found at, pglist_data->node_mem_map

struct page defined in include/linux/mm_types.h

*/

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
			printf("Error opening the output file for %s\n", output_cache_chain_file_path);
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


//Curently we have 2 zones. Normal and Movable. But we are going to worry here only about the Normal zone.
//add zone movable data later.

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

	fprintf(output_fp,"\n%-23s ","Number of blocks type ");

	for (i = 0; i < MIGRATE_TYPES; ++i) {
		fprintf(output_fp,"%12s ", migratetype_names[i]);
	}

	fprintf(output_fp,"\n");

#define OFFSETOF_ZONE_START_PFN 0x368
#define OFFSETOF_SPANNED_PAGES 0x36c

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_ZONE_START_PFN), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	start_pfn = input_read_buf;

	if(read_uint_from_ramdump(ramdump_fp, __pa(address + OFFSETOF_SPANNED_PAGES), &input_read_buf)) {
		printf("ERROR:%d\n",__LINE__);
		return -1;
	}

	end_pfn = start_pfn + input_read_buf;

#define pageblock_order         (MAX_ORDER-1)
#define pageblock_nr_pages      (1UL << pageblock_order)

	for (i = start_pfn; i < end_pfn; i += pageblock_nr_pages) {
	}

	fclose(output_fp);

	return 0;
}

int Decode_cache_chain(void)
{
	unsigned int address = 0;
	unsigned int list_head = 0;
	unsigned int input_read_buf=0;

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
		printf("FAULT detected in FLD of address 0x%x\n",input_read_buf);
		return -1;
	}

	if ((input_read_buf & 0x2) && (input_read_buf & 0x1)) {//[1:0]->11
		printf("RESERVED detected in FLD of address 0x%x\n",input_read_buf);
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
		printf("FAULT detected in FLD of address 0x%x\n",input_read_buf);
		return -1;
	}

	if ((input_read_buf & 0x2) && (input_read_buf & 0x1)) {//[1:0]->11
		printf("RESERVED detected in FLD of address 0x%x\n",input_read_buf);
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

int Extract_vmstat(void)
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
                printf("Error opening the output file for meminfo desc%s\n", output_meminfo_file_path);
                return -1;
        }

		address = get_addr_from_smap("vm_stat", 7);
        if(read_buf_from_ramdump(ramdump_fp, __pa(address), (VM_BUF_SIZE * 4), vm_buf)) {
            printf("ERROR:%d",__LINE__);
            return -1;
		}

        fprintf(output_fp,"vmstat:\n");
        fprintf(output_fp,"------\n");
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
        fprintf(output_fp,"ContigAlloc:%d pages --> %f Mb\n",
                        vm_buf[NR_CONTIG_PAGES],
                        ((float)(vm_buf[NR_CONTIG_PAGES])* 4)/1024);
        fprintf(output_fp,"CmaAnon:%d pages --> %f Mb\n",
                        vm_buf[NR_CMA_ANON],
                        ((float)vm_buf[NR_CMA_ANON] * 4)/1024);
        fprintf(output_fp,"CmaFile:%d pages --> %f Mb\n",
                        vm_buf[NR_CMA_FILE],
                        ((float)vm_buf[NR_CMA_FILE] * 4)/1024);
        fprintf(output_fp,"CmaUnevicatable:%d pages --> %f Mb\n",
                        vm_buf[NR_CMA_UNEVICTABLE],
                        ((float)vm_buf[NR_CMA_UNEVICTABLE] * 4)/1024);

        fprintf(output_fp,"------------------------------------------------\n");

        fclose(output_fp);

		return 0;
}


/********************************************************************/
/*
Early setup of memory - ARM
---------------------------
when we pass the "mem" param to the kernel, the early param handler "early_mem" of arch/arm/kernel/setup.c is invoked.

Here the function arm_add_memory() is invoked with start as PHYS_OFFSET and size as the value of mem we passed as kernel param.

Here we fill in the structure meminfo of type struct meminfo.

struct meminfo {
        int nr_banks;
        struct membank bank[NR_BANKS];
};

struct membank {
        phys_addr_t start;
        unsigned long size;
        unsigned int highmem;
};

NR_BANKS is CONFIG_ARM_NR_BANKS (which is 8 currently in Hawaii and Rhea).

Each time we call arm_add_memory(), nr_banks is incrememnted.

It is ensured that start and size are alligned to page boundary.

*/
