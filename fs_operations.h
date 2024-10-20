#ifndef FS_OPERATIONS_H
#define FS_OPERATIONS_H

#include <LittleFS.h>

bool writeFile(String path, String data, const bool binary=false, const bool overwrite=false);
String readFile(String path, const bool binary=false);

bool findFile(String path);
bool deleteFile(String path);
bool renameFile(String from, String to);

#endif // FS_OPERATIONS_H
