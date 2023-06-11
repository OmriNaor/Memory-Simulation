#ifndef EX4_SIM_MEM_H
#define EX4_SIM_MEM_H

#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <cmath>
#include <climits>

#define OUTER_TABLE_SIZE 4
#define MEMORY_SIZE 16
#define NEW_PAGE (-1)

extern char main_memory[MEMORY_SIZE];
typedef struct page_descriptor
{
    bool valid;
    int frame;
    bool dirty;
    int swap_index;
} page_descriptor;


using std::string;

class sim_mem {

    int swapfile_fd; //swap file fd
    int program_fd; //executable file fd
    int text_size;
    int data_size;
    int bss_size;
    int heap_stack_size;
    int num_of_pages;
    int page_size;
    page_descriptor **page_table; //pointer to page table
    int swap_size;
    int inner_table_size;
    bool* frames_status;
    bool* swap_status;
    int* frames_clock;
    int clock;

public:

    sim_mem(char exe_file_name[], char swap_file_name[], int text_size, int data_size, int bss_size, int heap_stack_size, int num_of_pages, int page_size);
    ~sim_mem();
    char load(int address);
    void store(int address, char value);
    void print_memory();
    void print_swap ();
    void print_page_table();

private:
    static long get_logical_address(long num);
    void get_physical_address(long address, int* outer, int* inside, int* offset) const;
    static char* read_from_file(int fd, int location, int amount);
    static bool write_to_file(int fd, off_t location, const char* data, size_t size);
    bool load_to_memory(page_descriptor* p, int fd, int location);
    bool clear_memory_page();
    int get_memory_space();
    int get_swap_space(int amount) const;
    static void init_page(page_descriptor* pd);
    bool is_legal(int outer, int inner);
    char get_memory_content(int outer, int inside, int offset);
    void update_frames_clock(int outer, int inside);
    void write_to_memory(int outer, int inside, int offset, char value);
};


#endif