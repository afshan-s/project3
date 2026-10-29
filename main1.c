#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define PAGE_SIZE 256
#define NUM_PAGES 256
#define NUM_FRAMES 256
#define TLB_SIZE 16
#define MEMORY_SIZE (PAGE_SIZE * NUM_FRAMES)

//TLB entry stores page number and its corresponding frame
typedef struct {
    int page_number;
    int frame_number;
    int valid;
} TLBEntry;

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s addresses.txt\n", argv[0]);
        return 1;
    }

    FILE *address_file = fopen(argv[1], "r");
    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");
    FILE *out1 = fopen("out1.txt", "w");
    FILE *out2 = fopen("out2.txt", "w");
    FILE *out3 = fopen("out3.txt", "w");

    //check if every required file is opening succesfully
    if (address_file == NULL || backing_store == NULL || out1 == NULL || out2 == NULL || out3 == NULL) {
        fprintf(stderr, "Error: Could not open a required file.\n");

        if (address_file != NULL) fclose(address_file);
        if (backing_store != NULL) fclose(backing_store);
        if (out1 != NULL) fclose(out1);
        if (out2 != NULL) fclose(out2);
        if (out3 != NULL) fclose(out3);

        return 1;
    }

    //physical memory of 256 frame, every one holding 256 bytes
    signed char physical_memory[MEMORY_SIZE];
    //-1 means the page isn't currently in memory
    int page_table[NUM_PAGES];
    //initialize every page-table entry as invalid
    for (int i = 0; i < NUM_PAGES; i++) {
        page_table[i] = -1;
    }

    //initialize all TLB entries as invalid
    TLBEntry tlb[TLB_SIZE];

    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].page_number = -1;
        tlb[i].frame_number = -1;
        tlb[i].valid = 0;
    }

    //FIFO - keeps track of the next TLB entry to replace
    int next_tlb_entry = 0;

    //Frames assigned in order when pages first enter memory
    int next_free_frame = 0;

    int page_faults = 0;
    int tlb_hits = 0;
    int total_addresses = 0;

    int input_address;
    int scan_result;

    //process each logical address in the input file
    while ((scan_result = fscanf(address_file, "%d", &input_address)) == 1) {
        total_addresses++;
    
    uint16_t logical_address = (uint16_t)((uint32_t)input_address & 0xFFFFu);

    //extracting the 8-bit page number and 8-bit offset
    int page_number = (logical_address >> 8) & 0xFF;
    int offset = logical_address & 0xFF;

    int frame_number = -1;
    int tlb_found = 0;

    //search the TLB for the requested page
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].page_number == page_number) {
            
            frame_number = tlb[i].frame_number;
            tlb_hits++;
            tlb_found = 1;
            break;
        }
    }

    //if TLB misses, check the page table
    if (!tlb_found) {
        frame_number = page_table[page_number];

        // if frame number = -1, page isn't in memory
        if (frame_number == -1) {
            page_faults++;

            //each new page gets the next available frame
            if (next_free_frame >= NUM_FRAMES) {
                fprintf(stderr, "Error: No free physical frames left.\n");

                fclose(address_file);
                fclose(backing_store);
                fclose(out1);
                fclose(out2);
                fclose(out3);

                return 1;
            }

            frame_number = next_free_frame;
            next_free_frame++;

            //read requested page from the backing store
            if (fseek(backing_store, (long)page_number * PAGE_SIZE, SEEK_SET) != 0 ||
                fread(&physical_memory[frame_number*PAGE_SIZE],
                        sizeof(signed char), PAGE_SIZE, backing_store) != PAGE_SIZE){
                fprintf(stderr, "Error: Could not read a page from" "BACKING_STORE.bin.\n");

                fclose(address_file);
                fclose(backing_store);
                fclose(out1);
                fclose(out2);
                fclose(out3);

                return 1;
            }

            //record new page-to-frame mapping
            page_table[page_number] = frame_number;
        }

            //insert page-to-frame mapping into the TLB
            //when it's full, FIFO will replace the oldest TLB entry
            tlb[next_tlb_entry].page_number = page_number;
            tlb[next_tlb_entry].frame_number = frame_number;
            tlb[next_tlb_entry].valid = 1;

            next_tlb_entry = (next_tlb_entry + 1) % TLB_SIZE;
        }

        //combine frame number and original offset
        int physical_address = frame_number*PAGE_SIZE + offset;

        //retrieve signed byte stored at that physical address
        int value = physical_memory[physical_address];

        //each output file contains numbers only
        fprintf(out1, "%d\n", input_address);
        fprintf(out2, "%d\n", physical_address);
        fprintf(out3, "%d\n", value);
    }
        }
    