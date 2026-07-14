<div align="center">

# MPI Game of Life
**2D Torus Topology Distributed Cellular Automaton**

[![C](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![MPI](https://img.shields.io/badge/Framework-OpenMPI-orange.svg)](https://www.open-mpi.org/)
[![Platform](https://img.shields.io/badge/Platform-Ubuntu-E95420.svg)](https://ubuntu.com/)
[![License](https://img.shields.io/badge/License-Afeka-green.svg)](#-license)

*A Parallel and Distributed Computing (Course 10324) Project*

</div>

---

## About The Project

A distributed implementation of an automated cellular automaton (Game of Life) using Message Passing Interface (MPI) in C[cite: 1, 5]. 
This project simulates a simplified version of Conway's Game of Life on a **2D Cartesian Torus grid**. The global matrix is divided into local blocks distributed across multiple processes for efficient parallel computation.

### Key Features
* **Torus Topology:** 2D Cartesian Torus implementation (`MPI_Cart_create`).
* **Halo/Ghost Cells Exchange:** Safe boundary communication between 4 direct neighbors (Up, Down, Left, Right) using `MPI_Cart_shift` and `MPI_Sendrecv` to prevent deadlocks.
* **Distributed Architecture:** 
  * Process `0` reads the initial state from `matrix.txt` and distributes $B \times B$ blocks.
  * Worker processes compute next generation rules and exchange boundaries concurrently.
  * Process `0` gathers updated blocks to reconstruct the final global matrix[cite: 1].
* **Automated Testing:** Shell script integration for comprehensive validation against expected outcomes[cite: 1, 6].

## Repository Structure

| File/Folder | Description |
| :--- | :--- |
| `game_of_life_solution.c` | The core MPI source code implementation[cite: 5] |
| `matrix.txt` | Input configuration: dimensions, iterations, and initial layout[cite: 5] |
| `gol_simulator.html` | Visual web simulator to test edge cases[cite: 1] |
| `run_tests.sh` | Bash script for automated compilation and testing[cite: 5] |
| `tests/` | Test configurations (blinker, glider, torus, etc.) and expected outputs[cite: 5] |

## Getting Started

### Prerequisites
The project is designed for an **Ubuntu Linux** environment using **OpenMPI**[cite: 1, 5].
*Note: If you have MPICH installed, please replace it with OpenMPI to support the `--oversubscribe` flag[cite: 5].*

<details>
<summary><b>Show OpenMPI Installation Commands</b></summary>

```bash
# Remove MPICH
sudo apt-get remove --purge mpich libmpich-dev
sudo apt-get autoremove

# Install OpenMPI
sudo apt-get update
sudo apt-get install openmpi-bin openmpi-common libopenmpi-dev
