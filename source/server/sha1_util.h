#ifndef SHA1UTIL_H_
#define SHA1UTIL_H_

#include <string>

bool toHex(char *result, unsigned char *data, unsigned int len);
bool SHA1FromString(char *result, const char *source);
bool SHA1FromBuffer(char *result, const char *source, int len);
bool SHA1FromString(std::string &result, const std::string &sourceStr);
int getFileHash(char *filename, char *hash);
bool sha1check();

#endif
