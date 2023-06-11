#include "sim_mem.h"


int main() {
    char* exec_file;
    char* swap_file;
    sim_mem mem_sm("exec_file", "swap_file" , 16, 32,32, 32, 16, 8);

    mem_sm.store(1025,'$');
    mem_sm.print_memory();
    mem_sm.print_page_table();

    mem_sm.store(3079,'%');
    mem_sm.print_memory();
    mem_sm.print_page_table();

    mem_sm.store(1048,'(');
    mem_sm.print_memory();
    mem_sm.print_page_table();

    mem_sm.load(3079);
    mem_sm.print_memory();
    mem_sm.print_page_table();
    mem_sm.print_swap();

    char c = mem_sm.load (1035);
    mem_sm.print_memory();
    mem_sm.print_page_table();
    mem_sm.print_swap();

    mem_sm.load(15);
    mem_sm.print_memory();
    mem_sm.print_page_table();
    mem_sm.print_swap();


    mem_sm.load (1025);
    mem_sm.print_memory();
    mem_sm.print_page_table();
    mem_sm.print_swap();

    mem_sm.load(1031);
    mem_sm.print_memory();
    mem_sm.print_page_table();
    mem_sm.print_swap();

    mem_sm.load(7);
    mem_sm.print_memory();
    mem_sm.print_page_table();
    mem_sm.print_swap();

    mem_sm.load(15);
    mem_sm.print_memory();
    mem_sm.print_page_table();
    mem_sm.print_swap();

    mem_sm.load(1031);
    mem_sm.print_memory();
    mem_sm.print_page_table();
    mem_sm.print_swap();
}
