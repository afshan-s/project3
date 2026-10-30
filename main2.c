#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define PAGE_SIZE 256
#define NUM_PAGES 256
#define NUM_FRAMES 128
#define TLB_SIZE 16
#define MEMORY_SIZE (PAGE_SIZE * NUM_FRAMES)

//stores page number and its matching frame number in the TLB
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

    //check that all files opened successfully
    if (address_file == NULL || backing_store == NULL || out1 == NULL || out2 == NULL || out3 == NULL) {
        fprintf(stderr, "Error: Could not open a required file.\n");

        if (address_file != NULL) fclose(address_file);
        if (backing_store != NULL) fclose(backing_store);
        if (out1 != NULL) fclose(out1);
        if (out2 != NULL) fclose(out2);
        if (out3 != NULL) fclose(out3);

        return 1;
    }

    //physical memory of 128 frames w/256 bytes in each frame 
    signed char physical_memory[MEMORY_SIZE];
    //-1 means the page isn't currently in memory
    int page_table[NUM_PAGES];
    //set each page table entry to -1 to show that no pages are loaded yet 
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

    //keeps track of which TLB entry will be replaced next using FIFO
    int next_tlb_entry = 0;

    //assign in order as new pages are loaded
    int next_free_frame = 0;
    
    //keep track of next frame to replace when memory is full
    int next_frame_to_replace = 0;
    
    //stores which page is currently in each frame
    int frame_to_page[NUM_FRAMES];

    //set each frame to -1 to show that it is empty
    for (int i = 0; i < NUM_FRAMES; i++) {
        frame_to_page[i] = -1;
    }

    int page_faults = 0;
    int tlb_hits = 0;
    int total_addresses = 0;

    int input_address;
    int scan_result;

    //process each logical address in the input file
    while ((scan_result = fscanf(address_file, "%d", &input_address)) == 1) {
        total_addresses++;
    
    uint16_t logical_address = (uint16_t)((uint32_t)input_address & 0xFFFFu);

    //find page number and offset (logical address)
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

        // if frame number = -1 the requested page isn't in memory
        if (frame_number == -1) {
            page_faults++;

            //use next available frame if the memory isn't full yet
            if (next_free_frame < NUM_FRAMES) {
                frame_number = next_free_frame;
                next_free_frame++;
            }
            else {
                //replace the oldest page in physical memory
                frame_number = next_frame_to_replace;

                 int old_page = frame_to_page[frame_number];

            //mark the old page-table entry as invalid
            page_table[old_page] = -1;

            //remove the old page's entry   
            for (int i = 0; i < TLB_SIZE; i++) {
                if (tlb[i].valid && tlb[i].page_number == old_page) {
                    tlb[i].valid = 0;
                    tlb[i].page_number = -1;
                    tlb[i].frame_number = -1;
                }
            }     

            //move to the next frame for the next fifo replacement
            next_frame_to_replace = (next_frame_to_replace + 1) % NUM_FRAMES;
        }    

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

            //update page table page -> frame mapping
            page_table[page_number] = frame_number;

            //record which page is currently stored in this frame
            frame_to_page[frame_number] = page_number;
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
        //check for incorrect input data 
        if (scan_result != EOF) {
            fprintf(stderr, "Error: Invalid data in the address file.\n");

            fclose(address_file);
            fclose(backing_store);
            fclose(out1);
            fclose(out2);
            fclose(out3);

            return 1;
        }

        //calculate rates 
        double page_fault_rate = 0.0;
        double tlb_hit_rate = 0.0;

        if (total_addresses > 0) {
            page_fault_rate = (double)page_faults / total_addresses;

            tlb_hit_rate = (double)tlb_hits / total_addresses;
        }

        //prints stats to terminal instead of output files
        printf("Total addresses: %d\n", total_addresses);
        printf("Page faults: %d\n", page_faults);
        printf("Page-fault rate: %.3f\n", page_fault_rate);
        printf("TLB hits: %d\n", tlb_hits);
        printf("TLB hit rate: %.3f\n", tlb_hit_rate);

        //closes all files
        fclose(address_file);
        fclose(backing_store);
        fclose(out1);
        fclose(out2);
        fclose(out3);
        return 0;

        }
    