#!/bin/bash

# This very ugly script converts a kernel font to a DAT file
cat $1 |  grep -o '0x[0-9a-fA-F]\{2\},' | sed 's/^.*0x\([0-9a-fA-F]\{2\}\)\,.*$/\1/' | head -1024 > $2