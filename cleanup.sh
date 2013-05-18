#! /bin/bash

find . -type d -exec chmod 755 {} \;
find . -type f -exec chmod 644 {} \;
find . -type f -name '*.@(o|lst)' -exec rm {} \;

chmod 744 cleanup.sh
