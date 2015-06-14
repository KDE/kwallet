#!/bin/sh
$XGETTEXT `find . -name "*.cpp" -o -name "*.h"` -o $podir/kwallet-query.pot
