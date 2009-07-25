/* memory copy support */

void * memcpy(void * dest, const void *src, unsigned long count)
{
        char *tmp = (char *) dest, *s = (char *) src;

        while (count--)
                *tmp++ = *s++;

        return dest;
}
