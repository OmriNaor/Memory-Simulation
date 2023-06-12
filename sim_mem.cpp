#include "sim_mem.h"

char main_memory[MEMORY_SIZE];

/**
 * This constructor opens provided executable and swap files, initializes memory,
 * page table and related arrays. It also sets up the swap file and calculates
 * the size of the inner table based on the page size. In case of file opening failures,
 * the program will exit with an error.
 *
 * @param exe_file_name: The name of the executable file.
 * @param swap_file_name: The name of the swap file.
 * @param text_size: Size of the text segment.
 * @param data_size: Size of the data segment.
 * @param bss_size: Size of the BSS segment.
 * @param heap_stack_size: Size of the heap/stack.
 * @param num_of_pages: Number of pages in the memory.
 * @param page_size: Size of each page in the memory.
 */
sim_mem::sim_mem(char exe_file_name[], char swap_file_name[], int text_size, int data_size, int bss_size,
                 int heap_stack_size, int num_of_pages, int page_size)
{
    // Checking if the executable file name is provided
    if (exe_file_name == nullptr)
    {
        std::cerr << "Error: Missing executable file name." << std::endl;
        return;
    }

    // Checking if the swap file name is provided
    if (swap_file_name == nullptr)
    {
        std::cerr << "Error: Missing swap file name." << std::endl;
        return;
    }

    // Opening the executable file
    program_fd = open(exe_file_name, O_RDONLY);

    // Error checking for the executable file opening
    if (program_fd == -1)
    {
        perror("Error: Failed to open the executable file");
        exit(EXIT_FAILURE);
    }

    // Opening/creating the swap file in read/write mode
    swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    // Error checking for the swap file opening/creation
    if (swapfile_fd == -1)
    {
        perror("Error: Failed to open/create the swap file");
        close(program_fd);
        exit(EXIT_FAILURE);
    }

    // Initializing main memory with 0s
    for (char &element: main_memory)
        element = '0';

    // Storing the passed arguments into instance variables
    this->swap_size = data_size + bss_size + heap_stack_size;
    this->page_size = page_size;
    this->heap_stack_size = heap_stack_size;
    this->bss_size = bss_size;
    this->data_size = data_size;
    this->text_size = text_size;
    this->inner_table_size = std::log2(page_size);
    this->frames_status = new bool[MEMORY_SIZE / page_size];
    this->swap_status = new bool[data_size + bss_size + heap_stack_size];
    this->clock = 0;
    this->frames_clock = new int[MEMORY_SIZE / page_size];

    // Initializing the swap file with 0s
    char value = '0';
    for (int i = 0; i < swap_size; i++)
        if (!write_to_file(swapfile_fd, i, &value, sizeof(char)))
        {
            std::cerr << "Error: Failed to initialize the swap file." << std::endl;
            return;
        }

    // Initializing swap_status array with true values
    for (int i = 0; i < swap_size; i++)
        swap_status[i] = true;

    // Initializing frames_status and frames_clock arrays
    for (int i = 0; i < MEMORY_SIZE / page_size; i++)
    {
        frames_status[i] = true;
        frames_clock[i] = -1;
    }

    // Initializing page table
    page_table = new page_descriptor* [OUTER_TABLE_SIZE];
    int page_split[] = {text_size, data_size, bss_size, heap_stack_size};

    for (int i = 0; i < OUTER_TABLE_SIZE; i++)
    {
        int num_pages = page_split[i] / page_size;  // Calculating the number of pages in the current section
        page_table[i] = new page_descriptor[num_pages];

        for (int j = 0; j < num_pages; j++)
            init_page(&page_table[i][j]); // Initializing each page in the page table
    }
}

/**
 * Fetches a byte from the specified address in the simulated memory.
 *
 * The function processes a logical address, converting it into physical terms. Based on the page type
 * (text, data, BSS, heap/stack), the page is loaded from the relevant file. If loading is not
 * possible, an error is signaled and a null character is returned.
 *
 * @param address: The logical address to load from.
 *
 * @return The byte from the given address, or '\0' in case of an error.
 */
char sim_mem::load(int address)
{
    int outer, inner, offset;

    // Convert the physical address to a logical address
    long logical_address = get_logical_address(address);

    // Get the table indices and offset for this address
    get_physical_address(logical_address, &outer, &inner, &offset);

    // If the address is not legal, print an error and return null character
    if (!is_legal(outer, inner))
    {
        std::cerr << "Address is out of bound!" << std::endl;
        return '\0';
    }

    // If the page is already in memory, update the frames clock and return the memory content
    if (page_table[outer][inner].valid)
    {
        update_frames_clock(outer, inner);
        return get_memory_content(outer, inner, offset);
    }

    // If the page is a text page, load it into memory from the program file
    if (outer == 0)
    {
        if (load_to_memory(&page_table[outer][inner], program_fd, page_size * inner))
            return get_memory_content(outer, inner, offset);

        return '\0';
    }

    // If the page is dirty, load it from the swap file
    if (page_table[outer][inner].dirty)
    {
        if (load_to_memory(&page_table[outer][inner], swapfile_fd, -1))
            return get_memory_content(outer, inner, offset);

        return '\0';
    }

    // If the page is a heap/stack page, print an error and return null character (can not load such page for the first time - it has to be created via store)
    if (outer == 3)
    {
        std::cerr << "May not load a new heap/stack page!" << std::endl;
        return '\0';
    }

    // If the page is a data page, load it from the program file
    if (outer == 1)
    {
        if (load_to_memory(&page_table[outer][inner], program_fd, text_size + (inner * page_size)))
            return get_memory_content(outer, inner, offset);

        return '\0';
    }

    // If the page is a BSS page, load it from the program file
    if (load_to_memory(&page_table[outer][inner], program_fd, text_size + data_size + (inner * page_size)))
        return get_memory_content(outer, inner, offset);

    // If none of the above conditions are met, return null character
    return '\0';
}


/**
 * Retrieves a byte from the simulated memory at a specific location.
 *
 * Given the indices of the outer page, inner page and offset within the page, this function
 * calculates the exact position in the main memory and returns the byte at that location.
 *
 * @param outer: Index of the outer page in the page table.
 * @param inner: Index of the inner page in the page table.
 * @param offset: Offset within the page.
 *
 * @return The byte stored at the calculated location in the main memory.
 */
char sim_mem::get_memory_content(int outer, int inner, int offset)
{
    return main_memory[page_table[outer][inner].frame * page_size + offset];
}

/**
 * Updates the access time for a given page in memory.
 *
 * This function is used to keep track of when a page was last accessed. It updates the
 * 'frames_clock' at the frame index for the given outer and inner page indices to the current
 * clock value, and then increments the clock.
 *
 * @param outer: Index of the outer page in the page table.
 * @param inner: Index of the inner page in the page table.
 */
void sim_mem::update_frames_clock(int outer, int inner)
{
    frames_clock[page_table[outer][inner].frame] = clock;
    clock++;
}


/**
 * Writes a given value into memory at a specified address.
 *
 * This function translates the provided logical address into a physical one, and
 * attempts to store the value at the translated address. If the page isn't in memory,
 * it loads the page before storing the value. Text pages are read-only and cannot be written to.
 *
 * @param address: The logical memory address to store the value at.
 * @param value: The value to store in memory.
 */
void sim_mem::store(int address, char value)
{
    // Convert logical address to physical address components
    int outer, inner, offset;
    long logical_address = get_logical_address(address);
    get_physical_address(logical_address, &outer, &inner, &offset);

    // Check if the address is valid
    if (!is_legal(outer, inner))
    {
        std::cerr << "Address is out of bound!" << std::endl;
        return;
    }

    // If the page is in memory
    if (page_table[outer][inner].valid)
    {
        // Update the access time and write the value to memory
        update_frames_clock(outer, inner);
        write_to_memory(outer, inner, offset, value);
        return;
    }

    // If it's a text page, writing is not allowed
    if (outer == 0)
    {
        std::cerr << "Writing to a text page is not allowed!" << std::endl;
        return;
    }

    // If the page is in the swap file
    if (page_table[outer][inner].dirty)
    {
        // Load the page into memory and then write the value
        if (load_to_memory(&page_table[outer][inner], swapfile_fd, -1))
            write_to_memory(outer, inner, offset, value);
        return;
    }

    // If it's a data page
    if (outer == 1)
    {
        // Load the page from the program file into memory and then write the value
        if (load_to_memory(&page_table[outer][inner], program_fd, text_size + (inner * page_size)))
            write_to_memory(outer, inner, offset, value);
        return;
    }

    // If it's a heap_stake or bss page, initialize a new page
    if (load_to_memory(&page_table[outer][inner], NEW_PAGE, -1))
        write_to_memory(outer, inner, offset, value);
}


/**
 * Writes a value to a specific location in the physical memory.
 *
 * This function receives the physical address in the form of outer, inner, and offset, and then writes
 * the provided value into the specified location. It also marks the related page as dirty.
 *
 * @param outer: The outer index of the page table.
 * @param inner: The inner index of the page table.
 * @param offset: The offset inner the page.
 * @param value: The value to write in the memory.
 */
void sim_mem::write_to_memory(int outer, int inner, int offset, char value)
{
    // Write the provided value to the specified physical memory location
    main_memory[page_table[outer][inner].frame * page_size + offset] = value;

    // Mark the page as dirty, indicating it has been written to and may need to be written back to disk
    page_table[outer][inner].dirty = true;
}


/**
 * Checks if the provided indices for the page table are within the legal bounds.
 *
 * @param outer: The outer index of the page table.
 * @param inner: The inner index of the page table.
 *
 * @return: True if the indices are within legal bounds, false otherwise.
 */
bool sim_mem::is_legal(int outer, int inner)
{
    // Check if the outer index is within the legal range (0-3)
    if (outer < 0 || outer > 3 || inner < 0)
        return false;

    // Array representing the sizes of different sections of memory
    int page_split[] = {text_size, data_size, bss_size, heap_stack_size};

    // Check if the inner index is within the legal range for the given outer index
    // by comparing it against the size of the corresponding section
    if (inner >= page_split[outer] / page_size)
        return false;

    return true;
}


/**
 * Initializes a page descriptor.
 *
 * @param pd: The page descriptor to be initialized.
 */
void sim_mem::init_page(page_descriptor* pd)
{
    pd->valid = false;       // Set the valid flag to false, indicating the page is not yet in memory.
    pd->frame = -1;         // Initialize the frame number to -1, meaning it is not yet assigned.
    pd->dirty = false;      // Set the dirty flag to false, indicating no modifications have been made.
    pd->swap_index = -1;    // Initialize the swap index to -1, meaning it is not yet assigned.
}


/**
 * Prints the current state of the physical memory.
 */
void sim_mem::print_memory()
{
    int i;
    printf("\n Physical memory\n");
    for (i = 0; i < MEMORY_SIZE; i++)
    {
        printf("[%c]\n", main_memory[i]);
    }
}


/**
 * Prints the current state of the swap file.
 */
void sim_mem::print_swap() const
{
    char* str = (char*) malloc(this->page_size * sizeof(char));
    int i;
    printf("\n Swap memory\n");
    lseek(swapfile_fd, 0, SEEK_SET); // go to the start of the file
    while (read(swapfile_fd, str, this->page_size) == this->page_size)
    {
        for (i = 0; i < page_size; i++)
        {
            printf("%d - [%c]\t", i, str[i]);
        }
        printf("\n");
    }

    free(str);
}

/**
 * Prints the current state of the page table.
 */
void sim_mem::print_page_table()
{
    int i;
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for (i = 0; i < text_size / page_size; i++)
    {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[0][i].valid,
               page_table[0][i].dirty,
               page_table[0][i].frame,
               page_table[0][i].swap_index);
    }
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for (i = 0; i < data_size / page_size; i++)
    {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[1][i].valid,
               page_table[1][i].dirty,
               page_table[1][i].frame,
               page_table[1][i].swap_index);
    }
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for (i = 0; i < bss_size / page_size; i++)
    {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[2][i].valid,
               page_table[2][i].dirty,
               page_table[2][i].frame,
               page_table[2][i].swap_index);
    }
    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for (i = 0; i < heap_stack_size / page_size; i++)
    {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[3][i].valid,
               page_table[3][i].dirty,
               page_table[3][i].frame,
               page_table[3][i].swap_index);
    }
}


/**
 * Destructor for the sim_mem class.
 *
 * Closes file descriptors, deallocates dynamic memory for the frames status, swap status, frames clock and the page table.
 */
sim_mem::~sim_mem()
{
    close(swapfile_fd);
    close(program_fd);
    delete[] frames_status;
    delete[] swap_status;
    delete[] frames_clock;

    for (int i = 0; i < OUTER_TABLE_SIZE; i++)
        delete[] page_table[i];

    delete[] page_table;
}


/**
 * Converts a decimal number to its binary representation.
 *
 * @param num: The decimal number to be converted.
 *
 * @return: The binary representation of the input number.
 */
long sim_mem::get_logical_address(long num)
{
    long binary = 0;  // Initialize binary number
    long base = 1;    // Initialize base to 1, i.e 2^0

    // Convert decimal to binary using bitwise operations
    while (num > 0)
    {
        long remainder = num & 1;  // Get the least significant bit by bitwise AND with 1
        binary += remainder * base;
        num >>= 1;  // Right shift num by 1 bit to process the next bit
        base *= 10;
    }

    return binary;  // Return the calculated binary number
}


/**
 * Translates a logical address into its respective physical address components.
 *
 * @param address: The logical address to be translated.
 * @param outer: A pointer to the location where the outer component of the physical address is to be stored.
 * @param inner: A pointer to the location where the inner component of the physical address is to be stored.
 * @param offset: A pointer to the location where the offset component of the physical address is to be stored.
 */
void sim_mem::get_physical_address(long address, int* outer, int* inner, int* offset) const
{
    std::stringstream ss;
    ss << std::setw(12) << std::setfill('0') << address; // Convert the address to a string, padding with zeros as needed.

    std::string binary = ss.str(); // Get the string representation of the binary address.

    // Extract the relevant bits for each component of the physical address.
    std::string outerStr = binary.substr(0, 2);
    std::string offsetStr = binary.substr(binary.length() - inner_table_size, inner_table_size);
    std::string insideStr = binary.substr(2, binary.length() - 2 - inner_table_size);

    // Convert each component from binary string to integer and store it in the provided locations.
    *outer = std::stoi(outerStr, nullptr, 2);
    *inner = std::stoi(insideStr, nullptr, 2);
    *offset = std::stoi(offsetStr, nullptr, 2);
}


/**
 * Reads data from a file at a specified location.
 *
 * @param fd: The file descriptor of the file to be read.
 * @param location: The location in the file from where data needs to be read.
 * @param amount: The number of bytes to read.
 *
 * @return: A pointer to a buffer containing the data read, or null if an error occurred.
 */
char* sim_mem::read_from_file(int fd, int location, int amount)
{
    char* buffer = new char[amount]; // Allocate memory to store the data that will be read from the file.

    // Reposition the file offset to the specified location.
    if (lseek(fd, location, SEEK_SET) == -1)
    {
        perror("Unable to reposition file offset: ");
        return nullptr; // Return null if there's an error.
    }

    // Read the specified amount of data into the buffer.
    ssize_t bytes_read = read(fd, buffer, amount);

    // Check if the read operation was successful.
    if (bytes_read == -1)
    {
        perror("Read operation failed: ");
        delete[] buffer; // Free the buffer if there's an error.
        return nullptr; // Return null if there's an error.
    }
    else if (bytes_read == 0) // Check if we've reached the end of the file.
    {
        std::cerr << "Reached end of file without reading any data" << std::endl;
        delete[] buffer; // Free the buffer if there's an error.
        return nullptr; // Return null if there's an error.
    }

    return buffer; // Return the buffer containing the data.
}


/**
 * Writes data to a file at a specified location.
 *
 * @param fd: The file descriptor of the file to be written to.
 * @param location: The location in the file where data needs to be written.
 * @param data: The data to be written.
 * @param size: The size of the data to be written.
 *
 * @return: True if the data was successfully written, false otherwise.
 */
bool sim_mem::write_to_file(int fd, off_t location, const char* data, size_t size)
{
    // Write the data to the file at the specified location.
    ssize_t bytes_written = pwrite(fd, data, size, location);

    // Check if the write operation was successful.
    if (bytes_written == -1)
    {
        perror("Error writing to file: ");
        return false; // Return false if there was an error.
    }

    return true; // Return true if the data was successfully written.
}


/**
 * Loads content into memory from either the swap file, the program file, or initializes a new page.
 *
 * @param p: Pointer to the page descriptor.
 * @param fd: The file descriptor, which determines the source of the data to be loaded.
 * @param location: The location in the file to read from.
 *
 * @return: True if the operation is successful, false otherwise.
 */
bool sim_mem::load_to_memory(page_descriptor* p, int fd, int location)
{
    // Find the first available memory space location.
    int memory_location = get_memory_space();
    char* data;

    // If no memory space is available.
    if (memory_location == -1)
    {
        if (!clear_memory_page()) // If failed to clear a page from the memory.
        {
            std::cerr << "Failed to clear a page from memory" << std::endl;
            return false;
        }
        memory_location = get_memory_space(); // Try to get available memory space again.
    }

    // If loading from swap file.
    if (fd == swapfile_fd)
    {
        data = read_from_file(fd, p->swap_index * page_size, page_size);

        if (data == nullptr)
            return false;

        // Delete the page from the swap file.
        char value = '0';
        for (int i = p->swap_index * page_size; i < (p->swap_index * page_size) + page_size; i++)
            if (!write_to_file(swapfile_fd, i, &value, sizeof(char)))
            {
                delete[] data;
                std::cerr << "Failed to delete the page from the swap file" << std::endl;
                return false;
            }

        // Update swap status and reset page's swap index.
        swap_status[p->swap_index] = true;
        p->swap_index = -1;
    }
        // If loading from program file.
    else if (fd == program_fd)
    {
        data = read_from_file(fd, location, page_size);

        if (data == nullptr)
            return false;
    }
        // If loading a new page.
    else if (fd == NEW_PAGE)
    {
        data = new char[page_size];
        for (int i = 0; i < page_size; i++)
            data[i] = '0';
    }

    // Copy the data into the memory.
    for (int i = 0; i < page_size; i++)
        main_memory[i + (memory_location * page_size)] = data[i];

    // Set the page's new attributes.
    (*p).frame = memory_location;
    (*p).valid = true;

    // Update the frame's clock for LRU policy.
    frames_clock[memory_location] = clock;
    clock++;

    // Update the frame's availability status.
    frames_status[(*p).frame] = false;

    delete[] data;
    return true;
}

/**
 * Finds the first available space in the swap file.
 *
 * @return: The index of the available space, or -1 if no space is available.
 */
int sim_mem::get_swap_space() const
{
    for (int i = 0; i < swap_size; i++)
        if (swap_status[i])
            return i;

    return -1;
}

/**
 * Finds the first available space in the main memory.
 *
 * @return The index of the available space, or -1 if no space is available.
 */
int sim_mem::get_memory_space()
{
    for (int i = 0; i < MEMORY_SIZE / page_size; i++)
        if (frames_status[i])
            return i;

    return -1; // No space available
}

/**
 * Clears a page from the main memory using the LRU (Least Recently Used) algorithm.
 *
 * @return True if a page was successfully cleared, false otherwise.
 */
bool sim_mem::clear_memory_page()
{
    int page_split[] = {text_size, data_size, bss_size,
                        heap_stack_size}; // Array to store sizes of different sections of memory
    bool found = false; // Flag to indicate if a page to remove is found
    int min_time = INT_MAX; // Variable to store the minimum time value
    int frame_to_remove; // Variable to store the frame index to remove
    int outer, inner; // Variables to store the outer and inner table indices

    // Check which frame should be cleared using the LRU algorithm
    for (int i = 0; i < MEMORY_SIZE / page_size; i++)
    {
        if (frames_clock[i] < min_time) // Check if the current frame has a lower time value
        {
            min_time = frames_clock[i];
            frame_to_remove = i;
        }
    }

    // Check which page is located in the found frame
    for (int i = 0; i < OUTER_TABLE_SIZE && !found; i++)
    {
        int num_pages = page_split[i] / page_size; // Calculate the number of pages in the current section
        for (int j = 0; j < num_pages; j++)
        {
            if (frame_to_remove == page_table[i][j].frame) // Check if the current page is mapped to the frame to remove
            {
                found = true;
                inner = j;
                outer = i;
                break;
            }
        }
    }

    // Found no page to remove (no valid pages)
    if (!found)
        return false;

    // Remove the page with the shortest time from memory
    page_table[outer][inner].valid = false; // Mark the page as invalid
    frames_status[page_table[outer][inner].frame] = true; // Mark the frame as available
    frames_clock[frame_to_remove] = 0; // Reset the time value of the removed frame

    // Page not dirty - not needed to store in swap file
    if (!page_table[outer][inner].dirty)
    {
        for (int i = 0; i < page_size; i++)
            main_memory[page_table[outer][inner].frame * page_size + i] = '0'; // Clear the memory of the removed page

        page_table[outer][inner].frame = -1; // Reset the frame index
        return true;
    }

    // Load the removed page to swap file
    int location = get_swap_space(); // Get an available location in the swap file
    if (location == -1)
        return false;

    for (int i = 0; i < page_size; i++)
    {
        if (!write_to_file(swapfile_fd, location * page_size + i,
                           &main_memory[page_table[outer][inner].frame * page_size + i], sizeof(char)))
            return false; // Write the page content to the swap file

        main_memory[page_table[outer][inner].frame * page_size + i] = '0'; // Clear the memory of the removed page
    }

    page_table[outer][inner].frame = -1; // Reset the frame index
    page_table[outer][inner].swap_index = location; // Update the swap index of the removed page
    swap_status[location] = false; // Mark the swap location as occupied

    return true; // Page removal was successful
}
