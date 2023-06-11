#include "sim_mem.h"

char main_memory[MEMORY_SIZE];

sim_mem::sim_mem(char exe_file_name[], char swap_file_name[], int text_size, int data_size, int bss_size, int heap_stack_size, int num_of_pages, int page_size)
{
    if (exe_file_name == nullptr)
    {
        std::cout << "ERR" << std::endl;
        return;
    }

    if (swap_file_name == nullptr)
    {
        std::cout << "ERR" << std::endl;
        return;
    }


    // Open the executable file
    program_fd = open(exe_file_name, O_RDONLY);

    // Check if the executable file was successfully opened
    if (program_fd == -1)
    {
        // If not, print an error message and exit the program
        perror("ERR\n");
        exit(EXIT_FAILURE);
    }


    // Open the swap file in read/write mode, or create it if it doesn't exist
    swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    // Check if the swap file was successfully opened or created
    if (swapfile_fd == -1)
    {
        // If not, print an error message and exit the program
        perror("ERR\n");
        close (program_fd);
        exit(EXIT_FAILURE);
    }


    // Init main memory with 0s
    for (char& element : main_memory)
        element = '0';



    // Save arguments in variables
    this->swap_size = data_size + bss_size + heap_stack_size;
    this->page_size = page_size;
    this->heap_stack_size = heap_stack_size;
    this->bss_size = bss_size;
    this->data_size = data_size;
    this->text_size = text_size;
    this->num_of_pages = num_of_pages;
    this->inner_table_size = std::log2(page_size);
    this->frames_status = new bool[MEMORY_SIZE / page_size];
    this->swap_status = new bool[data_size + bss_size + heap_stack_size];
    this->clock = 0;
    this->frames_clock = new int[MEMORY_SIZE / page_size];



    // Init the swap file with 0s
    char value = '0';
    for (int i = 0; i < swap_size ; i++)
        if (!write_to_file(swapfile_fd, i, &value, sizeof(char)))
            return;

    // Init swap_status
    for (int i = 0; i < swap_size ; i++)
        swap_status[i] = true;

    // Init frames_status and frames_clock
    for (int i = 0 ; i < MEMORY_SIZE / page_size ; i++)
    {
        frames_status[i] = true;
        frames_clock[i] = -1;
    }

    // Init page table
    page_table = new page_descriptor*[OUTER_TABLE_SIZE];
    int page_split[] = {text_size, data_size, bss_size, heap_stack_size};

    for (int i = 0 ; i < OUTER_TABLE_SIZE ; i++)
    {
        int num_pages = page_split[i] / page_size;  // Number of pages in the current section
        page_table[i] = new page_descriptor[num_pages];

        for (int j = 0; j < num_pages; j++)
            init_page(&page_table[i][j]);
    }
}

char sim_mem::load(int address)
{
    int outer, inside, offset;
    long logical_address = get_logical_address(address);

    get_physical_address(logical_address, &outer, &inside, &offset);
    if (!is_legal(outer, inside))
    {
        std::cout << "ERR" << std::endl;
        return '\0';
    }


    // Page is in memory
    if (page_table[outer][inside].valid)
    {
        update_frames_clock(outer, inside);
        return get_memory_content(outer, inside, offset);
    }

    // txt page
    if (outer == 0)
    {
        if (load_to_memory(&page_table[outer][inside], program_fd, page_size * inside))
            return get_memory_content(outer, inside, offset);

        return '\0';
    }

    // Read from swap file
    if (page_table[outer][inside].dirty)
    {
        if (load_to_memory(&page_table[outer][inside], swapfile_fd, -1))
            return get_memory_content(outer, inside, offset);

        return '\0';
    }

    // Heap_stack page
    if (outer == 3)
    {
        std::cout << "ERR" << std::endl;
        return '\0';
    }


    // data page
    if (outer == 1)
    {
        if (load_to_memory(&page_table[outer][inside], program_fd, text_size + (inside * page_size)))
            return get_memory_content(outer, inside, offset);

        return '\0';
    }

    // BSS page
    if (load_to_memory(&page_table[outer][inside], program_fd, text_size + data_size + (inside * page_size)))
        return get_memory_content(outer, inside, offset);

    return '\0';
}


char sim_mem::get_memory_content(int outer, int inside, int offset)
{
    return main_memory[page_table[outer][inside].frame * page_size + offset];
}


void sim_mem::update_frames_clock(int outer, int inside)
{
    frames_clock[page_table[outer][inside].frame] = clock;
    clock++;
}


void sim_mem::store(int address , char value)
{
    int outer, inside, offset;
    long logical_address = get_logical_address(address);

    get_physical_address(logical_address, &outer, &inside, &offset);
    if (!is_legal(outer, inside))
    {
        std::cout << "ERR" << std::endl;
        return;
    }


    // Page is in memory
    if (page_table[outer][inside].valid)
    {
        update_frames_clock(outer, inside);
        write_to_memory(outer, inside, offset, value);
        return;
    }

    // txt page - may not write
    if (outer == 0)
    {
        std::cout << "ERR" << std::endl;
        return;
    }

    // Read from swap file
    if (page_table[outer][inside].dirty)
    {
        if (load_to_memory(&page_table[outer][inside], swapfile_fd, -1))
            write_to_memory(outer, inside, offset, value);

        return;
    }

    // Data page
    if (outer == 1)
    {
        if (load_to_memory(&page_table[outer][inside], program_fd, text_size + (inside * page_size)))
            write_to_memory(outer, inside, offset, value);

        return;
    }

    // heap_stake or bss page - init a new page
    if (load_to_memory(&page_table[outer][inside], NEW_PAGE, -1))
        write_to_memory(outer, inside, offset, value);
}


void sim_mem::write_to_memory(int outer, int inside, int offset, char value)
{
    main_memory[page_table[outer][inside].frame * page_size + offset] = value;
    page_table[outer][inside].dirty = true;
}


bool sim_mem::is_legal(int outer, int inner)
{
    if (outer < 0 || outer > 3 || inner < 0)
        return false;


    int page_split[] = {text_size, data_size, bss_size, heap_stack_size};

    // Check whether inner index exceeds the maximum for the given outer index
    if (inner >= page_split[outer] / page_size)
        return false;

    return true;
}

void sim_mem::init_page(page_descriptor* pd)
{
    pd->valid = false;
    pd->frame = -1;
    pd->dirty = false;
    pd->swap_index = -1;
}

void sim_mem::print_memory()
{
    int i;
    printf("\n Physical memory\n");
    for(i = 0; i < MEMORY_SIZE; i++) {
        printf("[%c]\n", main_memory[i]);
    }
}


void sim_mem::print_swap()
{
    char* str = (char*) malloc(this->page_size *sizeof(char));
    int i;
    printf("\n Swap memory\n");
    lseek(swapfile_fd, 0, SEEK_SET); // go to the start of the file
    while(read(swapfile_fd, str, this->page_size) == this->page_size)
    {
        for(i = 0; i < page_size; i++) {
            printf("%d - [%c]\t", i, str[i]);
        }
        printf("\n");
    }

    free (str);
}

void sim_mem::print_page_table()
{
    int i;
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for(i = 0; i < text_size / page_size; i++) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[0][i].valid,
               page_table[0][i].dirty,
               page_table[0][i].frame ,
               page_table[0][i].swap_index);
    }
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for(i = 0; i < data_size / page_size ; i++) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[1][i].valid,
               page_table[1][i].dirty,
               page_table[1][i].frame ,
               page_table[1][i].swap_index);
    }
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for(i = 0; i < bss_size / page_size ; i++) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[2][i].valid,
               page_table[2][i].dirty,
               page_table[2][i].frame ,
               page_table[2][i].swap_index);
    }
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for(i = 0; i < heap_stack_size / page_size ; i++) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[3][i].valid,
               page_table[3][i].dirty,
               page_table[3][i].frame ,
               page_table[3][i].swap_index);
    }
}

sim_mem::~sim_mem ()
{
    close(swapfile_fd);
    close(program_fd);
    delete[] frames_status;
    delete[] swap_status;
    delete[] frames_clock;

    for (int i = 0 ; i < OUTER_TABLE_SIZE ; i++)
        delete[] page_table[i];

    delete[] page_table;
}


long sim_mem::get_logical_address(long num)
{
    long binary = 0;
    long base = 1;

    // Convert to binary using bitwise operations
    while (num > 0)
    {
        long remainder = num & 1;  // Get the least significant bit
        binary += remainder * base;
        num >>= 1;  // Right shift num by 1 bit
        base *= 10;
    }

    return binary;
}


void sim_mem::get_physical_address(long address, int* outer, int* inside, int* offset) const
{
    std::stringstream ss;
    ss << std::setw(12) << std::setfill('0') << address;

    std::string binary = ss.str();

    std::string outerStr = binary.substr(0, 2);
    std::string offsetStr = binary.substr(binary.length() - inner_table_size, inner_table_size);
    std::string insideStr = binary.substr(2, binary.length() - 2 - inner_table_size);

    *outer = std::stoi(outerStr, nullptr, 2);
    *inside = std::stoi(insideStr, nullptr, 2);
    *offset = std::stoi(offsetStr, nullptr, 2);
}

char* sim_mem::read_from_file(int fd, int location, int amount)
{
    char* buffer = new char[amount]; // Allocate memory for amount

    if (lseek(fd, location, SEEK_SET) == -1) // Set the file position indicator to the location
    {
        perror("ERR\n");
        return nullptr;
    }

    ssize_t bytes_read = read(fd, buffer, amount); // Read the data into the buffer

    if (bytes_read == -1) // Check if the read function failed
    {
        perror("ERR\n");
        delete[] buffer;
        return nullptr;
    }

    else if (bytes_read == 0) // Check if we've reached the end of the file
    {
        std::cout << "ERR" << std::endl;
        delete[] buffer;
        return nullptr;
    }

    return buffer;
}

bool sim_mem::write_to_file(int fd, off_t location, const char* data, size_t size)
{
    ssize_t bytes_written = pwrite(fd, data, size, location);

    if (bytes_written == -1)
    {
        perror("ERR\n");
        return false;
    }

    return true;
}

bool sim_mem::load_to_memory(page_descriptor* p, int fd, int location)
{
    int memory_location = get_memory_space(); // Get the first available memory space location
    char* data;

    // No memory space is available
    if (memory_location == -1)
    {
        if (!clear_memory_page()) // Clear a page from the memory and check if failed to do so
        {
            std::cout << "ERR" << std::endl;
            return false;
        }

        memory_location = get_memory_space(); // Check if memory space is available
    }

    // If load to memory from swap file
    if (fd == swapfile_fd)
    {
        data = read_from_file(fd, p->swap_index * page_size, page_size);

        if (data == nullptr)
            return false;

        // delete the page from the swap file
        char value = '0';
        for (int i = p->swap_index * page_size ; i < (p->swap_index * page_size) + page_size; i++)
            if (!write_to_file(swapfile_fd, i, &value, sizeof(char)))
            {
                delete[] data;
                return false;
            }

        swap_status[p->swap_index] = true;
        p->swap_index = -1;

    }

        // If load to memory from exe file
    else if (fd == program_fd)
    {
        data = read_from_file(fd, location, page_size);

        if (data == nullptr)
            return false;
    }

        // If load to memory a new page
    else if (fd == NEW_PAGE)
    {
        data = new char[page_size];
        for (int i = 0 ; i < page_size ; i++)
            data[i] = '0';
    }


    // Copy page content into the memory
    for (int i = 0 ; i < page_size ; i++)
        main_memory[i + (memory_location * page_size)] = data[i];


    // Set the page's new attributes
    (*p).frame = memory_location;
    (*p).valid = true;

    // Update frame's clock for LRU
    frames_clock[memory_location] = clock;
    clock++;

    // Update frame's availability
    frames_status[(*p).frame] = false;

    delete[] data;
    return true;
}

int sim_mem::get_swap_space(int amount) const
{
    for (int i = 0; i < swap_size ; i++)
        if (swap_status[i])
            return i;

    return -1;
}

int sim_mem::get_memory_space()
{
    for (int i = 0 ; i < MEMORY_SIZE / page_size ; i++)
        if (frames_status[i])
            return i;

    return -1; // No space available
}

bool sim_mem::clear_memory_page()
{
    int page_split[] = {text_size, data_size, bss_size, heap_stack_size};
    bool found = false;
    int min_time = INT_MAX;
    int frame_to_remove;
    int outer, inner;

    // Check which frame should be cleared
    for (int i = 0 ; i < MEMORY_SIZE / page_size ; i++)
    {
        if (frames_clock[i] < min_time)
        {
            min_time = frames_clock[i];
            frame_to_remove = i;
        }
    }

    // Check which page is located in the found frame
    for (int i = 0 ; i < OUTER_TABLE_SIZE && !found ; i++)
    {
        int num_pages = page_split[i] / page_size;  // Number of pages in the current section
        for (int j = 0; j < num_pages; j++)
            if (frame_to_remove == page_table[i][j].frame)
            {
                found = true;
                inner = j;
                outer = i;
                break;
            }
    }

    // Found no page to remove (no valid pages)
    if (!found)
        return false;

    // Remove the page with the longest time from memory
    page_table[outer][inner].valid = false;
    frames_status[page_table[outer][inner].frame] = true; // Frame is now available
    frames_clock[frame_to_remove] = 0;

    // Page not dirty - not needed to store in swap file
    if (!page_table[outer][inner].dirty)
    {
        for (int i = 0; i < page_size ; i++)
            main_memory[page_table[outer][inner].frame * page_size + i] = '0';


        page_table[outer][inner].frame = -1;
        return true;
    }

    // Load the removed page to swap file
    int location = get_swap_space(page_size);
    if (location == -1)
        return false;

    for (int i = 0; i < page_size ; i++)
    {
        if (!write_to_file(swapfile_fd, location * page_size + i, &main_memory[page_table[outer][inner].frame * page_size + i],sizeof(char)))
            return false;

        main_memory[page_table[outer][inner].frame * page_size + i] = '0';
    }

    page_table[outer][inner].frame = -1;
    page_table[outer][inner].swap_index = location;
    swap_status[location] = false;

    return true;
}