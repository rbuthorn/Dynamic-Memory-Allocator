/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>


int max_size_list(size_t M, size_t block_size){
    int max_size = 0;

    if(block_size <= M){
        max_size = 32;
    }

    else if(block_size > M && block_size <= (2*M)){
        max_size = 64;
    }

    else if(block_size > (2*M) && block_size <= (3*M)){
        max_size = 96;
    }

    else if(block_size > (3*M) && block_size <= (5*M)){
        max_size = 160;
    }

    else if(block_size > (5*M) && block_size <= (8*M)){
        max_size = 256;
    }

    else if(block_size > (8*M) && block_size <= (13*M)){
        max_size = 416;
    }
    else{
        max_size = 1000000;
    }
    return max_size;
}



int find_starting_index(size_t M, size_t block_size, int wilderness){
    int starting_index;
    if(wilderness){
        return 7;
    }

    else if(block_size <= M){
        starting_index = 0;
    }

    else if(block_size > M && block_size <= (2*M)){
        starting_index = 1;
    }

    else if(block_size > (2*M) && block_size <= (3*M)){
        starting_index = 2;
    }

    else if(block_size > (3*M) && block_size <= (5*M)){
        starting_index = 3;
    }

    else if(block_size > (5*M) && block_size <= (8*M)){
        starting_index = 4;
    }

    else if(block_size > (8*M) && block_size <= (13*M)){
        starting_index = 5;
    }
    else{
        starting_index = 6;
    }
    return starting_index;
}


//PAYLOAD CHECK
void insert_into_correct_freelist(size_t block_size, sf_block* add_of_block, int wilderness){  //inserts into correct freelist and adds a footer
    int starting_index = find_starting_index(32, block_size, wilderness);  //add_of_block is ptr to PAYLOAD

    sf_block* nextBlockFromAppended = sf_free_list_heads[starting_index].body.links.next;  //pointer to HEAD of sentinel
    sf_block* sentinelBlock = &sf_free_list_heads[starting_index];

    add_of_block->body.links.prev= sentinelBlock;
    add_of_block->body.links.next= nextBlockFromAppended;

    nextBlockFromAppended->body.links.prev = add_of_block;   //make sentinels previously next node be connected to new node + connect sentinel
    sentinelBlock->body.links.next = add_of_block;

    //add footer
    size_t footer = block_size;
    size_t* footer_address = (size_t*) ((char*) add_of_block + block_size - 8);
    *footer_address = footer;  //set footer of block
}



//PAYLOAD CHECK.....assumes allocating into an already free block
size_t allocate_block(sf_block* block, size_t req_size){  //allocates block and returns the size of the allocated block

    /*sf_block* prologue = (sf_block*) ((char*)new_page + 24);

            prologue->header = 0 | 0x10;
            char* prologue_footer_address = (char*) prologue + 24;
            size_t prologue_footer = 0 | 0x10;
            *prologue_footer_address = prologue_footer; */




    sf_block* nextBlock = block->body.links.next;
    sf_block* prevBlock = block->body.links.prev;

    nextBlock->body.links.prev = prevBlock;
    prevBlock->body.links.next = nextBlock;

    sf_block* alloc_block = block;

    alloc_block->header = req_size;
    alloc_block->header |= 0x10;  //set 4th bit to 1 to show allocation   //prev's next field = next block and vice versa to maintain linked list

    size_t actual_allocation_size = req_size;

    while(actual_allocation_size % 32 != 0){  // changes size of block to be allocated to be 32 byte aligned
        actual_allocation_size++;
    }

    size_t footer = actual_allocation_size | 0x10;
    size_t* footer_address = (size_t*) ((char*) alloc_block + actual_allocation_size - 8);
    *footer_address = footer;  //set footer of block

    return actual_allocation_size;
}


//PAYLOAD CHECK
sf_block* attempt_split(sf_block* block, size_t req_size){   //if split is possible, returns address of remainder of split block
    int header = block->header;

    if((block->header & 0x10) == 0x10){
        header = block->header ^ (1 << 4);
    }

    size_t remainder = header - req_size;

    size_t actual_allocation_size = req_size;

    if(remainder < 32){  //if block is not big enough to be split into two blocks
       return NULL;
    }

    while(actual_allocation_size % 32 != 0){  // changes size of block to be allocated to be 32 byte aligned
        actual_allocation_size++;
    }

    sf_block* upper_address = (sf_block *)((char *)block + actual_allocation_size);

    return upper_address;
}


//PAYLOAD CHECK
sf_block* satisfy_alloc_block_req(int starting_index, size_t allocation_size, int wilderness){  //returns ptr to alloc block if successful, if not return NULL
    int break_flag = 0;

    for(int i = starting_index; i < 8; i++){ //outer for loop for iterating through the individual free lists
        sf_block* currentBlock = sf_free_list_heads[i].body.links.next;

        while(currentBlock != &sf_free_list_heads[i]){  //inner for loop for traversing free list looking for a block
            if(currentBlock->header < allocation_size){
                currentBlock = currentBlock->body.links.next;
                continue;
            }
            if(((currentBlock->header & 0x10)) == 0x10){  //block is allocated already(should never occur anyways but just in case)

                currentBlock = currentBlock->body.links.next;
                continue;
            }

            else{  //if block is free, attempt to split before allocating
                sf_block* upper_address = attempt_split(currentBlock, allocation_size);
                break_flag = 1;

                if(upper_address == NULL){  //if block cannot be split, add additional space in block to padding and then allocate
                    size_t new_block_size = currentBlock->header; // allocated block size now includes padding
                    allocate_block(currentBlock, new_block_size);
                    break;
                }

                else{
                    size_t curHeader = currentBlock->header;

                    size_t new_allocation_size = allocate_block(currentBlock, allocation_size);  //allocate the block as is, then work on the remainder

                    sf_block* upperBlock = upper_address;
                    upperBlock->header = curHeader - new_allocation_size;  //upper block size is the former block size-what was allocated

                    insert_into_correct_freelist(upperBlock->header, upperBlock, wilderness);
                    break;
                }

            }
        }
        if(break_flag){
            return currentBlock;
        }
    }
    return NULL;
}


//PAYLOAD CHECK
void coalesce(int isWilderness, sf_block* ptr){
    int prev_block_free = 0;
    int next_block_free = 0;
    int no_wild_flag = 0;

    if(isWilderness){
        sf_block* wilderness_block = sf_free_list_heads[7].body.links.next;
        if((wilderness_block->header & 0x10) == 0x10){
            wilderness_block->header = wilderness_block->header ^ (1 << 4);
        }
        if(&sf_free_list_heads[7] == sf_free_list_heads[7].body.links.next){  //if no wilderness, ahve to insert it into free list
            no_wild_flag = 1;
        }
        wilderness_block->header += 2048; //old epilogue gets consumed by new page, new epilogue created

        if(no_wild_flag){
            insert_into_correct_freelist(wilderness_block->header, wilderness_block, 1);
            return;
        }
        size_t footer = wilderness_block->header;
        size_t* footer_address = (size_t*) ((char*)sf_mem_end() - 16);
        *footer_address = footer;


    }

    else{
        int hdr =ptr->header ^ (1 << 4);
        sf_block* next_header_address = (sf_block*) ((char*) ptr + hdr); //ptr to header+ size of block points to next block
        size_t next_size = next_header_address->header;
        if((next_size & 0x10) != 0x10){
            next_block_free = 1;
        }
        else{
            next_size = next_size ^ (1 << 4);
        }
        for(int i = 0; i < 8; i++){
            if(next_header_address == &sf_free_list_heads[i]){  //if next block is a sentinel, technically next adjacent block is not free
                next_block_free = 0;
                break;
            }
        }

        size_t* prev_footer_address = (size_t*) ((char*) ptr - 8); //ptr to header+ size of block points to next block
        size_t prev_size = *prev_footer_address ;
        sf_block* prev_header_address = (sf_block*) ((char*) ptr - prev_size);

        if((prev_size & 0x10) != 0x10){
            prev_block_free = 1;
        }
        else{
            prev_size = prev_size ^ (1 << 4);
        }
        for(int i = 0; i < 8; i++){
            if(prev_header_address == &sf_free_list_heads[i]){  //if prev block is a sentinel, technically prev adjacent block is not free
                prev_block_free = 0;
                break;
            }
        }

        ptr->header = ptr->header ^(1<<4);

        //4 cases for coalescing based on above findings

        if(!next_block_free && !prev_block_free){
            insert_into_correct_freelist(hdr, ptr, 0);
            return;
        }
        else if(next_block_free && !prev_block_free){
            ptr->header += next_size;

            size_t* new_footer_add = (size_t*) ((char*) ptr + ptr->header - 8);  //ptr should be to payload, so -8 for header of next block and another - 8 for start of footer
            size_t new_footer = ptr->header;
            *new_footer_add = new_footer;

            //change sentinel pointers of free lists that are having blocks taken out of them
            int max_size = max_size_list(32, next_size);
            if(new_footer > max_size){
                sf_block* nextnext = next_header_address->body.links.next;
                sf_block* nextprev = next_header_address->body.links.prev;

                nextnext->body.links.prev = nextprev;
                nextprev->body.links.next = nextnext;
            }

            insert_into_correct_freelist(ptr->header, ptr, 0);
            return;
        }
        else if(!next_block_free && prev_block_free){

            prev_header_address->header = ptr->header + prev_size;

            size_t* new_footer_add = (size_t*) ((char*) ptr + ptr->header - 8);  //ptr should be to payload, so -8 for header of next block and another - 8 for start of footer
            size_t new_footer = prev_header_address->header;
            *new_footer_add = new_footer;

            int max_size = max_size_list(32, prev_size);
            if(new_footer > max_size){
                sf_block* prevnext = prev_header_address->body.links.next;
                sf_block* prevprev = prev_header_address->body.links.prev;

                prevnext->body.links.prev = prevprev;
                prevprev->body.links.next = prevnext;
            }

            insert_into_correct_freelist(prev_header_address->header, prev_header_address, 0);
            return;
        }
        else{
            prev_header_address->header = ptr->header + prev_size + next_size;

            size_t* new_footer_add = (size_t*) ((char*) ptr + ptr->header - 8);  //ptr should be to payload, so -8 for header of next block and another - 8 for start of footer
            size_t new_footer = prev_header_address->header;
            *new_footer_add = new_footer;

            int max_size_prev = max_size_list(32, prev_size);
            if(new_footer > max_size_prev){
                sf_block* prevnext = prev_header_address->body.links.next;
                sf_block* prevprev = prev_header_address->body.links.prev;

                prevnext->body.links.prev = prevprev;
                prevprev->body.links.next = prevnext;
            }

            int max_size_next = max_size_list(32, next_size);
            if(new_footer > max_size_next){
                sf_block* nextnext = next_header_address->body.links.next;
                sf_block* nextprev = next_header_address->body.links.prev;

                nextnext->body.links.prev = nextprev;
                nextprev->body.links.next = nextnext;
            }

            insert_into_correct_freelist(prev_header_address->header, prev_header_address, 0);
            return;

        }
    }
}


//PAYLOAD CHECK
void *sf_malloc(size_t size) {
    if(size == 0){
        return NULL;
    }

    int initial = 0;  //keeps track of whether this call to sf_malloc is the first one or not
    size_t M = 32; // M = min block size
    int starting_index;  //index of free list
    size_t allocation_size = size + 16; //header + footer = 16 bytes
    while(allocation_size % 32 != 0){  // changes size of block to be allocated to be 32 byte aligned
        allocation_size++;
    }

   starting_index = find_starting_index(M, allocation_size, 0);

    // initialize ALL free lists on first call to sf_malloc

    if(sf_free_list_heads[starting_index].body.links.prev == NULL && sf_free_list_heads[starting_index].body.links.next == NULL){
        for(int i = 0; i < 8; i++){
            sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i]; //sentinels next and prev points to its own payload
            sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];  //set both prev and next of each sentinel node of free list to address of itself
        }
        initial = 1;  //sets initial flag to be used for sf_mem_grow later
    }

    sf_block* official_allocated_block = satisfy_alloc_block_req(starting_index, allocation_size, 0);  //block should be allocated, if not, either its the very first alloc, or not enough space on heap(will equal NULL)

    while(!official_allocated_block){
        sf_block* new_page = sf_mem_grow();

        if(new_page == NULL){
            sf_errno = ENOMEM;
            return NULL;
        }

        if(initial){  // make prologue, make wilderness block, make footer, insert wilderness into freelist
            sf_block* prologue = (sf_block*) ((char*)new_page + 24);

            sf_header* epilogue = (sf_header*)((char *)sf_mem_end() -8);  //make epilogue 8 bytes before end of heap on every call to mem_grow
            *epilogue = 0 | 0x10;

            prologue->header = 32 | 0x10;
            char* prologue_footer_address = (char*) prologue + 24;
            size_t prologue_footer = 32 | 0x10;
            *prologue_footer_address = prologue_footer;


            sf_block* initial_wilderness_block = sf_mem_start() + 24 + 32;  //ptr to payload start of wilderness block
            initial_wilderness_block->header = 2048-24-32-8;  //page - (unused - prologue - epilogue)

            size_t footer = initial_wilderness_block->header;
            size_t* footer_address = (size_t*) ((char*) initial_wilderness_block + initial_wilderness_block->header - 8);
            *footer_address = footer;  //set footer of block

            insert_into_correct_freelist(initial_wilderness_block->header, initial_wilderness_block, 1);  //wilderness block now exists

            official_allocated_block = satisfy_alloc_block_req(7, allocation_size, 1);
            initial = 0;
        }

        else{   //coalesce new page with existing wilderness block, then satisfy_alloc_block_req
            coalesce(1, new_page);
            sf_header* epilogue = (sf_header*)((char *)sf_mem_end() -8);  //make epilogue 8 bytes before end of heap on every call to mem_grow
            *epilogue = 0 | 0x10;
            official_allocated_block = satisfy_alloc_block_req(7, allocation_size, 1);
        }
    }

    //should always be allocated by this point no matter what

    sf_block* payload_of_alloc_block = (sf_block*) ((char*) official_allocated_block + 8);

    return payload_of_alloc_block; //returns pointer to payload of allocated block
}


//PAYLOAD CHECK
void sf_free(void *pp) {

    sf_block* ptr = (sf_block*) ((char*) pp - 8);

    if((pp == NULL) || ((ptr->header & 0x10) != 0x10))
        abort();

    size_t hdr = ptr->header ^ (1<<4);

    if((hdr < 32) || (hdr % 32 != 0))
        abort();

    if(((void*)((char*) pp - 8) < sf_mem_start()) ||((void*)((char*)pp+hdr-8) > sf_mem_end())){
        abort();
    }

    int pp_alignment = hdr & 0xF;

    if(pp_alignment != 0){
        abort();
    }

    //now free the block

    size_t* footer_ptr = (size_t*)((char *)ptr + hdr - 8);
    size_t footer = ptr->header;
    *footer_ptr = footer;

    coalesce(0, ptr);

    return;
}



void *sf_realloc(void *pp, size_t rsize) {
    sf_block* ptr = (sf_block*) ((char*) pp - 8);

    if((pp == NULL) || ((ptr->header & 0x10) != 0x10))
        abort();

    size_t hdr = ptr->header ^(1<<4);

    if((hdr < 32) || (hdr % 32 != 0))
        abort();

    if(((void*)((char*) pp - 8) < sf_mem_start()) ||((void*)((char*)pp+hdr-8) > sf_mem_end())){
        abort();
    }

    int pp_alignment = hdr & 0xF;

    if(pp_alignment != 0){
        abort();
    }

    if(rsize == 0){
        sf_free(ptr);
        return NULL;
    }

    //allocate to larger size:
    if(rsize > (hdr-16)){
        sf_block* new_block_pp = sf_malloc(rsize);


        if(!new_block_pp){
            return NULL;
        }
        memcpy(new_block_pp, pp, rsize);

        sf_free(pp);

        return new_block_pp;

    }
    else if(rsize < (hdr-16)){
        sf_block* upper_address = attempt_split(ptr, rsize+16);

        if(!upper_address){
            int alloc_size = rsize+16;
            while(alloc_size%32 != 0){
                alloc_size++;  //now I have total block size that went to the first block fo the split
            }
            ptr->header = alloc_size;
            ptr->header |= 0x10;
            size_t footer = ptr->header;
            size_t* footer_address = (size_t*) ((char*) ptr + hdr - 8);
            *footer_address = footer;  //set footer of block

            sf_block* new_payload = (sf_block*) ((char*) ptr + 8);
            return new_payload;
        }
        else{
            int first_block_size = rsize + 16;
            while(first_block_size%32 != 0){
                first_block_size++;  //now I have total block size that went to the first block fo the split
            }
            ptr->header = first_block_size;
            ptr->header |=0x10;
            upper_address-> header = hdr - first_block_size;

            sf_block* upper_payload = (sf_block*) ((char*) ptr + 8);
            sf_free(upper_payload);

            size_t footer = ptr->header;
            size_t* footer_address = (size_t*) ((char*) ptr + first_block_size - 8);
            *footer_address = footer;  //set footer of block

            sf_block* new_payload = (sf_block*) ((char*) ptr + 8);
            return new_payload;
        }
    }
    return NULL;
}

void *sf_memalign(size_t size, size_t align) {
    return NULL;
}
