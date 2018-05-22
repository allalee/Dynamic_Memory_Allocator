/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define HEADER_FOOTER_BYTESIZE 8
/**
 * You should store the head of your free list in this variable.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
sf_free_header* freelist_head = NULL;
char* sf_lowest_block = NULL;
bool sf_lowest_block_bool = false;
static int coalesces = 0;
static int allocatedBlocks = 0;
static int splinterBlocks = 0;
static int paddingSize = 0;
static int splintering = 0;
static int maxHeapSize = 0;
static int currentPK = 0;
static int maxPK = 0;

//Use this function to find the correct place in the freelist to store this memory block
//from lowest to highest address
void* sf_check_freelist(void* ptr){
	void* value = NULL;
	struct sf_free_header* temp = freelist_head;
	while (temp != NULL){
		if(ptr >= (void*)temp){
			value = temp;
		}
		temp = temp->next;
	}
	return value;
}


//Adjusting the list if we are: 1. Mallocing at the only available free block
//2. Mallocing at the first free block of the list
//3. Mallocing at the last free block of the list
//4. Mallocing at any middle free block of the list
void sf_adjust_freelist(void* ptr){
	struct sf_free_header* temp = ptr;
	if(temp->next == NULL && temp->prev == NULL){
		freelist_head = NULL;
	} else if(temp->next != NULL && temp->prev == NULL){
		temp->next->prev = NULL;
		freelist_head = temp->next;
	} else if(temp->next == NULL && temp->prev != NULL){
		temp->prev->next = NULL;
		temp = temp->prev;
	} else {
		temp->prev->next = temp->next;
		temp->next->prev = temp->prev;
	}
}


//Findfit will find the lower bound of the free block
void *sf_findfit(int size){
	struct sf_free_header* temp = freelist_head;
	void* rtnAddr = NULL;
	int bestfit = 0;
	//CASE 1, If we are at the end of the freelist
	while (temp != NULL) {
		if(temp->next == NULL && (temp->header.block_size << 4) >= size && rtnAddr == NULL){
			rtnAddr = temp;
			return rtnAddr;
		}
		//CASE 2, If we have the exact block size then just allocate it there
		else if((temp->header.block_size << 4) == size) {
			rtnAddr = temp;
			break;
		} 	//CASE 3, If the block size is greater than size then we consider it for allocation
			//Don't do anything if it is less.
		else if((temp->header.block_size << 4) > size) {
			//CASE4A, assign bestfit block size if 0 or if less than current bestfit
			if(bestfit == 0 || (temp->header.block_size << 4) < bestfit){
				bestfit = temp->header.block_size << 4;
				rtnAddr = temp;
			}
		}
		temp = temp->next;
	}
	return rtnAddr;
}

//CASES TO CONSIDER. 1. There is no free block behind or after
//2. There is a free block behind
//3. There is a free block in front
//4. There is a free block both behind and in front
void sf_coalesce(void* ptr){
	struct sf_free_header* current = (struct sf_free_header*)((char*)ptr - HEADER_FOOTER_BYTESIZE);
	struct sf_footer* previous = (struct sf_footer*)((char*)current - HEADER_FOOTER_BYTESIZE);
	struct sf_footer* next = (struct sf_footer*)((char*)current + (current->header.block_size << 4));
	int behind;
	int front;
	//If there is nothing behind the freelist at this position then we do not bother with coalescing the back
	if(current->prev == NULL){
		behind = 1;
	} else {
		behind = previous->alloc;
	}
	//If there is nothing in front of the freelist at this position then we do not bother with coalescing the front
	if(current->next == NULL){
		front = 1;
	} else {
		front = next->alloc;
	}
	//If we do not have to coalesce because there are allocated blocks both behind and in front
	if(behind == 1 && front == 1){
		return;
	} //We have to coalesce the block behind
	else if (behind == 0 && front == 1){
		current->prev->header.block_size = current->prev->header.block_size + current->header.block_size;
		struct sf_footer* nextFooter = (struct sf_footer*)(((char*)(current->prev)) + (current->prev->header.block_size << 4) - HEADER_FOOTER_BYTESIZE);
		nextFooter->block_size = current->prev->header.block_size;
		if(current->next != NULL){
			current->next->prev = current->prev;
			current->prev->next = current->next;
		} else {
			current->prev->next = NULL;
		}
		//current->prev = current->next;
		coalesces++;
		return;
	} //We have to coalesce the block in front
	else if (behind == 1 && front == 0){
		current->header.block_size = current->header.block_size + current->next->header.block_size;
		struct sf_footer* nextFooter = (struct sf_footer*)(((char*)current + (current->header.block_size << 4) - HEADER_FOOTER_BYTESIZE));
		nextFooter->block_size = current->header.block_size;
		//If the next next is not null then we set the previous to current else we don't have to worry about it
		if(current->next->next != NULL){
			current->next->next->prev = current;
		}
		//Doesn't matter if it is null or not null
		current->next = current->next->next;
		coalesces++;
		return;
	} //Coalesce both back and front
	else {
		current->prev->header.block_size = current->prev->header.block_size + current->next->header.block_size + current->header.block_size;
		struct sf_footer* nextFooter = (struct sf_footer*)((char*)current->prev + (current->prev->header.block_size << 4) - HEADER_FOOTER_BYTESIZE);
		nextFooter->block_size = current->prev->header.block_size;
		if(current->next->next != NULL){
			current->next->next->prev = current->prev;
		}
		current->prev->next = current->next->next;
		coalesces++;
	}
}

void *sf_malloc(size_t size) {
	int padding;
	bool set_freelist_null = false;
	//Ignore bad sf_malloc request
	if(size == 0){
		errno = EINVAL;
		return NULL;
	}
	//Have to account for padding but only if it is not a multiple of 16
	if(size%16 == 0){
		padding = 0;
	} else{
	 	padding = 16-size%16;
	}
	int newSize = size+padding+(HEADER_FOOTER_BYTESIZE*2);
	if(newSize % 4096 == 0){
		set_freelist_null = true;
	}
	currentPK += size;
	allocatedBlocks++;
	paddingSize += padding;
	//Initialize head with sf_sbrk if NULL. We do not touch the other two structs of the header because we are allocating.
	//No splinters because it is the first free block we use. CASE 1
	if(freelist_head == NULL){
		freelist_head = (struct sf_free_header*) sf_sbrk(newSize);
		void* nomem_check = (void*)-1;
		if(freelist_head == nomem_check){
			//If we couldn't sf_malloc then return an error
			allocatedBlocks--;
			currentPK -= size;
			errno = ENOMEM;
			return NULL;
		}
		maxHeapSize += (int)((char*)sf_sbrk(0) - (char*) freelist_head);
		freelist_head->header.alloc = 1;
		freelist_head->header.block_size = newSize >> 4;
		//Check for splinters
		int splinterCheck = (int)((char*)sf_sbrk(0) - (char*)freelist_head - (freelist_head->header.block_size << 4));
		if(splinterCheck < 32 && splinterCheck != 0){
			freelist_head->header.splinter = 1;
			freelist_head->header.splinter_size = splinterCheck;
			splinterBlocks++;
			splintering += splinterCheck;
			set_freelist_null = true;
		} else {
			freelist_head->header.splinter = 0;
			freelist_head->header.splinter_size = 0;
		}
		freelist_head->header.block_size = (newSize + freelist_head->header.splinter_size) >> 4;
		freelist_head->header.requested_size = size;
		freelist_head->header.padding_size = padding;
		struct sf_footer* footer_address = (struct sf_footer*)(((char*)freelist_head) + newSize + freelist_head->header.splinter_size - HEADER_FOOTER_BYTESIZE);
		footer_address->alloc = 1;
		footer_address->splinter = freelist_head->header.splinter;
		footer_address->block_size = freelist_head->header.block_size;
		//Need to allocate memory for the next block for searching
		if(set_freelist_null){
			char* ptr = (char*)freelist_head + 8;
			freelist_head = NULL;
			return ptr;
		} else {
			struct sf_free_header* free_header_address = (struct sf_free_header*)(((char*)freelist_head) + newSize);
			free_header_address->header.alloc = 0;
			free_header_address->header.block_size = ((char*)sf_sbrk(0) - (char*)free_header_address) >> 4;
			struct sf_footer* free_footer_address = (struct sf_footer*)((char*)free_header_address + (free_header_address->header.block_size << 4) - HEADER_FOOTER_BYTESIZE);
			free_footer_address->alloc = 0;
			free_footer_address->block_size = ((char*)sf_sbrk(0) - (char*)free_header_address) >> 4;
			free_header_address->next = NULL;
			free_header_address->prev = NULL;
			//Return the ptr of the payload for the allocated block and increment the freelist_head to the next available block
			char* ptr = (char*)freelist_head + 8;
			freelist_head = (struct sf_free_header*)(((char*)footer_address) + HEADER_FOOTER_BYTESIZE);
			return ptr;
		}
	} //For all other cases. CASE 2
	else {
		//Find the best fit and then adjust the freelist so that we remove the free block from the list
		//However, if we cannot find a fit then we must sbrk again and add this new free block to the beginning of the list
		struct sf_free_header* bestfit = sf_findfit(newSize);
		if(bestfit == NULL){
			bestfit = (struct sf_free_header*) sf_sbrk(newSize);
			void* nomem_check = (void*)-1;
			if(bestfit == nomem_check){
				//If we couldn't sf_malloc then return an error
				allocatedBlocks--;
				currentPK -= size;
				errno = ENOMEM;
				return NULL;
			}
			maxHeapSize += (int)((char*)sf_sbrk(0) - (char*)bestfit);
			bestfit->header.alloc = 1;
			bestfit->header.block_size = newSize >> 4;
			//Check for splinters
			int splinterCheck = (int)((char*)sf_sbrk(0) - (char*)bestfit - (bestfit->header.block_size << 4));
			if(splinterCheck < 32 && splinterCheck != 0){
				bestfit->header.splinter = 1;
				bestfit->header.splinter_size = splinterCheck;
				set_freelist_null = true;
				splinterBlocks++;
				splintering += splinterCheck;
			} else {
				bestfit->header.splinter = 0;
				bestfit->header.splinter_size = 0;
			}
			bestfit->header.block_size = (newSize + freelist_head->header.splinter_size) >> 4;
			bestfit->header.requested_size = size;
			bestfit->header.padding_size = padding;
			struct sf_footer* footer_address = (struct sf_footer*)(((char*)bestfit) + newSize - HEADER_FOOTER_BYTESIZE);
			footer_address->alloc = 1;
			footer_address->splinter = bestfit->header.splinter;
			footer_address->block_size = bestfit->header.block_size;
			if(set_freelist_null){
				char* ptr = (char*)bestfit + 8;
				return ptr;
			}
			struct sf_free_header* header_address = (struct sf_free_header*)(((char*)bestfit) + newSize);
			header_address->header.alloc = 0;
			header_address->header.block_size = ((char*)sf_sbrk(0) - (char*)header_address) >> 4;
			header_address->next = NULL;
			header_address->prev = NULL;
			footer_address = (struct sf_footer*)((char*)header_address + (header_address->header.block_size << 4) - HEADER_FOOTER_BYTESIZE);
			footer_address->alloc = 0;
			footer_address->block_size = ((char*)sf_sbrk(0) - (char*)header_address) >> 4;
			struct sf_free_header* currPos = sf_check_freelist(header_address);
			//We know that this is inserting at the end of the list
			currPos->next = header_address;
			header_address->prev = currPos;
			char* ptr = (char*)bestfit + 8;
			return ptr;
		} //If we found a fit then we must remove the free block from the list and malloc
		else {
			sf_adjust_freelist(bestfit);
			//Need this boolean to check if the difference between the block size of the best fit and the newSize can be split into a new block
			bool canSplitBlock;
			if((bestfit->header.block_size << 4) - newSize >= 32){
				canSplitBlock = true;
			} else{
				canSplitBlock = false;
			}
			//Account for cases here
			//CASE2A, The current address of the new free block is at the top of the heap
			if(bestfit->next == NULL){
				bestfit->header.alloc = 1;
				bestfit->header.block_size = newSize >> 4;
				//Check for splinters
				int splinterCheck = (int)((char*)sf_sbrk(0) - (char*)bestfit - (bestfit->header.block_size << 4));
				if(splinterCheck < 32 && splinterCheck != 0){
					bestfit->header.splinter = 1;
					bestfit->header.splinter_size = splinterCheck;
					set_freelist_null = true;
					splinterBlocks++;
					splintering += splinterCheck;
				} else {
					bestfit->header.splinter = 0;
					bestfit->header.splinter_size = 0;
				}
				bestfit->header.block_size = (newSize + bestfit->header.splinter_size) >> 4;
				bestfit->header.requested_size = size;
				bestfit->header.padding_size = padding;
				struct sf_footer* footer_address = (struct sf_footer*)(((char*)bestfit) + (bestfit->header.block_size << 4) - HEADER_FOOTER_BYTESIZE);
				footer_address->alloc = 1;
				footer_address->splinter = bestfit->header.splinter;
				footer_address->block_size = bestfit->header.block_size;
				struct sf_free_header* header_address = (struct sf_free_header*)(((char*)bestfit) + (bestfit->header.block_size << 4));
				//Consider the case if we go outside of the boundaries of our allocated memory
				if((char*)header_address >=(char*)sf_sbrk(0)){
					freelist_head = NULL;
				} else {
					header_address->header.alloc = 0;
					header_address->header.block_size = ((char*)sf_sbrk(0) - (char*)header_address) >> 4;
					header_address->next = NULL;
					header_address->prev = NULL;
					struct sf_footer* free_footer_address = (struct sf_footer*)((char*)header_address + (header_address->header.block_size << 4) - HEADER_FOOTER_BYTESIZE);
					free_footer_address-> alloc = 0;
					free_footer_address-> block_size = ((char*)sf_sbrk(0) - (char*)header_address) >> 4;
					//CASE2B, The current head of the freelist is NULL so we just set the freelist_head to this address
					if(freelist_head == NULL){
						header_address->next = NULL;
						header_address->prev = NULL;
						freelist_head = header_address;
					} else {
						struct sf_free_header* insertionPoint = sf_check_freelist(header_address);
						header_address->prev = insertionPoint;
						header_address->next = insertionPoint->next;
						insertionPoint->next = header_address;
					}
				}
			} //CASE3A, We are inserting in the middle or beginning of the list
			else {
				struct sf_free_header* header_address = (struct sf_free_header*)(((char*)bestfit)+newSize);
				//Need an if statement to check if we can split the block. If yes then we need to create another free block
				//to add to the list. Otherwise, we do not have to worry about it and just stick it in.
				if(canSplitBlock){
					header_address->header.alloc = 0;
					header_address->header.block_size = ((bestfit->header.block_size << 4) - newSize) >> 4;
					header_address->next = NULL;
					header_address->prev = NULL;
					struct sf_footer* free_footer_address = (struct sf_footer*)((char*)header_address + (header_address->header.block_size << 4) - HEADER_FOOTER_BYTESIZE);
					free_footer_address->alloc = 0;
					free_footer_address->block_size = header_address->header.block_size;
					//If the new block is created at an address lower than the first element of the freelist
					if(freelist_head->prev == NULL){
						freelist_head->prev = header_address;
						header_address->next = freelist_head;
						freelist_head = header_address;
					} //If the new block is created in the middle of the freelist
					else {
						bestfit->prev->next = header_address;
						bestfit->next->prev = header_address;
						header_address->prev = bestfit->prev;
						header_address->next = bestfit->next;
					}
				}
				bestfit->header.alloc = 1;
				int splinterCheck = (bestfit->header.block_size << 4) - newSize;
				if(splinterCheck < 32 && splinterCheck != 0){
					bestfit->header.splinter = 1;
					bestfit->header.splinter_size = splinterCheck;
					set_freelist_null = true;
					splinterBlocks++;
					splintering += splinterCheck;
				} else {
					bestfit->header.splinter = 0;
					bestfit->header.splinter_size = 0;
				}
				bestfit->header.block_size = (newSize + bestfit->header.splinter_size) >> 4;
				bestfit->header.requested_size = size;
				bestfit->header.padding_size = padding;
				struct sf_footer* footer_address = (struct sf_footer*)(((char*)bestfit) + (bestfit->header.block_size << 4) - 8);
				footer_address->alloc = 1;
				footer_address->splinter = bestfit->header.splinter;
				footer_address->block_size = bestfit->header.block_size;
			}
			char* ptr = (char*)bestfit + 8;
			return ptr;
		}
	}
}

//Helper function for sf_free to check if it is a valid block to free
bool sf_check_val_free(void* ptr){
	bool notValid = false;
	if(ptr == NULL){
		notValid = true;
		return notValid;
	}
	struct sf_free_header* free_ptr = (struct sf_free_header*)((char*)ptr - HEADER_FOOTER_BYTESIZE);
	struct sf_footer* footer_address = (struct sf_footer*)(((char*)free_ptr) + (free_ptr->header.block_size << 4) - HEADER_FOOTER_BYTESIZE);
	//Only free allocated blocks
	if(free_ptr->header.alloc == 0 || footer_address->alloc == 0){
		notValid = true;
		return notValid;
	}
	//If these two variables are not the same then we should do free
	if(free_ptr->header.splinter != footer_address->splinter){
		notValid = true;
		return notValid;
	}
	//If the two block sizes are different then do not free
	if(free_ptr->header.block_size != footer_address->block_size){
		notValid = true;
		return notValid;
	}
	return notValid;
}

void *sf_realloc(void *ptr, size_t size) {
	//CASE 0: Invalid pointer
	if(sf_check_val_free(ptr)){
		errno = EINVAL;
		return NULL;
	}
	//CASE 1: Resizing to 0. Free and return NULL;
	if(size == 0){
		sf_free(ptr);
		return NULL;
	}
	allocatedBlocks++;
	currentPK += size;
	//Points to the pointer's header and footer
	struct sf_free_header* header_address = (struct sf_free_header*)((char*)ptr - 8);
	struct sf_footer* footer_address = (struct sf_footer*)((char*)header_address + (header_address->header.block_size << 4) - 8);
	int padding;
	if(size%16 == 0){
		padding = 0;
	} else {
		padding = 16 - size % 16;
	}
	int newSize = size + padding + 2*HEADER_FOOTER_BYTESIZE;
	paddingSize += padding;
	//CASE 2: Size is the same as before. Return the ptr back
	if(newSize == header_address->header.block_size << 4){
		return ptr;
	}
	//CASE 3: If size is less than the original pointer's size then we return the ptr and deal with splinters or splitting
	if(newSize < header_address->header.block_size << 4){
		//CASE 3A: Consider splinters
		if((header_address->header.block_size << 4) - newSize < 32){
			//The block size and alloc value stays the same, but we edit everything else
			header_address->header.splinter = 1;
			header_address->header.splinter_size = (header_address->header.block_size << 4) - newSize;
			header_address->header.requested_size = size;
			header_address->header.padding_size = padding;
			footer_address->splinter = 1;
			splinterBlocks++;
			splintering += header_address->header.splinter_size;
		} //CASE 3B: Consider splitting
		else if((header_address->header.block_size << 4) - newSize >= 32){
			struct sf_free_header* new_header_address = (struct sf_free_header*)((char*)footer_address + 8);
			new_header_address->header.alloc = 0;
			new_header_address->header.splinter = 0;
			new_header_address->header.block_size = (int)(((char*)footer_address - (char*)header_address - newSize) >> 4);
			struct sf_footer* new_footer_address = (struct sf_footer*)((char*)new_header_address + (new_header_address->header.block_size << 4) - 8);
			new_footer_address->alloc = 0;
			new_footer_address->splinter = 0;
			new_footer_address->block_size = new_header_address->header.block_size;
			//Change the block size to the newSize
			header_address->header.splinter = 0;
			header_address->header.splinter_size = 0;
			header_address->header.block_size = newSize >> 4;
			header_address->header.requested_size = size;
			header_address->header.padding_size = padding;
			footer_address = (struct sf_footer*)((char*)header_address + (header_address->header.block_size << 4) - 8);
			footer_address->alloc = header_address->header.alloc;
			footer_address->splinter = 0;
			footer_address->block_size = header_address->header.block_size;
		} //CASE 3C: Consider neither case
		else {

		}
		return ptr;
	}	//CASE 4: If size is larger than the original pointer's size then we return a new ptr and deal with splinters or splitting
	else {
		struct sf_free_header* bestfit = sf_findfit(newSize);
		//CASE 4A: If there is no free block available in the freelist
		if(bestfit == NULL){
			bestfit = (struct sf_free_header*) sf_sbrk(newSize);
			void* nomem_check = (void*)-1;
			if(bestfit == nomem_check){
				//If we couldn't sf_malloc then return an error
				allocatedBlocks--;
				currentPK -= size;
				errno = ENOMEM;
				return NULL;
			}
			maxHeapSize += (int)((char*)sf_sbrk(0) - (char*) bestfit);
			struct sf_free_header* new_header = memcpy(bestfit, header_address, header_address->header.block_size << 4);
			sf_free(ptr);
			int splinterCheck = (int)((char*)sf_sbrk(0) - (char*)new_header - newSize);
			if(splinterCheck < 32 && splinterCheck != 0){
				new_header->header.splinter = 1;
				new_header->header.splinter_size = splinterCheck;
				splinterBlocks++;
				splintering += splinterCheck;
			} else {
				new_header->header.splinter = 0;
				new_header->header.splinter_size = 0;
			}
			new_header->header.block_size = (newSize + new_header->header.splinter_size) >> 4;
			new_header->header.requested_size = size;
			new_header->header.padding_size = padding;
			struct sf_footer* new_footer = (struct sf_footer*)((char*)new_header + (new_header->header.block_size << 4) - 8);
			new_footer->splinter = new_header->header.splinter;
			new_footer->alloc = new_header->header.alloc;
			new_footer->block_size = new_header->header.block_size;
			if((char*)sf_sbrk(0) - ((char*)new_footer + 8) == 0){
				//If the realloc takes up the entire page then we don't do anything and return the pointer
				return (char*)new_header + 8;
			} else {
				//It can never be null because we freed the block
				struct sf_free_header* splitBlock = (struct sf_free_header*)((char*)new_footer + 8);
				splitBlock->header.alloc = 0;
				splitBlock->header.splinter = 0;
				splitBlock->header.block_size = ((char*)sf_sbrk(0) - (char*)splitBlock) >> 4;
				splitBlock->next = NULL;
				splitBlock->prev = NULL;
				struct sf_footer* splitBlockFooter = (struct sf_footer*)((char*)splitBlock + (splitBlock->header.block_size << 4) - 8);
				splitBlockFooter->alloc = 0;
				splitBlockFooter->splinter = 0;
				splitBlockFooter->block_size = splitBlock->header.block_size;
				struct sf_free_header* fit = sf_check_freelist(splitBlock);
				if(fit == NULL){
					freelist_head->prev = splitBlock;
					splitBlock->next = freelist_head;
					freelist_head = splitBlock;
				} else {
					splitBlock->next = fit->next;
					fit->next = splitBlock;
					splitBlock->prev = fit;
				}
			}
			return (char*)new_header + 8;
		} else {
			//CASE 4C: There is a free block available in the freelist
			//need to use adjust list
			bool canSplit;
			struct sf_footer* bestfit_footer = (struct sf_footer*)((char*)bestfit + (bestfit->header.block_size << 4) - 8);
			int splinterCheck = (int)((char*)bestfit_footer + 8 - (char*)bestfit - newSize);
			sf_adjust_freelist(bestfit);
			sf_free(ptr);
			struct sf_free_header* new_header = memcpy(bestfit, header_address, header_address->header.block_size << 4);
			if(splinterCheck < 32 && splinterCheck != 0){
				new_header->header.splinter = 1;
				new_header->header.splinter_size = splinterCheck;
				splinterBlocks++;
				splintering += splinterCheck;
				canSplit = false;
			} else {
				new_header->header.splinter = 0;
				new_header->header.splinter_size = 0;
				canSplit = true;
			}
			new_header->header.block_size = (newSize + new_header->header.splinter_size) >> 4;
			new_header->header.alloc = 1;
			new_header->header.requested_size = size;
			new_header->header.padding_size = padding;
			struct sf_footer* new_footer = (struct sf_footer*)((char*)new_header + (new_header->header.block_size << 4) - 8);
			new_footer->splinter = new_header->header.splinter;
			new_footer->alloc = new_header->header.alloc;
			new_footer->block_size = new_header->header.block_size;
			if(canSplit){
				struct sf_free_header* splitBlock = (struct sf_free_header*)((char*)new_footer + 8);
				splitBlock->header.alloc = 0;
				splitBlock->header.splinter = 0;
				splitBlock->header.block_size = ((char*)bestfit_footer - (char*)splitBlock + 8) >> 4;
				splitBlock->next = NULL;
				splitBlock->prev = NULL;
				bestfit_footer->alloc = splitBlock->header.alloc;
				bestfit_footer->splinter = splitBlock->header.splinter;
				bestfit_footer->block_size = splitBlock->header.block_size;
				struct sf_free_header* fit = sf_check_freelist(splitBlock);
				if(fit == NULL){
					freelist_head->prev = splitBlock;
					splitBlock->next = freelist_head;
					freelist_head = splitBlock;
				} else {
					splitBlock->next = fit->next;
					fit->next = splitBlock;
					splitBlock->prev = fit;
				}
				return (char*)new_header+8;
			}
			else {
				return (char*)new_header+8;
			}
		}
	}
}

//Things to implement. Coalescing and error handling. ie. What if the ptr is not a proper block or is null?
void sf_free(void* ptr) {
	if(sf_check_val_free(ptr)){
		errno = EINVAL;
		return;
	}
	allocatedBlocks--;
	//Always perform this function to free the block
	struct sf_free_header* free_ptr = (struct sf_free_header*)((char*)ptr - HEADER_FOOTER_BYTESIZE);
	if(free_ptr->header.splinter == 1){
		splinterBlocks--;
		splintering -= free_ptr->header.splinter_size;
	}
	paddingSize -= free_ptr->header.padding_size;
	free_ptr->header.alloc = 0;
	free_ptr->header.splinter = 0;
	free_ptr->next = NULL;
	free_ptr->prev = NULL;
	struct sf_footer* footer_address = (struct sf_footer*)(((char*)free_ptr) + (free_ptr->header.block_size << 4) - HEADER_FOOTER_BYTESIZE);
	footer_address->alloc = 0;
	footer_address->splinter = 0;
	if(freelist_head == NULL){
		freelist_head = free_ptr;
		freelist_head->prev = NULL;
		freelist_head->next = NULL;
		return;
	}
	sf_free_header* insertionPoint = sf_check_freelist(free_ptr);
	//3 CASES, 1 - Inserting at the beginning of the list
	//2 - Inserting in the middle of the list
	//3 - Inserting at the end of the list
	if(insertionPoint == NULL){
		freelist_head->prev = free_ptr;
		free_ptr->next = freelist_head;
		freelist_head = free_ptr;
	} else if(insertionPoint->next == NULL){
		insertionPoint-> next = free_ptr;
		free_ptr->prev = insertionPoint;
	} else {
		free_ptr->prev = insertionPoint;
		free_ptr->next = insertionPoint->next;
		insertionPoint->next->prev = free_ptr;
		insertionPoint->next = free_ptr;
	}
	sf_coalesce(ptr);
	return;
}

int sf_info(info* ptr) {
	if(ptr == NULL){
		return -1;
	}
	if(maxHeapSize == 0){
		return -1;
	}
	if(currentPK > maxPK){
		maxPK = currentPK;
	}
	ptr->allocatedBlocks = allocatedBlocks;
	ptr->splinterBlocks = splinterBlocks;
	ptr->padding = paddingSize;
	ptr->splintering = splintering;
	ptr->coalesces = coalesces;
	ptr->peakMemoryUtilization = ((double)maxPK/(double)maxHeapSize) * 100;
	return 0;
}
