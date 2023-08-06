#!/bin/bash

#if git commit --dry-run -a > /dev/null; then
#    echo "Uncommitted changes"
#    exit
#fi

set -e
    
# Clock version number
(cd flashy && npm version prerelease --preid=alpha)

# Build bootloader
(cd bootloader && make aarch -B)

## Package it
(cd flashy && npm publish --access public)

# Commit
VERSION=`node -p require\(\"./flashy/package.json\"\).version`
git add .
git commit -m "Build $VERSION"
git push


