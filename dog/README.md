# Dog

## Goal
The goal of this program, aside from learning the lab format and setting up the environment properly, is to mimic the “cat” command in linux with some minor changes.

## Setup the Environment
* Install Ubuntu 18.04 LTS or any other linux distribution that can support gcc
* Install gcc with `sudo apt install build-essential`
* compile using `make`
* remove build files using `make clean`

## Running the code
* `./dog` echoes user input. press `ctrl+c` to exit.
* `./dog - > outfile` echoes user input into an outfile. press `ctrl+c` to exit.
* `./dog file` will print the contents of the file to the screen.
* `./dog file1 file2 file3` will print the contents of the files to the screen.
* `./dog file1 file2 file3 > outfile` will concatenate the files into an outfile.
* All errors should be the same as cat.

## Constraints
* There is no support for flags, so basic functionality of cat is all dog can do.
* cannot use it on directories (no recursive functionality)
* 32KiB MAX BUF size
