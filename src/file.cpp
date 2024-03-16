#include "file.h"

void write_header(FILE *file, const char *header)
{
        fprintf(file, "%s", header);
        fprintf(file, "\n");
}

void write_data(FILE *file, vector<float> &v_float)
{
        for (unsigned int i = 0; i < v_float.size(); i++) {
                fprintf(file, "%-3.2f", v_float[i]);
                if (i == v_float.size() - 1) {
                        fprintf(file, "\n");
                        break;
                }
                fprintf(file, ",");
        }
}
