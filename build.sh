#!/bin/bash

#if git commit --dry-run -a > /dev/null; then
#    echo "Uncommitted changes"
#    exit
#fi

set -e
    
# Clock version number
#npm version patch

# Build bootloader
#(cd bootloader && make aarch -B)

# Commit
VERSION=`node -p require\(\"./package.json\"\).version`
git add .
git commit -m "Build $VERSION"
git push


