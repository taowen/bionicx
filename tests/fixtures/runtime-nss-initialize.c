/* Stand-in for GreD libnss3.so: Firefox loads this first, then softoken. */
int NSS_Initialize(void) {
    return 0;
}
