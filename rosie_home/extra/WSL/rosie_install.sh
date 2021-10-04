#!/bin/bash

# This is an EXAMPLE script and may or may not work for your system.
# It is an illustration of one way to install Rosie and its
# prerequisites on Windows Subsystem for Linux (WSL).

# Made for WSL on ubuntu, and tested on linux mint, ubuntu, and centOs
if [[ $(command apt-get) ]]; then
    # Dependencies:
    sudo apt-get update;
    sudo apt-get upgrade;
    # Some Os's do not come with the standard lib in their lib c
    sudo apt-get install build-essentials
    # Some Os's apparently do not come with lib c
    sudo apt-get install gcc;
    #########################
    sudo apt-get install g++; # This is for the special case of bare bones os's that seperate c's standard lib
    #########################
    sudo apt-get install make;
    # This covers both readline commons and realine dev
    sudo apt-get install libbsd-dev libreadline-dev;
    # Copying the repo:
    git clone https://gitlab.com/rosie-pattern-language/rosie;
    cd ./rosie/;
    sudo apt-get update;
    #Installation:
    sudo make install;
# this is for yum based package managers
elif [[ $(command yum) ]]; then
    sudo yum install gcc; 
    sudo yum install g++;
    sudo yum install make;
    sudo yum install libbsd-devel libreadline-dev;
    # Fedora's version of build essentials that make sure if they are using a 
    # light weight os that they have the specific packages needed to run make it
    yum groupinstall "Development Tools" "Development Libraries";
    git clone https://gitlab.com/rosie-pattern-language/rosie;
    cd ./rosie/;
    sudo make install;
else 
    echo "Please install a package manager, either yum, or apt-get";
fi
