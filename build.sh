#!/bin/bash

# Clock version number
cd flashy ; npm version patch

# Build bootloader
cd bootloader ; make cleanall ; make bootloader.zip

# Package it
cd flashy ; npm publish --access public
