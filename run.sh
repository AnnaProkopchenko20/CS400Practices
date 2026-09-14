#!/bin/bash

set -e

echo "Building the compiler via CMake..."
mkdir -p build
cd build
# Standard CMake configuration (defaults to 'Unix Makefiles' on Ubuntu)
cmake ..
# Build the 'compiler' target
cmake --build . --target compiler
cd ..

set +e

echo -e "\nStarting Test Suite..."

for test_file in tests/*.txt; do
    # Skip CMakeLists.txt if the glob picks it up
    if [[ "$(basename "$test_file")" == "CMakeLists.txt" ]]; then
        continue
    fi

    echo "--------------------------------------------------"
    echo "Testing: $test_file"

    # Run the compiled executable
    ./build/compiler "$test_file" "output.ll"
    COMPILER_EXIT_CODE=$?

    if [ $COMPILER_EXIT_CODE -eq 0 ]; then
        echo "-> Compiler generated output.ll successfully."

        # Compile IR to object file and link it to an executable
        llc -filetype=obj -relocation-model=pic output.ll -o output.o
        clang -fPIE output.o -o program

        echo "-> Running program:"
        ./program

        # Clean up the generated artifacts
        rm output.ll output.o program
    else
        echo "-> Compiler rejected the file with exit code $COMPILER_EXIT_CODE (Expected for fail tests)."
    fi
done

echo "--------------------------------------------------"
echo "Test suite finished."
