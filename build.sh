#!/bin/bash

if git commit --dry-run -a > /dev/null; then
    echo "Uncommitted changes"
    exit
fi
    
# Clock version number
(cd flashy ; npm version patch)

# Build bootloader
(cd bootloader ; make cleanall aarch)

## Package it
(cd flashy ; npm publish --access public)

# Commit
VERSION=`node -p require\(\"./flashy/package.json\"\).version`
git add .
git commit -m "Build $VERSION"
git push


