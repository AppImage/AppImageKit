#! /bin/bash

# converts a built runtime to an embeddable C file, and generates an appropriate header for it
# (the latter is required to make code inspection work with IDEs)

xxd -i runtime > runtime-embed.c
echo '/* header for the embeddable runtime file */' > runtime-embed.h
echo -n 'unsigned char runtime[' >> runtime-embed.h
grep 'unsigned int' runtime-embed.c | cut -d'=' -f2 | cut -d';' -f1 | awk '{print $1}' | tr -d '\n' >> runtime-embed.h
echo '];' >> runtime-embed.h
echo 'unsigned int runtime_len;' >> runtime-embed.h
