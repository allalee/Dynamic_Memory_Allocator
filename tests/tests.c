#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sfmm.h"

/**
 *  HERE ARE OUR TEST CASES NOT ALL SHOULD BE GIVEN STUDENTS
 *  REMINDER MAX ALLOCATIONS MAY NOT EXCEED 4 * 4096 or 16384 or 128KB
 */

Test(sf_memsuite, Malloc_an_Integer, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(sizeof(int));
  *x = 4;
  cr_assert(*x == 4, "Failed to properly sf_malloc space for an integer!");
}

Test(sf_memsuite, Free_block_check_header_footer_values, .init = sf_mem_init, .fini = sf_mem_fini) {
  void *pointer = sf_malloc(sizeof(short));
  sf_free(pointer);
  pointer = (char*)pointer - 8;
  sf_header *sfHeader = (sf_header *) pointer;
  cr_assert(sfHeader->alloc == 0, "Alloc bit in header is not 0!\n");
  sf_footer *sfFooter = (sf_footer *) ((char*)pointer + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->alloc == 0, "Alloc bit in the footer is not 0!\n");
}

Test(sf_memsuite, SplinterSize_Check_char, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(32);
  void* y = sf_malloc(32);
  (void)y;

  sf_free(x);

  x = sf_malloc(16);

  sf_header *sfHeader = (sf_header *)((char*)x - 8);
  cr_assert(sfHeader->splinter == 1, "Splinter bit in header is not 1!");
  cr_assert(sfHeader->splinter_size == 16, "Splinter size is not 16");

  sf_footer *sfFooter = (sf_footer *)((char*)sfHeader + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->splinter == 1, "Splinter bit in header is not 1!");
}

Test(sf_memsuite, Check_next_prev_pointers_of_free_block_at_head_of_list, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  memset(x, 0, 0);
  cr_assert(freelist_head->next == NULL);
  cr_assert(freelist_head->prev == NULL);
}

Test(sf_memsuite, Coalesce_no_coalescing, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  int *y = sf_malloc(4);
  memset(y, 0, 0);
  sf_free(x);

  //just simply checking there are more than two things in list
  //and that they point to each other
  cr_assert(freelist_head->next != NULL);
  cr_assert(freelist_head->next->prev != NULL);
}

//#
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//#

//This test is to see if we get a splinter when we allocate with the freelist_head as null
Test(sf_memsuite, Splinter_null_check, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(4064);
  sf_header *sfHeader = (sf_header*)((char*)x - 8);
  sf_footer *sfFooter = (sf_footer *)((char*)sfHeader + (sfHeader->block_size << 4) - 8);
  cr_assert(sfHeader->splinter == 1, "Splinter is not set to 1");
  cr_assert(sfHeader->splinter_size == 16, "Splinter size is not 16");
  cr_assert(freelist_head == NULL, "Freelist_head is not null");
  cr_assert(sfFooter->splinter == 1, "Splinter at footer is not 1");
  cr_assert(sfHeader->block_size == sfFooter->block_size, "Block sizes are not equal");
}

//This test is to see if we get a splinter when we allocate with a freelist_head not null, but we sbrk because there isn't enough memory
Test(sf_memsuite, Splinter_null_check_after_sbrk, .init = sf_mem_init, .fini = sf_mem_fini){
  int *y = sf_malloc(64);
  int *x = sf_malloc(4064);
  sf_header *sfHeader = (sf_header*)((char*)x - 8);
  sf_footer *sfFooter = (sf_footer *)((char*)sfHeader + (sfHeader->block_size << 4) - 8);
  cr_assert(y != NULL, "Pointer is null");
  cr_assert(sfHeader->splinter == 1, "Splinter is not set to 1");
  cr_assert(sfHeader->splinter_size == 16, "Splinter size is not 16");
  cr_assert(freelist_head->next == NULL, "Freelist_head next is not null");
  cr_assert(sfFooter->splinter == 1, "Splinter at footer is not 1");
  cr_assert(sfHeader->block_size == sfFooter->block_size, "Block sizes are not equal");
}

//This test is to see if we get a splinter when we allocate 32 bytes and then 4048 bytes
Test(sf_memsuite, Splinter_null_check_top_heap, .init = sf_mem_init, .fini = sf_mem_fini){
  int *y = sf_malloc(4);
  int *x = sf_malloc(4032);
  sf_header *sfHeader = (sf_header*)((char*)x - 8);
  sf_footer *sfFooter = (sf_footer *)((char*)sfHeader + (sfHeader->block_size << 4) - 8);
  cr_assert(y != NULL, "Pointer is null");
  cr_assert(sfHeader->splinter == 1, "Splinter is not set to 1");
  cr_assert(sfHeader->splinter_size == 16, "Splinter size is not 16");
  cr_assert(freelist_head == NULL, "Freelist_head is not null");
  cr_assert(sfFooter->splinter == 1, "Splinter at footer is not 1");
  cr_assert(sfHeader->block_size == sfFooter->block_size, "Block sizes are not equal");
  cr_assert(sfHeader->splinter == sfFooter->splinter, "Splinter bits are not equal");
}

//This test is a basic realloc test to see if we return the same pointer
Test(sf_memsuite, realloc_return_ptr, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(4);
  int *y = sf_malloc(4);
  x = sf_realloc(x, 4);
  sf_header *xHeader = (sf_header*)((char*)y - 40);
  sf_footer *xFooter = (sf_footer*)((char*)y - 8);
  cr_assert(xHeader->block_size << 4 == 32, "Block size is not 32");
  cr_assert(xFooter->block_size << 4 == 32, "Block size is not 32");
  cr_assert((void*)xHeader == (void*)((char*)xFooter - 32), "The addresses are not the same");
}

Test(sf_memsuite, Malloc_an_int, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(4);
  int *y = sf_malloc(4);
  int *z = sf_malloc(4);

  sf_header *sfHeader = (sf_header*)((char*)x - 8);
  sf_footer *sfFooter = (sf_footer *)((char*)sfHeader + (sfHeader->block_size << 4) - 8);
  cr_assert(sfHeader->alloc == 1, "The alloc bit for your free is not 1");
  cr_assert(sfFooter->alloc == 1, "The alloc bit for your first malloc is not 1");
  sf_free(x);
  cr_assert(sfHeader->alloc == 0, "The alloc bit for your free is not 0");
  cr_assert(sfFooter->alloc == 0, "The alloc bit for your first malloc is not 0");
  sf_header *sfHeadery = (sf_header*)((char*)y - 8);
  sf_footer *sfFootery = (sf_footer *)((char*)sfHeadery + (sfHeadery->block_size << 4) - 8);
  sf_free(y);
  cr_assert(sfHeadery->alloc == 0, "The alloc bit for your free is not 0");
  cr_assert(sfFootery->alloc == 0, "The alloc bit for your first malloc is not 0");
  cr_assert(sfHeader->block_size << 4 == 64, "The size of the free block at the header is not 64");
  cr_assert(sfFootery->block_size << 4 == 64, "The size of the free block at the header is not 64");
  int *a = sf_malloc(4);
  sf_header *sfHeadera = (sf_header*)((char*)a - 8);
  cr_assert((char*)sfHeader == (char*)sfHeadera, "The addresses of the blocks are not the same");
  cr_assert(sfHeadera->block_size << 4 == 32, "The block size is not 32");
  cr_assert(sfFootery->block_size << 4 == 32, "The block size of the free block is not 32");
  sf_free(z);
}

//This test is to make sure the pointer is NULL if we allocated over 4 pages
Test(sf_memsuite, malloc_over_heap, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(20000);
  cr_assert(x == NULL, "We are mallocing over the max amount of memory we have when we cannot do so");

}

//This test is to check if we allocate exactly a multiple of 4096 bytes then we set the freelist_head to null (include padding)
Test(sf_memsuite, malloc_4080, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(4080);
  sf_header *sfHeader = (sf_header*)((char*)x - 8);
  cr_assert(sfHeader->block_size << 4 == 4096, "The block size is not 4096 bytes");
  cr_assert(freelist_head == NULL, "The freelist_head is not null");
}

//This test is to check if by allocating exactly 4096 bytes after a smaller malloc call that we do not set the freelist_head to null
Test(sf_memsuite, malloc_4096_total, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(4);
  int *y = sf_malloc(4080);
  cr_assert(freelist_head != NULL, "The freelist_head is null");
  sf_header *sfHeader = (sf_header*)((char*)x - 8);
  cr_assert(sfHeader->block_size << 4 == 32, "block_size is not 32");
  sf_header *sfHeadery = (sf_header*)((char*)y - 8);
  cr_assert(sfHeadery->block_size << 4 == 4096, "block_size is not 32");
}

//Another test to consider is when we alloc 32 then the rest of a 4096 block and then try to alloc 4096 and see if the freelist_head is null
Test(sf_memsuite, malloc_two_pages, .init = sf_mem_init, .fini = sf_mem_fini){
  void *x = sf_malloc(4);
  void *y = sf_malloc(4048);
  void *z = sf_malloc(4080);
  cr_assert(freelist_head == NULL, "freelist_head is not null");
  sf_free(x);
  cr_assert(freelist_head->header.block_size << 4 == 32, "First free block is not 32");
  sf_free(y);
  sf_free(z);
  cr_assert(freelist_head->header.block_size << 4 == 8192, "Free block is not 8192");
}

//Reallocing to 0
Test(sf_memsuite, realloc_zero, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(32);
  x = sf_realloc(x, 0);
  cr_assert(x == NULL, "Realloc did not return NULL");
  cr_assert(freelist_head->header.alloc == 0, "Alloc bit is not 0");
  cr_assert(freelist_head->header.splinter == 0, "Splinter bit is not 0");
  cr_assert(freelist_head->header.block_size << 4 == 4096, "Block size is not 4096");
  cr_assert(freelist_head->prev == NULL, "Previous is not null");
  cr_assert(freelist_head->next == NULL, "Next is not null");

}

//Realloc same size
Test(sf_memsuite, realloc_same_size, .init = sf_mem_init, .fini = sf_mem_fini){
  void *x = sf_malloc(32);
  void *y = sf_realloc(x, 32);
  cr_assert(x == y, "Realloc did not return NULL");
}

//Reallocing with splinters
Test(sf_memsuite, realloc_splinters, .init = sf_mem_init, .fini = sf_mem_fini){
  void *x = sf_malloc(32);
  void *z = sf_malloc(4);
  void *y = sf_realloc(x, 16);
  struct sf_free_header* sf_Header = (struct sf_free_header*)((char*)x - 8);
  cr_assert(x == y, "Realloc did not return the same location in memory");
  cr_assert(sf_Header->header.splinter == 1, "Splinter is not 1");
  cr_assert(sf_Header->header.block_size << 4 == 48, "Block size is not 48");
  cr_assert(sf_Header->header.splinter_size == 16, "Splinter size is not 16");
  sf_free(z);
}

//Simple info test
Test(sf_memsuite, basic_info_test, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(4);
  int *y = sf_malloc(4);
  int *z = sf_malloc(4);
  info myinfo;
  myinfo.padding = 0;
  myinfo.allocatedBlocks = 0;
  myinfo.splinterBlocks = 0;
  myinfo.coalesces = 0;
  myinfo.splintering = 0;
  myinfo.peakMemoryUtilization = 0;
  sf_info(&myinfo);
  cr_assert(myinfo.padding == 36, "Padding is not equal to 36");
  cr_assert(myinfo.allocatedBlocks == 3, "There is not 3 allocatedBlocks");
  cr_assert(myinfo.peakMemoryUtilization >= 0.29296, "Peak Memory Utilization is not (12/4096)*100");
  sf_free(x);
  sf_free(y);
  sf_free(z);
}

//sf_info test with frees, coalesces, and splinters
Test(sf_memsuite, info_with_coalesces_frees_splinters, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(32); //Takes up 48 bytes
  int *y = sf_malloc(4);  //Takes up 32 bytes
  int *z = sf_malloc(4);  //Takes up 32 bytes
  info myinfo;
  myinfo.padding = 0;
  myinfo.allocatedBlocks = 0;
  myinfo.splinterBlocks = 0;
  myinfo.coalesces = 0;
  myinfo.splintering = 0;
  myinfo.peakMemoryUtilization = 0;
  sf_free(x);
  sf_info(&myinfo);
  cr_assert(myinfo.allocatedBlocks == 2, "Info does not have 2 allocated blocks");
  cr_assert(myinfo.padding == 24, "Padding is not equal to 24");
  int *a = sf_malloc(4); //Should take the place of x
  sf_info(&myinfo);
  cr_assert(myinfo.allocatedBlocks == 3, "Info does not have 3 allocated blocks");
  cr_assert(myinfo.splinterBlocks == 1, "Info does have one splinter");
  cr_assert(myinfo.splintering == 16, "There is a splintering of 16 bytes");
  sf_free(a);
  sf_free(y); //Leads to one coalesce
  sf_free(z); //Leads to second coalesce
  sf_info(&myinfo);
  cr_assert(myinfo.coalesces == 2, "There are two coalescses");
  cr_assert(myinfo.allocatedBlocks == 0, "Info does not have 0 allocated blocks");
  cr_assert(myinfo.splinterBlocks == 0, "Info does not have 0 splinters");
  cr_assert(myinfo.splintering == 0, "Info does not have 0 splinter bytes");
  cr_assert(myinfo.padding == 0, "Info does not have 0 padding bytes");
}

//basic sf_info test with realloc
Test(sf_memsuite, basic_info_with_realloc, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(4);
  int *y = sf_malloc(4);
  int *z = sf_malloc(4);
  sf_realloc(x, 30); //Should take up 48 bytes and we freed the malloc block
  sf_free(y);
  info myinfo;
  myinfo.padding = 0;
  myinfo.allocatedBlocks = 0;
  myinfo.splinterBlocks = 0;
  myinfo.coalesces = 0;
  myinfo.splintering = 0;
  myinfo.peakMemoryUtilization = 0;
  sf_info(&myinfo);
  cr_assert(myinfo.coalesces == 1, "Info does not have one coalesce");
  sf_free(z);
  sf_info(&myinfo);
  cr_assert(myinfo.coalesces == 2, "Info does not have two coalesces");
  cr_assert(myinfo.allocatedBlocks == 1, "Info does not have an allocated block");
  cr_assert(myinfo.padding == 2, "Info does not have two padding bytes");
  cr_assert(myinfo.splinterBlocks == 0, "Info has splinter blocks");
  cr_assert(myinfo.splintering == 0, "Info has splintering bytes");
}

//Test with behind coalescing
Test(sf_memsuite, coalesce_behind, .init = sf_mem_init, .fini = sf_mem_fini){
 int *x = sf_malloc(4);
 int *y = sf_malloc(4);
 int *z = sf_malloc(4);
 sf_free(x);
 sf_free(y);
 struct sf_free_header* sf_Header = (struct sf_free_header*)((char*)x - 8);
 struct sf_footer* sf_Footer = (struct sf_footer*)((char*)x + 48);
 cr_assert(sf_Header->header.alloc == 0, "Alloc bit is not 0");
 cr_assert(sf_Header->header.splinter == 0, "Splinter bit is not 0");
 cr_assert(sf_Header->prev == NULL, "Previous pointer is not NULL");
 cr_assert(sf_Header->next != NULL, "Next pointer is NULL");
 cr_assert(sf_Header->header.block_size << 4 == 64, "Block size is not 64");
 cr_assert(sf_Footer->alloc == 0, "Alloc bit is not 0");
 cr_assert(sf_Footer->splinter == 0, "Splinter bit is not 0");
 cr_assert(sf_Footer->block_size << 4 == 64, "Block size is not 64");
 sf_free(z);
 cr_assert(sf_Header->header.block_size << 4 == 4096, "Block size is not 4096");


}
//Test with front coalescing
Test(sf_memsuite, coalesce_front, .init = sf_mem_init, .fini = sf_mem_fini){
 int *x = sf_malloc(4);
 int *y = sf_malloc(4);
 int *z = sf_malloc(4);
 sf_free(z);
 struct sf_free_header* sf_Header = (struct sf_free_header*)((char*)z - 8);
 struct sf_footer* sf_Footer = (struct sf_footer*)((char*)sf_Header + (sf_Header->header.block_size << 4) - 8);
 cr_assert(sf_Header->header.alloc == 0, "Alloc bit is not 0");
 cr_assert(sf_Header->header.splinter == 0, "Splinter bit is not 0");
 cr_assert(sf_Header->prev == NULL, "Previous pointer is not NULL");
 cr_assert(sf_Header->next == NULL, "Next pointer is NULL");
 cr_assert(sf_Header->header.block_size << 4 == 4032, "Block size is not 4032");
 cr_assert(sf_Footer->alloc == 0, "Alloc bit is not 0");
 cr_assert(sf_Footer->splinter == 0, "Splinter bit is not 0");
 cr_assert(sf_Footer->block_size << 4 == 4032, "Block size is not 4032");
 sf_free(y);
 sf_free(x);
 cr_assert(sf_Footer->block_size << 4 == 4096, "Block size is not 4096");
 struct sf_free_header* xHeader = (struct sf_free_header*)((char*)x - 8);
 cr_assert(xHeader->header.block_size << 4 == 4096, "Block size is not 4096");
}

//Test with behind and front coalescing
Test(sf_memsuite, coalesce_both, .init = sf_mem_init, .fini = sf_mem_fini){
 int *x = sf_malloc(4);
 int *y = sf_malloc(4);
 int *z = sf_malloc(4);
 int *a = sf_malloc(4);
 sf_free(x);
 sf_free(z);
 sf_free(y);
 struct sf_free_header* sf_Header = (struct sf_free_header*)((char*)x - 8);
 struct sf_footer* sf_Footer = (struct sf_footer*)((char*)sf_Header + (sf_Header->header.block_size << 4) - 8);
 cr_assert(sf_Header->header.alloc == 0, "Alloc bit is not 0");
 cr_assert(sf_Header->header.splinter == 0, "Splinter bit is not 0");
 cr_assert(sf_Header->prev == NULL, "Previous pointer is not NULL");
 cr_assert(sf_Header->next != NULL, "Next pointer is NULL");
 cr_assert(sf_Header->header.block_size << 4 == 96, "Block size is not 96");
 cr_assert(sf_Footer->alloc == 0, "Alloc bit is not 0");
 cr_assert(sf_Footer->splinter == 0, "Splinter bit is not 0");
 cr_assert(sf_Footer->block_size << 4 == 96, "Block size is not 96");
 sf_free(a);
}

//Basic test freeing with splinters
Test(sf_memsuite, free_with_splinters, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(32);
  int *y = sf_malloc(4);
  sf_free(x);
  struct sf_free_header* sf_Header = (struct sf_free_header*)((char*)x - 8);
  cr_assert(sf_Header->header.splinter_size == 0, "Splinter size is not 0");
  sf_free(y);
}

//Test for no crashes with multiple mallocs, frees
Test(sf_memsuite, malloc_and_free, .init = sf_mem_init, .fini = sf_mem_fini){
  void *x = sf_malloc(40);
  void *y = sf_malloc(100);
  sf_free(x);
  void *z = sf_malloc(4);
  void *a = sf_malloc(60);
  sf_free(z);
  void *b = sf_malloc(5000);
  sf_free(b);
  sf_free(y);
  void *c = sf_malloc(2000);
  sf_free(a);
  sf_free(c);
  cr_assert(freelist_head->header.block_size << 4 == 12288, "Block size is not 8192");
}

//Test for no crashes with multiple mallocs, frees, and reallocs
Test(sf_memsuite, malloc_free_realloc, .init = sf_mem_init, .fini = sf_mem_fini){
  void *x = sf_malloc(40);
  x = sf_realloc(x, 40);
  x = sf_realloc(x, 1000);
  sf_varprint(x);
  sf_snapshot(true);
  void *y = sf_malloc(100);
  sf_varprint(x);
  sf_varprint(y);
  sf_snapshot(true);
  sf_free(x);
  void *z = sf_malloc(4);
  y = sf_realloc(y, 20);
  sf_varprint(y);
  sf_snapshot(true);
  void *a = sf_malloc(60);
  a = sf_realloc(a, 80);
  sf_free(z);
  void *b = sf_malloc(5000);
  sf_varprint(b);
  sf_free(b);
  sf_free(y);
  void *c = sf_malloc(2000);
  sf_snapshot(true);
  sf_free(a);
  sf_free(c);
}

//Assigning values
Test(sf_memsuite, assigning_values, .init = sf_mem_init, .fini = sf_mem_fini){
  struct basic_struct{
    int x;
    int y;
  };

  struct basic_struct* mystruct = sf_malloc(sizeof(struct basic_struct));
  mystruct->x = 100;
  mystruct->y = 10;
  double *ptr = sf_malloc(sizeof(double));
  *ptr = 50;
  sf_free(mystruct);
}
