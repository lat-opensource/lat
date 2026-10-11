__attribute__((noinline))
static int calculate(void)
{
    volatile int step = 7;
    int result = 0;
    for (int i = 0; i < 6; i++) {
        result += step;
    }
    return result;
}

int guest_main(void)
{
    return calculate();
}
