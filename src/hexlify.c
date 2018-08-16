#include <string.h>
#include <stdlib.h>
#include <stdio.h>

char* appimage_hexlify(const char* bytes, const size_t numBytes) {
    // first of all, allocate the new string
    // a hexadecimal representation works like "every byte will be represented by two chars"
    // additionally, we need to null-terminate the string
    char* hexlified = (char*) calloc((2 * numBytes + 1), sizeof(char));

    for (size_t i = 0; i < numBytes; i++) {
        char buffer[3];
        sprintf(buffer, "%02x", (unsigned char) bytes[i]);
        strcat(hexlified, buffer);
    }

    return hexlified;
}
