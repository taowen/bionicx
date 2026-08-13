#include <cups/cups.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    cups_dest_t *destinations = NULL;
    int count = cupsGetDests(&destinations);
    printf("cups-probe: destinations=%d\n", count);
    int found = 0;
    for (int i = 0; i < count; ++i) {
        printf("cups-probe: destination=%s instance=%s\n",
               destinations[i].name,
               destinations[i].instance != NULL ? destinations[i].instance : "");
        if (strcmp(destinations[i].name, "bionicx-test") == 0) found = 1;
    }
    cupsFreeDests(count, destinations);
    if (!found) {
        fprintf(stderr, "cups-probe: bionicx-test destination missing\n");
        return 1;
    }
    return 0;
}
