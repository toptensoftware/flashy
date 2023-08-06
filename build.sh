#!/bin/bash

#if git commit --dry-run -a > /dev/null; then
#    echo "Uncommitted changes"
#    exit
#fi
    
# Clock version number
pushd flashy
npm version prerelease --preid=alpha
popd

# Build bootloader
pushd bootloader
cd bootloader && make aarch -B
popd

## Package it
pushd flashy
cd flashy && npm publish --access public
popd

# Commit
VERSION=`node -p require\(\"./flashy/package.json\"\).version`
git add .
git commit -m "Build $VERSION"
git push


