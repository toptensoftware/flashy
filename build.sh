#!/bin/bash

## Clock version number
#cd flashy ; npm version patch
#
## Build bootloader
#cd bootloader ; make cleanall ; make bootloader.zip
#
## Package it
#cd flashy ; npm publish --access public
#
## Commit
#git add .
#VERSION=`node -p require\(\"./flashy/package.json\"\).version`
#git commit -m "Build $VERSION"
#git push


git commit --dry-run -a
echo $?