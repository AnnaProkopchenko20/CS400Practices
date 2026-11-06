int isqrt(int v)
{
    int r = 0;
    while (r + 1 <= v / (r + 1))
        ++r;
    return r;
}
