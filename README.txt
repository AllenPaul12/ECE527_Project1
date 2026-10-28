========================================================================
   ECE527_Project1 RETIMING PROJECT - Allen Harrison Paul (954353816)
========================================================================

This REAMDE.txt file has step by step instructions on how to run 
the programs as well as what each program does.


FILE ARCHITECTURE AND MODULE DESCRIPTION
-------------------------------------------
The codebase and approach I used is modularized into header, implementation, and test files:

- retiming.h
  * Primary header file containing type definitions and function declarations.
  * Defines structural models for `Circuit` (nodes, delays, edge weights),
    `RetimingResult` (W matrix, D matrix, retiming vector r), and `Edge`.
  * Exposes initialization, parsing, matrix generation, constraint-solving,
    and output formatting interfaces.

- parser.c
  * File-parsing module responsible for reading circuit descriptions.
  * Strips C-style comments (`//`) and parses parameters: TotalNodes, 
    NodeDelays, Edge definitions, and MaxClockCycle constraints.

- retiming.c
  * Core algorithmic engine implementing Leiserson-Saxe theory:
    1. init_circuit(): Clears graphs and sets non-adjacent edge weights to INF.
    2. compute_W_and_D(): 
       - Calculates Matrix W using Floyd-Warshall all-pairs shortest paths.
       - Calculates Matrix D by measuring maximum zero-register combinational
         subpath delays along W-critical paths.
    3. solve_constraints(): Constructs retiming difference constraints:
       - Legal retiming bounds: r(u) - r(v) <= w(e)
       - Clock period bounds: r(u) - r(v) <= W(u, v) - 1 for D(u, v) > c
       - Uses Bellman-Ford to detect negative cycles (feasibility check).
    4. run_retiming_algorithm(): Iteratively checks clock periods from the 
       initial max clock down to c_min to find the optimal retiming vector r(v).

- main.c
  * Program entry point. Coordinates CLI argument handling, calls the file parser,
    triggers retiming execution, and formats output matrices and edge retiming details.


ClaudeAI Generated File:
-verify.c
    * This program links against my retiming.c and parser.c. It deliberately shares no algorithmic logic with them, so agreement means something.
     It does three checks:
        > Brute-forces W and D by enumerating every simple path.
        > Brute-forces the true minimum clock period over all retiming vectors, then checks that your c_min equals it and that your r is legal and actually achieves it.
        > Runs this on your three circuit files plus thousands of random circuits.

- Makefile
  * Automated build script supporting target compilation (`make`), suite testing
    (`make test`), cleanups (`make clean`), and object management.


INPUT CIRCUIT TEST FILES
---------------------------
- example_class.txt   : 4-node reference circuit from class slides (Optimal c_min = 2).
- 5_node_pipeline.txt : 5-node pipeline circuit with a delay bottleneck (Optimal c_min = 4).
- multi_loop.txt      : 4-node multi-loop network constrained by ring delay (Optimal c_min = 5).


STEP-BY-STEP INSTRUCTIONS TO BUILD AND RUN
----------------------------------------------

PREREQUISITES:
- GCC compiler (MinGW/GCC on Windows PowerShell or native GCC on Linux/macOS)
- GNU Make utility

OPTION 1: USING THE MAKEFILE

  1. Open terminal/PowerShell in the project folder.
  
  2. Compile the project:
     make
     
  3. Run the automated test suite across all 3 test circuits:
     make test
     
  4. Run an individual circuit file manually:
     ./retiming.exe example_class.txt
     ./retiming.exe 5_node_pipeline.txt
     ./retiming.exe multi_loop.txt

  5. Clean object files and binary executable:
     make clean


OPTION 2: DIRECT GCC COMPILATION (PowerShell / Command Line)

  1. Compile all C source files into the executable:
     gcc -Wall -Wextra -O2 -std=c11 main.c retiming.c parser.c -o retiming.exe

  2. Execute against test circuits:
     ./retiming.exe example_class.txt
     ./retiming.exe 5_node_pipeline.txt
     ./retiming.exe multi_loop.txt
