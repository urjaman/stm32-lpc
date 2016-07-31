void dmacpy_setup(void);
int dmacpy_w(void *dest, const void* src, size_t words, void(*cb)(int));
