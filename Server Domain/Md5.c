// Server Md5
// According to this piazza post: https://piazza.com/class/l1d2ss2rhcc3uu?cid=389
// we are allowed to either use the original implementiation or a library, I have opted for the library
#include <stdio.h>
#include <stdlib.h>
#include<unistd.h>
#include <string.h>

#include <openssl/md5.h>
#define MD5_DIGEST_LENGTH 16

// This code has been taken from here: 
//    https://stackoverflow.com/questions/10324611/how-to-calculate-the-md5-hash-of-a-large-file-in-c/10324904#10324904
unsigned char* md5HashFunction(char* filename) {
    unsigned char* c;
    c = (unsigned char *)malloc(MD5_DIGEST_LENGTH * sizeof(unsigned char));
    int i;
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];

    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);
    fclose (inFile);
    
    return c;
}