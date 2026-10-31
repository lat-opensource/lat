typedef unsigned int u32;
struct nl {
    u32 len;
    unsigned short type, flags;
    u32 seq, pid;
};
struct addr {
    unsigned short family, pad;
    u32 pid, groups;
};
static int sc(int n, int a, int b, int c)
{
    int r;
    __asm__ volatile("int $0x80"
                     : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c)
                     : "memory");
    return r;
}
static void die(void)
{
    sc(1, 1, 0, 0);
}
static int sock(int op, int *args)
{
    return sc(102, op, (int)args, 0);
}
void _start(void)
{
    int args[6] = {16, 3, 0}, fd = sock(1, args);
    if (fd < 0) {
        die();
    }
    struct addr a = {16, 0, 0, 0};
    args[0] = fd;
    args[1] = (int)&a;
    args[2] = sizeof(a);
    if (sock(2, args)) {
        die();
    }
    struct {
        struct nl h;
        unsigned char family, pad;
        unsigned short type;
        int index;
        u32 flags, change;
    } req = {{32, 18, 0x301, 77, 0}, 0};
    args[1] = (int)&req;
    args[2] = sizeof(req);
    args[3] = 0;
    args[4] = (int)&a;
    args[5] = sizeof(a);
    if (sock(11, args) != sizeof(req)) {
        die();
    }
    char buf[4096];
    int total = 0, done = 0;
    while (!done) {
        args[0] = fd;
        args[1] = (int)buf;
        args[2] = sizeof(buf);
        args[3] = 0;
        int got = sock(10, args);
        if (got <= 0) {
            die();
        }
        for (int pos = 0; pos < got;) {
            struct nl *h = (void *)(buf + pos);
            if (got - pos < 16 || h->len < 16 || h->len > got - pos) {
                die();
            }
            if (h->type == 16) {
                total++;
            }
            if (h->type == 3) {
                done = 1;
            }
            pos += (h->len + 3) & ~3;
        }
    }
    if (!total) {
        die();
    }
    sc(4, 1, (int)"PASS i386 socketcall\n", 20);
    sc(1, 0, 0, 0);
}
