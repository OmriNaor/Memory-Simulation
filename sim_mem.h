#ifndef EX4_SIM_MEM_H
#define EX4_SIM_MEM_H

// Required libraries and headers
#include <string>
#include <cstring>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <cmath>
#include <climits>

// Constants for the simulation
#define OUTER_TABLE_SIZE 4
#define MEMORY_SIZE 16
#define NEW_PAGE (-1)

extern char main_memory[MEMORY_SIZE];  // The main memory of the simulated system

// Page descriptor structure for each page
typedef struct page_descriptor
{
    bool valid;       // Is the page currently in memory?
    int frame;        // The frame where the page is loaded in memory
    bool dirty;       // Has the page been modified since it was loaded?
    int swap_index;   // The location of the page in the swap file
} page_descriptor;

using std::string;

// Class for simulating memory management
class sim_mem {

    int swapfile_fd;       // File descriptor for the swap file
    int program_fd;        // File descriptor for the executable file
    int text_size;         // Size of the .text section
    int data_size;         // Size of the .data section
    int bss_size;          // Size of the .bss section
    int heap_stack_size;   // Size of the heap/stack
    int page_size;         // Size of a single page
    page_descriptor **page_table; // Pointer to the page table
    int swap_size;         // Size of the swap file
    int inner_table_size;  // Size of the inner page table
    bool* frames_status;   // Array to track the status of each frame in memory
    bool* swap_status;     // Array to track the status of each page in the swap file
    int* frames_clock;     // Array to track the "age" of each frame in memory
    int clock;             // The current time step in the simulation

public:
    sim_mem(char exe_file_name[], char swap_file_name[], int text_size, int data_size, int bss_size, int heap_stack_size, int page_size);  // Constructor
    ~sim_mem();  // Destructor
    char load(int address);  // Load a byte from the given address
    void store(int address, char value);  // Store a byte to the given address
    static void print_memory();  // Print the current state of the memory
    void print_swap () const;  // Print the current state of the swap file
    void print_page_table();  // Print the current state of the page table

private:
    static long get_logical_address(long num);  // Function to get logical address from a given number
    void get_physical_address(long address, int* outer, int* inner, int* offset) const;  // Function to get physical address from a given logical address
    static char* read_from_file(int fd, int location, int amount);  // Function to read from file
    static bool write_to_file(int fd, off_t location, const char* data, size_t size);  // Function to write to file
    bool load_to_memory(page_descriptor* p, int fd, int location);  // Function to load page to memory
    bool clear_memory_page();  // Function to clear memory page
    int get_memory_space();  // Function to get available memory space
    int get_swap_space() const;  // Function to get available swap space
    static void init_page(page_descriptor* pd);  // Function to initialize page descriptor
    bool is_legal(int outer, int inner);  // Function to check if address is legal
    char get_memory_content(int outer, int inner, int offset);  // Function to get content from memory
    void update_frames_clock(int outer, int inner);  // Function to update frames clock
    void write_to_memory(int outer, int inner, int offset, char value);  // Function to write value to memory
};

#endif
