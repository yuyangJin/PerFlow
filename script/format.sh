#!/bin/bash
set -x
SCRIPT_DIR=$(cd "$(dirname "$0")"; pwd) 

if [ ! $(which clang-format) ]; then
  echo "Cannot find clang-format, Please install it!"
fi

find $SCRIPT_DIR/../include -iname *.h -o -iname *.cpp | xargs clang-format -i -style=file
find $SCRIPT_DIR/../src -iname *.h -o -iname *.cpp | xargs clang-format -i -style=file
find $SCRIPT_DIR/../example -iname *.h -o -iname *.cpp | xargs clang-format -i -style=file
# find $SCRIPT_DIR/../test -iname *.h -o -iname *.cpp | xargs clang-format -i -style=file
find $SCRIPT_DIR/../builtin -iname *.h -o -iname *.cpp | xargs clang-format -i -style=file

