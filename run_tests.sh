#!/bin/bash

# 1. Compile the code
echo "========================================="
echo "Compiling game_of_life_solution.c..."
mpicc game_of_life_solution.c -o main

if [ $? -ne 0 ]; then
    echo "Compilation failed! Aborting tests."
    exit 1
fi
echo "Compilation successful."
echo "========================================="

# 2. Define the tests and their required process counts (test_name processes)
TESTS=(
    "absolute_death_test 4"
    "blinker_test 4"
    "still_life_test 4"
    "torus_test 4"
    "glider_test 9"
    "grid_test 9"
)

echo -e "\n          STARTING TEST SUITE            "
echo "========================================="

# 3. Iterate through the tests and run them
for test_info in "${TESTS[@]}"; do
    # Split the string into variables
    read -r test_name np <<< "$test_info"

    echo -e "\n---> Running: $test_name (Processes: $np)"
    mpirun --oversubscribe -np $np ./main "tests/$test_name"
    echo "-----------------------------------------"
done

echo -e "\nAll tests completed!"