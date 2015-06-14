#!/usr/bin/env bash
# this script converts the adoc file to docbook
# it uses asciidoc tool a2x
a2x -v -d manpage -f docbook kwallet-query.adoc
tr -d '\015' <kwallet-query.xml >man-kwallet-query.1.docbook
rm kwallet-query.xml
