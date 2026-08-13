#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    struct group *android = getgrgid(10148);
    if (android == NULL || strcmp(android->gr_name, "u0_a148") != 0 ||
            android->gr_mem == NULL || android->gr_mem[0] == NULL ||
            strcmp(android->gr_mem[0], android->gr_name) != 0 ||
            android->gr_mem[1] != NULL)
        return 1;

    if (setenv("BIONICX_VIRTUAL_ROOT", "1", 1) != 0)
        return 2;
    if (getgrgid(148) != NULL || getgrgid(10148) != NULL)
        return 3;

    puts("Android glibc identity namespace: PASS");
    return 0;
}
