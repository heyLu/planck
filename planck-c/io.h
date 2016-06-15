#include <time.h>

char* get_contents(char *path, time_t *last_modified);
void write_contents(char *path, char *contents);
int mkdir_p(char *path);