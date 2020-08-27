/*
 *		mod_malloc - A Kernel Module to measure allocation CPU Cycles
 */

#include <linux/kernel.h>		//required by modules
#include <linux/module.h>		//KERN_INFO, ...
#include <linux/slab.h> 		//we are using kmalloc
#include <linux/preempt.h>
#include <linux/init.h>
#include <linux/hardirq.h>
#include <linux/sched.h>

#define WARMUPS 5

//module Descriptions
MODULE_LICENSE("GPL");
MODULE_INFO(author,"Nils Lübben");
MODULE_INFO(intree,"N");
MODULE_DESCRIPTION("Module for measuring allocation time, created for class assignment");

static int loop_cnt=1;	 //number of times alloc_size bytes are allocated
static int alloc_size=100; //number of bytes that are allocated

//make input parameters visible using OTHER permission tag
module_param(loop_cnt, int, S_IRUGO);
module_param(alloc_size, int, S_IRUGO);
MODULE_PARM_DESC(loop_cnt,"Number of allocations that are performed");
MODULE_PARM_DESC(alloc_size,"Size of allocated memory");

//global variables for holding tsc values and still enter scope of void function
unsigned int hi1, hi2, lo1, lo2;

//construct 64 bit time stamp from two 32 ints
unsigned long long construct_tscp(unsigned int hi, unsigned int lo){
				return(((unsigned long long) hi << 32) | (unsigned long long) lo);
}

/* inline assembly that calls rdtsc to read time stamps and cpuid to 
 * serialize CPU instructions. Both instructions modify registers which
 * is why we additionally clobber r[a-d]x under Intel 64 Architecture
 * and e[a-d]x under 32 Architecture
 */
static __inline__ void rdtsc_f(void){
				__asm__ __volatile__("CPUID\n\t"
									"RDTSC\n\t"
									"mov %%edx, %0\n\t"
									"mov %%eax, %1\n\t": "=r" (hi1), "=r" (lo1):: "%rax", "%rbx", "%rcx", "%rdx");
}

/* inline assembly that calls rdtscp first, then the serialization
 * instruction such that the following instructions are serialized and cant
 * precede that point, resulting in not being accidentally measured
 */
static __inline__ void rdtsc_s(void){
				__asm__	__volatile__("RDTSCP\n\t"
									"mov %%edx, %0\n\t"
									"mov %%eax, %1\n\t"
									"CPUID\n\t": "=r" (hi2), "=r" (lo2):: "%rax", "%rbx", "%rcx", "%rdx");
}

//global pointer for allocation
static void **pointers;

/*
 * The implementation of the measurement is inspired by the method outlined
 * in the Intel whitepaper found at
 * https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/ia-32-ia-64-benchmark-code-execution-paper.pdf
 * which discusses accurate CPU Cycle reading, without measuring
 * a lot of the overhead that the measurment itself produces.
 * The following is a naive approach, still containing overhead and
 * not taking any advanced statistics
 */
 
__init int init_module(void){
	
	int i, k; 																			//indicies
	unsigned long long CPU_cycles, offset_cycles; 	//for storing the cycles
	unsigned long irq_flags;												//for restoring IRQ state
	

	printk(KERN_DEBUG"Measuring %d allocation time of %d Bytes...\n", loop_cnt, alloc_size);
  
//allocate pointer of loop_cnt pointers, dont measure yet
	pointers = kmalloc(loop_cnt*alloc_size, GFP_KERNEL);
	if(!pointers){
					printk(KERN_ERR "Unable to allocate Memory");
					return 0;
	}
	
//warmup for offset measurement
	preempt_disable();
	raw_local_irq_save(irq_flags);
	for(i = 0; i < WARMUPS;i++){
	        rdtsc_f();
					rdtsc_s();
	}
	preempt_enable();
	raw_local_irq_restore(irq_flags);

//calculate an offset, measuring the rdtsc process
	offset_cycles = construct_tscp(hi2, lo2) - construct_tscp(hi1, lo1); //calculate CPU cycles
	
//take CPU ownership
	preempt_disable();								//disable context switching
	raw_local_irq_save(irq_flags);		//mask interrupts

//warmup again
	for(i = 0; i < WARMUPS;i++){
					rdtsc_f();
					rdtsc_s();
	}
  
//start the actual measurement
	rdtsc_f();
  
//allocate the memory, check for failed allocations
	for(i = 0; i < loop_cnt; i++){
					pointers[i] = kmalloc(alloc_size, GFP_KERNEL);
					if(!pointers[i]){
									printk(KERN_ERR"Unable to allocate Memory at pointers[%d]",i);
									for(k = 0; k < i - 1; k++)
													kfree(pointers[k]);
									return 0;
					}
	}

	rdtsc_s();
	preempt_enable();										//release CPU ownership
	raw_local_irq_restore(irq_flags);		//restore IRQ
	
	for(i = 0; i < loop_cnt; i++)
					kfree(pointers[i]);
	kfree(pointers);

	CPU_cycles = construct_tscp(hi2, lo2) - construct_tscp(hi1, lo1); //calculate CPU cycles

	printk(KERN_DEBUG"Allocation took %llu (-%llu) cycles \n",CPU_cycles, offset_cycles);

	return 0;
}

__exit void cleanup_module(void){
//		printk(KERN_INFO "Goodbye mod_malloc!\n");
}
