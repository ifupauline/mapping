#ifndef FILE_H
#define FILE_H

#define BUFFSIZE  256

#include "mapping.h"

void write_header(FILE *file, const char *header);

void write_data(FILE *file, vector<float> &v_float);

#endif /* FILE.H */   
