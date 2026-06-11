# UART Frame Parser Assessment

**Candidate Name:** Deva T

## Module Description

### uart_parser.c

Standalone UART frame parser implementation demonstrating incremental UART frame processing, XOR checksum verification, inter-byte timeout handling, and assessment test execution through `main()`.

## Build Instructions

Compile using GCC:

```bash
gcc -Wall -std=c99 uart_parser.c -o uart_parser
```

The program compiles cleanly with zero warnings and zero errors.

## Execution Instructions

Run the program using:

```bash
./uart_parser
```

On Windows (MinGW), execute:

```bash
uart_parser.exe
```
