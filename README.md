# C++ Project: Operating System Memory Simulation

## Introduction

This project is a simple and lightweight implementation of an operating system memory simulation. The simulator uses a two-layer process page table and simulates the pages of a single process, where each page is made up of characters.

## About

This simulator supports a variety of options, including:

- Loading a text page from a file representing the executable program.
- Removing a page from memory when it's full.
- Saving dirty pages into a swap file.
- Init a new page for undirty heap_stack pages. 

## Structure

- `main.cpp`: This file contains the main function definition. Here, the user (representing the operating system) chooses whether to get or store a character from a specific page.
- `sim_mem.cpp`: This file contains the class responsible for performing the simulation. It includes the load/store functions, which convert a logical address given by the user (representing the operating system) into a physical address and load the required page into memory.

## The Algorithm

The program follows the following algorithm:

1. Take a logical page address for load or store commands from the user (representing the operating system).
2. Four page types describe the memory: text, data, bss, and heap_stack.
3. Check the given input and handle it accordingly, whether it's a load or store command, based on the page type.
4. If possible, convert the given logical address to a physical address and load the required page into memory (unless it's already there).
5. When the memory is full and the required page is not present, use the LRU (Least Recently Used) algorithm to remove one page from memory and make space.
6. For store commands, write into the offset in the page. For load commands, return the value of the offset of the page.

## Remarks:

- Refer to the following diagram for the flow of the memory simulation:

![Memory Flow](https://github.com/OmriNaor/Memory-Simulation/assets/106623821/af424667-2b77-4fa3-b46a-7f6154612737)

## Getting Started

To compile and run the project, follow these steps:

1. Clone the repository or download the source code.
2. Download the txt file (representing the executable file) from the repository and place it in the project's directory.
3. Navigate to the project directory.
4. Compile the project using a C++ compiler (e.g., g++): `g++ main.cpp sim_mem.cpp -o simulator`
5. Run the compiled executable: `./simulator`

## Examples

- Swap file saving the dirty pages:

![Swap](https://github.com/OmriNaor/Memory-Simulation/assets/106623821/a0c0221b-cc00-4c8f-997c-f0fca34cb0db)

- Main memory containing two pages:

![Memory](https://github.com/OmriNaor/Memory-Simulation/assets/106623821/c475aa4a-3d7e-41f9-863d-e933584208eb)

- Page table current status:

![Page Table](https://github.com/OmriNaor/Memory-Simulation/assets/106623821/517cee05-9351-4bbe-b22e-3a59b0280476)
