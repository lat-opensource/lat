#include "config-host.h"
#include "lsenv.h"
#include "myalign.h"
#include "elfloader.h"
#include "elfloader_private.h"
#include <sys/epoll.h>
#include <sys/sem.h>
#include <sys/syscall.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#define FPU_t mmx87_regs_t
int box64_x87_no80bits = 1;
#define BIAS80 16383
#define BIAS64 1023
static TranslationBlock* kzt_add_fucn_by_addr(uint32* inst_old, CPUState *cpu, uintptr_t addr, int (*latx_ld_callback)(void *, void (*)(CPUX86State *)), void (*kzt_tb_callback)(CPUX86State *));

#ifndef HUGE_VAL
#define HUGE_VAL (1.0 / 0.0)
#endif

typedef union {
	uint64_t	q;
	int64_t		sq;
	double		d;
	float		f[2];
	uint32_t	ud[2];
	int32_t 	sd[2];
	uint16_t 	uw[4];
	int16_t 	sw[4];
	uint8_t 	ub[8];
	int8_t 		sb[8];
} mmx87_regs_t;

static int regs_abi[] = {R_EDI, R_ESI, R_EDX, R_ECX, R_R8, R_R9};
uintptr_t getVArgs(int pos, uintptr_t* b, int N)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    if((pos+N)>5)
        return b[pos+N-6];
    return cpu->regs[regs_abi[pos+N]];
}
void* getVargN(int n)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    if(n<6)
        return (void*)cpu->regs[regs_abi[n]];
    return ((void**)cpu->regs[R_ESP])[1+n-6];
}
void LD2D(void* ld, void* d)
{
    if(box64_x87_no80bits) {
        *(uint64_t*)d = *(uint64_t*)ld;
        return;
    }
	FPU_t result;
    #pragma pack(push, 1)
	struct {
		FPU_t f;
		int16_t b;
	} val;
    #pragma pack(pop)
    #if 1
    memcpy(&val, ld, 10);
    #else
	val.f.ud[0] = *(uint32_t*)ld;
    val.f.ud[1] = *(uint32_t*)(char*)(ld+4);
	val.b  = *(int16_t*)((char*)ld+8);
    #endif
	int32_t exp64 = (((uint32_t)(val.b&0x7fff) - BIAS80) + BIAS64);
	int32_t exp64final = exp64&0x7ff;
    // do specific value first (0, infinite...)
    // bit 63 is "integer part"
    // bit 62 is sign
    if((uint32_t)(val.b&0x7fff)==0x7fff) {
        // infinity and nans
        int t = 0; //nan
        switch((val.f.ud[1]>>30)) {
            case 0: if((val.f.ud[1]&(1<<29))==0) t = 1;
                    break;
            case 2: if((val.f.ud[1]&(1<<29))==0) t = 1;
                    break;
        }
        if(t) {    // infinite
            result.d = HUGE_VAL;
        } else {      // NaN
            result.ud[1] = 0x7ff << 20;
            result.ud[0] = 0;
        }
        if(val.b&0x8000)
            result.ud[1] |= 0x80000000;
        *(uint64_t*)d = result.q;
        return;
    }
    if(((uint32_t)(val.b&0x7fff)==0) || (exp64<-1074)) {
        //if(val.f.q==0)
        // zero
        //if(val.f.q!=0)
        // denormal, but that's to small value for double
        uint64_t r = 0;
        if(val.b&0x8000)
            r |= 0x8000000000000000L;
        *(uint64_t*)d = r;
        return;
    }

    if(exp64<=0 && val.f.q) {
        // try to see if it can be a denormal
        int one = -exp64-1022;
        uint64_t r = 0;
        if(val.b&0x8000)
            r |= 0x8000000000000000L;
        r |= val.f.q>>one;
        *(uint64_t*)d = r;
        return;

    }

    if(exp64>=0x7ff) {
        // to big value...
        result.d = HUGE_VAL;
        if(val.b&0x8000)
            result.ud[1] |= 0x80000000;
        *(uint64_t*)d = result.q;
        return;
    }

	uint64_t mant64 = (val.f.q >> 11) & 0xfffffffffffffL;
	uint32_t sign = (val.b&0x8000)?1:0;
    result.q = mant64;
	result.ud[1] |= (sign <<31)|((exp64final&0x7ff) << 20);

	*(uint64_t*)d = result.q;
}

#ifndef HAVE_LD80BITS
long double LD2localLD(void* ld)
{
    // local implementation may not be try Quad precision, but double-double precision, so simple way to keep the 80bits precision in the conversion
    double ret; // cannot add = 0; it break factorio (issue when calling fmodl)
    LD2D(ld, &ret);
    return ret;
}
#else
long double LD2localLD(void* ld)
{
    return *(long double*)ld;
}
#endif

void myStackAlign(const char* fmt, uint64_t* st, uint64_t* mystack, int xmm, int pos)
{
    if(!fmt)
        return;
    // loop...
    const char* p = fmt;
    int state = 0;
    __MY_CPU;
    #ifndef HAVE_LD80BITS
    double d;
    long double ld;
    #endif
    int x = 0;
    while(*p)
    {
        switch(state) {
            case 0:
                switch(*p) {
                    case '%': state = 1; ++p; break;
                    default:
                        ++p;
                }
                break;
            case 1: // normal
            case 2: // l
            case 3: // ll
            case 4: // L
            case 5: // z
                switch(*p) {
                    case '%': state = 0;  ++p; break; //%% = back to 0
                    case 'l': ++state; if (state>3) state=3; ++p; break;
                    case 'L': state = 4; ++p; break;
                    case 'z': state = 5; ++p; break;
                    case 'a':
                    case 'A':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                    case 'F':
                    case 'f': state += 10; break;    //  float
                    case 'd':
                    case 'i':
                    case 'o':
                    case 'u':
                    case 'x':
                    case 'X': state += 20; break;   // int
                    case 'h': ++p; break;  // ignored...
                    case '\'':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '.':
                    case '#':
                    case '+':
                    case '-': ++p; break; // formating, ignored
                    case 'm': state = 0; ++p; break; // no argument
                    case 'n':
                    case 'p':
                    case 'S':
                    case 's': state = 30; break; // pointers
                    case '$': ++p; break; // should issue a warning, it's not handled...
                    case '*':
                        if(pos<6)
                            *mystack = cpu->regs[regs_abi[pos++]];
                        else {
                            *mystack = *st;
                            ++st;
                        }
                        ++mystack;
                        ++p;
                        break; // fetch an int in the stack....
                    case ' ': state=0; ++p; break;
                    default:
                        state=20; // other stuff, put an int...
                }
                break;
            case 11:    //double
            case 12:    //%lg, still double
            case 13:    //%llg, still double
            case 15:    //%zg, meh.. double?
                if(xmm) {
                    lsassertm(0 , "wait for check. todo");
                    *mystack = cpu->xmm_regs->ZMM_D(x++);
                    --xmm;
                    mystack++;
                } else {
                    *mystack = *st;
                    st++; mystack++;
                }
                state = 0;
                ++p;
                break;
            case 14:    //%LG long double
                if((((uintptr_t)st)&0xf)!=0)
                    st++;
                #ifdef HAVE_LD80BITS
                if((((uintptr_t)mystack)&0xf)!=0)
                    mystack++;
                memcpy(mystack, st, 16);
                st+=2; mystack+=2;
                #else
                // there is 128bits long double on ARM64, but they need 128bit alignment
                if((((uintptr_t)mystack)&0xf)!=0)
                    mystack++;
                LD2D((void*)st, &d);
                ld = d ;
                memcpy(mystack, &ld, 16);
                st+=2; mystack+=2;
                #endif
                state = 0;
                ++p;
                break;
            case 20:    // fallback
            case 21:
            case 22:
            case 23:    // 64bits int
            case 24:    // normal int / pointer
            case 25:    // size_t int
            case 30:
                if(pos<6)
                    *mystack = cpu->regs[regs_abi[pos++]];
                else {
                    *mystack = *st;
                    ++st;
                }
                ++mystack;
                state = 0;
                ++p;
                break;
            default:
                // whaaaat?
                state = 0;
        }
    }
}
void myStackAlignW(const char* fmt, uint64_t* st, uint64_t* mystack, int xmm, int pos)
{
    // loop...
    const wchar_t* p = (const wchar_t*)fmt;
    int state = 0;
    #ifndef HAVE_LD80BITS
    double d;
    long double ld;
    #endif
    int x = 0;
    __MY_CPU;
    while(*p)
    {
        switch(state) {
            case 0:
                switch(*p) {
                    case '%': state = 1; ++p; break;
                    default:
                        ++p;
                }
                break;
            case 1: // normal
            case 2: // l
            case 3: // ll
            case 4: // L
            case 5: // z
                switch(*p) {
                    case '%': state = 0;  ++p; break; //%% = back to 0
                    case 'l': ++state; if (state>3) state=3; ++p; break;
                    case 'L': state = 4; ++p; break;
                    case 'z': state = 5; ++p; break;
                    case 'a':
                    case 'A':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                    case 'F':
                    case 'f': state += 10; break;    //  float
                    case 'd':
                    case 'i':
                    case 'o':
                    case 'u':
                    case 'x':
                    case 'X': state += 20; break;   // int
                    case 'h': ++p; break;  // ignored...
                    case '\'':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '.':
                    case '#':
                    case '+':
                    case '-': ++p; break; // formating, ignored
                    case 'm': state = 0; ++p; break; // no argument
                    case 'n':
                    case 'p':
                    case 'S':
                    case 's': state = 30; break; // pointers
                    case '$': ++p; break; // should issue a warning, it's not handled...
                    case '*':
                        if(pos<6)
                            *mystack = cpu->regs[regs_abi[pos++]];
                        else {
                            *mystack = *st;
                            ++st;
                        }
                        ++mystack;
                        ++p;
                        break; // fetch an int in the stack....
                    case ' ': state=0; ++p; break;
                    default:
                        state=20; // other stuff, put an int...
                }
                break;
            case 11:    //double
            case 12:    //%lg, still double
            case 13:    //%llg, still double
            case 15:    //%zg, meh .. double
                if(xmm) {
                lsassertm(0 , "wait for check. todo");
                    *mystack = cpu->xmm_regs->ZMM_D(x++);
                    --xmm;
                    mystack++;
                } else {
                    *mystack = *st;
                    st++; mystack++;
                }
                state = 0;
                ++p;
                break;
            case 14:    //%LG long double
                if((((uintptr_t)st)&0xf)!=0)
                    st++;
                #ifdef HAVE_LD80BITS
                if((((uintptr_t)mystack)&0xf)!=0)
                    mystack++;
                memcpy(mystack, st, 16);
                st+=2; mystack+=2;
                #else
                // there is 128bits long double on ARM64, but they need 128bit alignment
                if((((uintptr_t)mystack)&0xf)!=0)
                    mystack++;
                LD2D((void*)st, &d);
                ld = d ;
                memcpy(mystack, &ld, 16);
                st+=2; mystack+=2;
                #endif
                state = 0;
                ++p;
                break;
            case 20:    // fallback
            case 21:
            case 22:
            case 23:    // 64bits int
            case 24:    // normal int / pointer
            case 25:    // size_t int
            case 30:
                if(pos<6)
                    *mystack = cpu->regs[regs_abi[pos++]];
                else {
                    *mystack = *st;
                    ++st;
                }
                ++mystack;
                state = 0;
                ++p;
                break;
            default:
                // whaaaattt?
                state = 0;
        }
    }
}
void myStackAlignScanf(const char* fmt, uint64_t* st, uint64_t* mystack, int pos)
{
    if(!fmt)
        return;
    // loop...
    const char* p = fmt;
    int state = 0;
    int ign = 0;
    __MY_CPU;
    while(*p)
    {
        switch(state) {
            case 0:
                ign = 0;
                switch(*p) {
                    case '%': state = 1; ++p; break;
                    default:
                        ++p;
                }
                break;
            case 1: // normal
            case 2: // l
            case 3: // ll
            case 4: // L
            case 5: // z
                switch(*p) {
                    case '%': state = 0;  ++p; break; //%% = back to 0
                    case 'l': ++state; if (state>3) state=3; ++p; break;
                    case 'L': state = 4; ++p; break;
                    case 'z': state = 5; ++p; break;
                    case 'a':
                    case 'A':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                    case 'F':
                    case 'f': state += 10; break;    //  float
                    case 'd':
                    case 'i':
                    case 'o':
                    case 'u':
                    case 'x':
                    case 'X': state += 20; break;   // int
                    case 'h': ++p; break;  // ignored...
                    case '\'':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '.':
                    case '#':
                    case '+':
                    case '-': ++p; break; // formating, ignored
                    case 'm': state = 0; ++p; break; // no argument
                    case 'n':
                    case 'p':
                    case 'S':
                    case 's': state = 30; break; // pointers
                    case '$': ++p; break; // should issue a warning, it's not handled...
                    case '*': ign=1; ++p; break; // ignore arg
                    case ' ': state=0; ++p; break;
                    default:
                        state=20; // other stuff, put an int...
                }
                break;
            case 11:    //double
            case 12:    //%lg, still double
            case 13:    //%llg, still double
            case 14:    //%Lg long double
            case 15:    //%zg
            case 20:    // fallback
            case 21:
            case 22:
            case 23:    // 64bits int
            case 24:    // normal int / pointer
            case 25:    // size_t int
            case 30:
                if(!ign) {
                    if(pos<6)
                        *mystack = cpu->regs[regs_abi[pos++]];
                    else {
                        *mystack = *st;
                        ++st;
                    }
                    ++mystack;
                }
                state = 0;
                ++p;
                break;
            default:
                // whaaaat?
                state = 0;
        }
    }
}
void myStackAlignScanfW(const char* fmt, uint64_t* st, uint64_t* mystack, int pos)
{
    if(!fmt)
        return;
    // loop...
    const wchar_t* p = (const wchar_t*)fmt;
    int state = 0;
    int ign = 0;
    __MY_CPU;
    while(*p)
    {
        switch(state) {
            case 0:
                ign = 0;
                switch(*p) {
                    case '%': state = 1; ++p; break;
                    default:
                        ++p;
                }
                break;
            case 1: // normal
            case 2: // l
            case 3: // ll
            case 4: // L
            case 5: // z
                switch(*p) {
                    case '%': state = 0;  ++p; break; //%% = back to 0
                    case 'l': ++state; if (state>3) state=3; ++p; break;
                    case 'L': state = 4; ++p; break;
                    case 'z': state = 5; ++p; break;
                    case 'a':
                    case 'A':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                    case 'F':
                    case 'f': state += 10; break;    //  float
                    case 'd':
                    case 'i':
                    case 'o':
                    case 'u':
                    case 'x':
                    case 'X': state += 20; break;   // int
                    case 'h': ++p; break;  // ignored...
                    case '\'':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '.':
                    case '#':
                    case '+':
                    case '-': ++p; break; // formating, ignored
                    case 'm': state = 0; ++p; break; // no argument
                    case 'n':
                    case 'p':
                    case 'S':
                    case 's': state = 30; break; // pointers
                    case '$': ++p; break; // should issue a warning, it's not handled...
                    case '*': ign = 1; ++p; break; // ignore arg
                    case ' ': state=0; ++p; break;
                    default:
                        state=20; // other stuff, put an int...
                }
                break;
            case 11:    //double
            case 12:    //%lg, still double
            case 13:    //%llg, still double
            case 14:    //%Lg long double
            case 15:    //%zg
            case 20:    // fallback
            case 21:
            case 22:
            case 23:    // 64bits int
            case 24:    // normal int / pointer
            case 25:    // size_t int
            case 30:
                if(!ign) {
                    if(pos<6)
                        *mystack = cpu->regs[regs_abi[pos++]];
                    else {
                        *mystack = *st;
                        ++st;
                    }
                    ++mystack;
                }
                state = 0;
                ++p;
                break;
            default:
                // whaaaat?
                state = 0;
        }
    }
}
#ifndef CONVERT_VALIST
void myStackAlignValist(const char* fmt, uint64_t* mystack, x64_va_list_t va)
{
    if(!fmt)
        return;
    // loop...
    const char* p = fmt;
    int state = 0;
    #ifndef HAVE_LD80BITS
    double d;
    long double ld;
    #endif
    //int x = 0;
    uintptr_t *area = (uintptr_t*)va->reg_save_area;    // the direct registers copy
    uintptr_t *st = (uintptr_t*)va->overflow_arg_area;  // the stack arguments
    uintptr_t gprs = va->gp_offset;
    uintptr_t fprs = va->fp_offset;
    while(*p)
    {
        switch(state) {
            case 0:
                switch(*p) {
                    case '%': state = 1; ++p; break;
                    default:
                        ++p;
                }
                break;
            case 1: // normal
            case 2: // l
            case 3: // ll
            case 4: // L
            case 5: // z
                switch(*p) {
                    case '%': state = 0;  ++p; break; //%% = back to 0
                    case 'l': ++state; if (state>3) state=3; ++p; break;
                    case 'L': state = 4; ++p; break;
                    case 'z': state = 5; ++p; break;
                    case 'a':
                    case 'A':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                    case 'F':
                    case 'f': state += 10; break;    //  float
                    case 'd':
                    case 'i':
                    case 'o':
                    case 'u':
                    case 'x':
                    case 'X': state += 20; break;   // int
                    case 'h': ++p; break;  // ignored...
                    case '\'':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '.':
                    case '+':
                    case '-': ++p; break; // formating, ignored
                    case 'm': state = 0; ++p; break; // no argument
                    case 'n':
                    case 'p':
                    case 'S':
                    case 's': state = 30; break; // pointers
                    case '$': ++p; break; // should issue a warning, it's not handled...
                    case '*':
                        if(gprs<X64_VA_MAX_REG) {
                            *mystack = area[gprs/8];
                            gprs+=8;
                        } else {
                            *mystack = *st;
                            ++st;
                        }
                        ++mystack;
                        ++p;
                        break; // fetch an int in the stack....
                    case ' ': state=0; ++p; break;
                    default:
                        state=20; // other stuff, put an int...
                }
                break;
            case 11:    //double
            case 12:    //%lg, still double
            case 13:    //%llg, still double
            case 15:    //%zg, meh.. double?
                if(fprs<X64_VA_MAX_XMM) {
                    *mystack = area[fprs/8];
                    fprs+=16;
                    mystack++;
                } else {
                    *mystack = *st;
                    st++; mystack++;
                }
                state = 0;
                ++p;
                break;
            case 14:    //%LG long double
                if((((uintptr_t)st)&0xf)!=0)
                    st++;
                #ifdef HAVE_LD80BITS
                if((((uintptr_t)mystack)&0xf)!=0)
                    mystack++;
                memcpy(mystack, st, 16);
                st+=2; mystack+=2;
                #else
                // there is 128bits long double on ARM64, but they need 128bit alignment
                if((((uintptr_t)mystack)&0xf)!=0)
                    mystack++;
                LD2D((void*)st, &d);
                ld = d ;
                memcpy(mystack, &ld, 16);
                st+=2; mystack+=2;
                #endif
                state = 0;
                ++p;
                break;
            case 20:    // fallback
            case 21:
            case 22:
            case 23:    // 64bits int
            case 24:    // normal int / pointer
            case 25:    // size_t int
            case 30:
                if(gprs<X64_VA_MAX_REG) {
                    *mystack = area[gprs/8];
                    gprs+=8;
                } else {
                    *mystack = *st;
                    ++st;
                }
                ++mystack;
                state = 0;
                ++p;
                break;
            default:
                // whaaaat?
                state = 0;
        }
    }
}

void myStackAlignWValist(const char* fmt, uint64_t* mystack, x64_va_list_t va)
{
    // loop...
    const wchar_t* p = (const wchar_t*)fmt;
    int state = 0;
    #ifndef HAVE_LD80BITS
    double d;
    long double ld;
    #endif
//    int x = 0;
    uintptr_t *area = (uintptr_t*)va->reg_save_area;    // the direct registers copy
    uintptr_t *st = (uintptr_t*)va->overflow_arg_area;  // the stack arguments
    uintptr_t gprs = va->gp_offset;
    uintptr_t fprs = va->fp_offset;
    while(*p)
    {
        switch(state) {
            case 0:
                switch(*p) {
                    case '%': state = 1; ++p; break;
                    default:
                        ++p;
                }
                break;
            case 1: // normal
            case 2: // l
            case 3: // ll
            case 4: // L
            case 5: // z
                switch(*p) {
                    case '%': state = 0;  ++p; break; //%% = back to 0
                    case 'l': ++state; if (state>3) state=3; ++p; break;
                    case 'L': state = 4; ++p; break;
                    case 'z': state = 5; ++p; break;
                    case 'a':
                    case 'A':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                    case 'F':
                    case 'f': state += 10; break;    //  float
                    case 'd':
                    case 'i':
                    case 'o':
                    case 'u':
                    case 'x':
                    case 'X': state += 20; break;   // int
                    case 'h': ++p; break;  // ignored...
                    case '\'':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '.':
                    case '+':
                    case '-': ++p; break; // formating, ignored
                    case 'm': state = 0; ++p; break; // no argument
                    case 'n':
                    case 'p':
                    case 'S':
                    case 's': state = 30; break; // pointers
                    case '$': ++p; break; // should issue a warning, it's not handled...
                    case '*':
                        if(gprs<X64_VA_MAX_REG) {
                            *mystack = area[gprs/8];
                            gprs+=8;
                        } else {
                            *mystack = *st;
                            ++st;
                        }
                        ++mystack;
                        ++p;
                        break; // fetch an int in the stack....
                    case ' ': state=0; ++p; break;
                    default:
                        state=20; // other stuff, put an int...
                }
                break;
            case 11:    //double
            case 12:    //%lg, still double
            case 13:    //%llg, still double
            case 15:    //%zg, meh .. double
                if(fprs<X64_VA_MAX_XMM) {
                    *mystack = area[fprs/8];
                    fprs+=16;
                    mystack++;
                } else {
                    *mystack = *st;
                    st++; mystack++;
                }
                state = 0;
                ++p;
                break;
            case 14:    //%LG long double
                if((((uintptr_t)st)&0xf)!=0)
                    st++;
                #ifdef HAVE_LD80BITS
                if((((uintptr_t)mystack)&0xf)!=0)
                    mystack++;
                memcpy(mystack, st, 16);
                st+=2; mystack+=2;
                #else
                // there is 128bits long double on ARM64, but they need 128bit alignment
                if((((uintptr_t)mystack)&0xf)!=0)
                    mystack++;
                LD2D((void*)st, &d);
                ld = d ;
                memcpy(mystack, &ld, 16);
                st+=2; mystack+=2;
                #endif
                state = 0;
                ++p;
                break;
            case 20:    // fallback
            case 21:
            case 22:
            case 23:    // 64bits int
            case 24:    // normal int / pointer
            case 25:    // size_t int
            case 30:
                if(gprs<X64_VA_MAX_REG) {
                    *mystack = area[gprs/8];
                    gprs+=8;
                } else {
                    *mystack = *st;
                    ++st;
                }
                ++mystack;
                state = 0;
                ++p;
                break;
            default:
                // whaaaattt?
                state = 0;
        }
    }
}

void myStackAlignScanfValist(const char* fmt, uint64_t* mystack, x64_va_list_t va)
{
    if(!fmt)
        return;
    // loop...
    const char* p = fmt;
    int state = 0;
    int ign = 0;
    uintptr_t *area = (uintptr_t*)va->reg_save_area;    // the direct registers copy
    uintptr_t *st = (uintptr_t*)va->overflow_arg_area;  // the stack arguments
    uintptr_t gprs = va->gp_offset;
//    uintptr_t fprs = va->fp_offset;
    while(*p)
    {
        switch(state) {
            case 0:
                ign = 0;
                switch(*p) {
                    case '%': state = 1; ++p; break;
                    default:
                        ++p;
                }
                break;
            case 1: // normal
            case 2: // l
            case 3: // ll
            case 4: // L
            case 5: // z
                switch(*p) {
                    case '%': state = 0;  ++p; break; //%% = back to 0
                    case 'l': ++state; if (state>3) state=3; ++p; break;
                    case 'L': state = 4; ++p; break;
                    case 'z': state = 5; ++p; break;
                    case 'a':
                    case 'A':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                    case 'F':
                    case 'f': state += 10; break;    //  float
                    case 'd':
                    case 'i':
                    case 'o':
                    case 'u':
                    case 'x':
                    case 'X': state += 20; break;   // int
                    case 'h': ++p; break;  // ignored...
                    case '\'':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '.':
                    case '+':
                    case '-': ++p; break; // formating, ignored
                    case 'm': state = 0; ++p; break; // no argument
                    case 'n':
                    case 'p':
                    case 'S':
                    case 's': state = 30; break; // pointers
                    case '$': ++p; break; // should issue a warning, it's not handled...
                    case '*': ign=1; ++p; break; // ignore arg
                    case ' ': state=0; ++p; break;
                    default:
                        state=20; // other stuff, put an int...
                }
                break;
            case 11:    //double
            case 12:    //%lg, still double
            case 13:    //%llg, still double
            case 14:    //%Lg long double
            case 15:    //%zg
            case 20:    // fallback
            case 21:
            case 22:
            case 23:    // 64bits int
            case 24:    // normal int / pointer
            case 25:    // size_t int
            case 30:
                if(!ign) {
                    if(gprs<X64_VA_MAX_REG) {
                        *mystack = area[gprs/8];
                        gprs+=8;
                    } else {
                        *mystack = *st;
                        ++st;
                    }
                    ++mystack;
                }
                state = 0;
                ++p;
                break;
            default:
                // whaaaat?
                state = 0;
        }
    }
}

void myStackAlignScanfWValist(const char* fmt, uint64_t* mystack, x64_va_list_t va)
{
    if(!fmt)
        return;
    // loop...
    const wchar_t* p = (const wchar_t*)fmt;
    int state = 0;
    int ign = 0;
    uintptr_t *area = (uintptr_t*)va->reg_save_area;    // the direct registers copy
    uintptr_t *st = (uintptr_t*)va->overflow_arg_area;  // the stack arguments
    uintptr_t gprs = va->gp_offset;
//    uintptr_t fprs = va->fp_offset;
    while(*p)
    {
        switch(state) {
            case 0:
                ign = 0;
                switch(*p) {
                    case '%': state = 1; ++p; break;
                    default:
                        ++p;
                }
                break;
            case 1: // normal
            case 2: // l
            case 3: // ll
            case 4: // L
            case 5: // z
                switch(*p) {
                    case '%': state = 0;  ++p; break; //%% = back to 0
                    case 'l': ++state; if (state>3) state=3; ++p; break;
                    case 'L': state = 4; ++p; break;
                    case 'z': state = 5; ++p; break;
                    case 'a':
                    case 'A':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                    case 'F':
                    case 'f': state += 10; break;    //  float
                    case 'd':
                    case 'i':
                    case 'o':
                    case 'u':
                    case 'x':
                    case 'X': state += 20; break;   // int
                    case 'h': ++p; break;  // ignored...
                    case '\'':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '.':
                    case '+':
                    case '-': ++p; break; // formating, ignored
                    case 'm': state = 0; ++p; break; // no argument
                    case 'n':
                    case 'p':
                    case 'S':
                    case 's': state = 30; break; // pointers
                    case '$': ++p; break; // should issue a warning, it's not handled...
                    case '*': ign = 1; ++p; break; // ignore arg
                    case ' ': state=0; ++p; break;
                    default:
                        state=20; // other stuff, put an int...
                }
                break;
            case 11:    //double
            case 12:    //%lg, still double
            case 13:    //%llg, still double
            case 14:    //%Lg long double
            case 15:    //%zg
            case 20:    // fallback
            case 21:
            case 22:
            case 23:    // 64bits int
            case 24:    // normal int / pointer
            case 25:    // size_t int
            case 30:
                if(!ign) {
                    if(gprs<X64_VA_MAX_REG) {
                        *mystack = area[gprs/8];
                        gprs+=8;
                    } else {
                        *mystack = *st;
                        ++st;
                    }
                    ++mystack;
                }
                state = 0;
                ++p;
                break;
            default:
                // whaaaat?
                state = 0;
        }
    }
}

#endif
void UnalignStat64(const void* source, void* dest)
{
    struct x64_stat64 *x64st = (struct x64_stat64*)dest;
    struct stat *st = (struct stat*) source;

    x64st->__pad0 = 0;
	memset(x64st->__glibc_reserved, 0, sizeof(x64st->__glibc_reserved));
    x64st->st_dev      = st->st_dev;
    x64st->st_ino      = st->st_ino;
    x64st->st_mode     = st->st_mode;
    x64st->st_nlink    = st->st_nlink;
    x64st->st_uid      = st->st_uid;
    x64st->st_gid      = st->st_gid;
    x64st->st_rdev     = st->st_rdev;
    x64st->st_size     = st->st_size;
    x64st->st_blksize  = st->st_blksize;
    x64st->st_blocks   = st->st_blocks;
    x64st->st_atim     = st->st_atim;
    x64st->st_mtim     = st->st_mtim;
    x64st->st_ctim     = st->st_ctim;
}

void AlignStat64(const void* source, void* dest)
{
    struct stat *st = (struct stat*) dest;
    struct x64_stat64 *x64st = (struct x64_stat64*)source;

    st->st_dev      = x64st->st_dev;
    st->st_ino      = x64st->st_ino;
    st->st_mode     = x64st->st_mode;
    st->st_nlink    = x64st->st_nlink;
    st->st_uid      = x64st->st_uid;
    st->st_gid      = x64st->st_gid;
    st->st_rdev     = x64st->st_rdev;
    st->st_size     = x64st->st_size;
    st->st_blksize  = x64st->st_blksize;
    st->st_blocks   = x64st->st_blocks;
    st->st_atim     = x64st->st_atim;
    st->st_mtim     = x64st->st_mtim;
    st->st_ctim     = x64st->st_ctim;
}
double FromLD(void* ld)
{
    if(box64_x87_no80bits)
        return *(double*)ld;
    double ret; // cannot add = 0; it break factorio (issue when calling fmodl)
    LD2D(ld, &ret);
    return ret;
}
static int nCPU = 0;
static double bogoMips = 100.;

void grabNCpu(void) {
    nCPU = 1;  // default number of CPU to 1
    FILE *f = fopen("/proc/cpuinfo", "r");
    size_t dummy;
    if(f) {
        nCPU = 0;
        int bogo = 0;
        size_t len = 500;
        char* line = malloc(len);
        while ((dummy = getline(&line, &len, f)) != -1) {
            if(!strncmp(line, "processor\t", strlen("processor\t")))
                ++nCPU;
            if(!bogo && !strncmp(line, "BogoMIPS\t", strlen("BogoMIPS\t"))) {
                // grab 1st BogoMIPS
                float tmp;
                if(sscanf(line, "BogoMIPS\t: %g", &tmp)==1) {
                    bogoMips = tmp;
                    bogo = 1;
                }
            }
        }
        free(line);
        fclose(f);
        if(!nCPU) nCPU=1;
    }
}
int getNCpu(void)
{
    if(!nCPU)
        grabNCpu();
    return nCPU;
}
double getBogoMips(void)
{
    if(!nCPU)
        grabNCpu();
    return bogoMips;
}

const char* getCpuName(void)
{
    static char name[300] = "Unknown CPU";
    static int done = 0;
    if(done)
        return name;
    done = 1;
    FILE* f = popen("lscpu | grep \"Model name:\" | sed -r 's/Model name:\\s{1,}//g'", "r");
    if(f) {
        char tmp[200] = "";
        ssize_t s = fread(tmp, 1, 200, f);
        pclose(f);
        if(s>0) {
            // worked! (unless it's saying "lscpu: command not found" or something like that)
            if(!strstr(tmp, "lscpu")) {
                // trim ending
                while(strlen(tmp) && tmp[strlen(tmp)-1]=='\n')
                    tmp[strlen(tmp)-1] = 0;
                // incase multiple cpu type are present, there will be multiple lines
                while(strchr(tmp, '\n'))
                    *strchr(tmp,'\n') = ' ';
                strncpy(name, tmp, 199);
                name[199] = '\0';
            }
            return name;
        }
    }
    // failled, try to get architecture at least
    f = popen("lscpu | grep \"Architecture:\" | sed -r 's/Architecture:\\s{1,}//g'", "r");
    if(f) {
        char tmp[200] = "";
        ssize_t s = fread(tmp, 1, 200, f);
        pclose(f);
        if(s>0) {
            // worked!
            // trim ending
            while(strlen(tmp) && tmp[strlen(tmp)-1]=='\n')
                tmp[strlen(tmp)-1] = 0;
            // incase multiple cpu type are present, there will be multiple lines
            while(strchr(tmp, '\n'))
                *strchr(tmp,'\n') = ' ';
            snprintf(name, 299, "unknown %s cpu", tmp);
            return name;
        }
    }
    // Nope, bye
    return name;
}

const char* getBoxCpuName(void)
{
    static char branding[3*4*4+1] = "";
    static int done = 0;
    if(!done) {
        done = 1;
        const char* name = getCpuName();
        if(strstr(name, "MHz") || strstr(name, "GHz")) {
            // name already have the speed in it
            snprintf(branding, sizeof(branding), "Box64 on %.*s", 39, name);
        } else {
            unsigned int MHz = get_cpuMhz();
            if(MHz>1500) { // swiches to GHz display...
                snprintf(branding, sizeof(branding), "Box64 on %.*s @%1.2f GHz", 28, name, MHz/1000.);
            } else {
                snprintf(branding, sizeof(branding), "Box64 on %.*s @%04d MHz", 28, name, MHz);
            }
        }
    }
    return branding;
}
int get_cpuMhz(void)
{
	int MHz = 0;
	FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
	if(f) {
		int r;
		if(1==fscanf(f, "%d", &r))
			MHz = r/1000;
		fclose(f);
	}
    if(!MHz) {
        // try with lscpu, grabbing the max frequency
        FILE* f = popen("lscpu | grep \"CPU max MHz:\" | sed -r 's/CPU max MHz:\\s{1,}//g'", "r");
        if(f) {
            char tmp[200] = "";
            ssize_t s = fread(tmp, 1, 200, f);
            pclose(f);
            if(s>0) {
                // worked! (unless it's saying "lscpu: command not found" or something like that)
                if(!strstr(tmp, "lscpu")) {
                    // trim ending
                    while(strlen(tmp) && tmp[strlen(tmp)-1]=='\n')
                        tmp[strlen(tmp)-1] = 0;
                    // incase multiple cpu type are present, there will be multiple lines
                    while(strchr(tmp, '\n'))
                        *strchr(tmp,'\n') = ' ';
                    // cut the float part (so '.' or ','), it's not needed
                    if(strchr(tmp, '.'))
                        *strchr(tmp, '.')= '\0';
                    if(strchr(tmp, ','))
                        *strchr(tmp, ',')= '\0';
                    int mhz;
                    if(sscanf(tmp, "%d", &mhz)==1)
                        MHz = mhz;
                }
            }
        }
    }
	if(!MHz)
		MHz = 1000; // default to 1Ghz...
	return MHz;
}
void CreateMemorymapFile(box64context_t* context, int fd)
{
    // this will tranform current memory map
    // by anotating anonymous entry that belong to emulated elf
    // also anonymising current stack
    // and setting emulated stack as the current one

    char* line = NULL;
    size_t len = 0;
    char buff[1024];
    int dummy;
    FILE* f = fopen("/proc/self/maps", "r");
    if(!f)
        return;
    while(getline(&line, &len, f)>0) {
        // line is like
        // aaaadd750000-aaaadd759000 r-xp 00000000 103:02 13386730                  /usr/bin/cat
        uintptr_t start, end;
        if (sscanf(line, "%zx-%zx", &start, &end)==2) {
            elfheader_t* h = FindElfAddress(context, start);
            int l = strlen(line);
            if(h && l<73) {
                sprintf(buff, "%s%*s\n", line, 74-l, h->name);
                dummy = write(fd, buff, strlen(buff));
            } else if(start==(uintptr_t)context->stack) {
                sprintf(buff, "%s%*s\n", line, 74-l, "[stack]");
                dummy = write(fd, buff, strlen(buff));
            } else if (strstr(line, "[stack]")) {
                char* p = strstr(line, "[stack]")-1;
                while (*p==' ' || *p=='\t') --p;
                p[1]='\0';
                strcat(line, "\n");
                dummy = write(fd, line, strlen(line));
            } else {
                dummy = write(fd, line, strlen(line));
            }
        }
    }
    fclose(f);
    (void)dummy;
}
struct __attribute__((packed)) x64_epoll_event {
    uint32_t            events;
    uint64_t            data;
};
// Arm -> x64
void UnalignEpollEvent(void* dest, void* source, int nbr)
{
    struct x64_epoll_event *x64_struct = (struct x64_epoll_event*)dest;
    struct epoll_event *arm_struct = (struct epoll_event*)source;
    while(nbr) {
        x64_struct->events = arm_struct->events;
        x64_struct->data = arm_struct->data.u64;
        ++x64_struct;
        ++arm_struct;
        --nbr;
    }
}

// x64 -> Arm
void AlignEpollEvent(void* dest, void* source, int nbr)
{
    struct x64_epoll_event *x64_struct = (struct x64_epoll_event*)source;
    struct epoll_event *arm_struct = (struct epoll_event*)dest;
    while(nbr) {
        arm_struct->events = x64_struct->events;
        arm_struct->data.u64 = x64_struct->data;
        ++x64_struct;
        ++arm_struct;
        --nbr;
    }
}
struct __attribute__((packed)) x64_semid_ds {
    struct ipc_perm sem_perm;
    time_t sem_otime;
    unsigned long _reserved1;
    time_t sem_ctime;
    unsigned long _reserved2;
    unsigned long sem_nsems;
    unsigned long _reserved3;
    unsigned long _reserved4;
};
void UnalignSemidDs(void *dest, const void* source)
{
    struct x64_semid_ds *x64_struct = (struct x64_semid_ds*)dest;
    const struct semid_ds *arm_struct = (const struct semid_ds*)source;

    x64_struct->sem_perm = arm_struct->sem_perm;
    x64_struct->sem_otime = arm_struct->sem_otime;
    x64_struct->sem_ctime = arm_struct->sem_ctime;
    x64_struct->sem_nsems = arm_struct->sem_nsems;
}

void AlignSemidDs(void *dest, const void* source)
{
    const struct x64_semid_ds *x64_struct = (const struct x64_semid_ds*)source;
    struct semid_ds *arm_struct = (struct semid_ds*)dest;

    arm_struct->sem_perm = x64_struct->sem_perm;
    arm_struct->sem_otime = x64_struct->sem_otime;
    arm_struct->sem_ctime = x64_struct->sem_ctime;
    arm_struct->sem_nsems = x64_struct->sem_nsems;
}
int GetTID(void)
{
    return syscall(SYS_gettid);
}
#define MUTEX_SIZE_X64 40
typedef struct my_xcb_ext_s {
    pthread_mutex_t lock;
    struct lazyreply *extensions;
    int extensions_size;
} my_xcb_ext_t;

typedef struct x64_xcb_ext_s {
    uint8_t lock[MUTEX_SIZE_X64];
    struct lazyreply *extensions;
    int extensions_size;
} x64_xcb_ext_t;

typedef struct my_xcb_xid_s {
    pthread_mutex_t lock;
    uint32_t last;
    uint32_t base;
    uint32_t max;
    uint32_t inc;
} my_xcb_xid_t;

typedef struct x64_xcb_xid_s {
    uint8_t lock[MUTEX_SIZE_X64];
    uint32_t last;
    uint32_t base;
    uint32_t max;
    uint32_t inc;
} x64_xcb_xid_t;

typedef struct my_xcb_fd_s {
    int fd[16];
    int nfd;
    int ifd;
} my_xcb_fd_t;

typedef struct my_xcb_in_s {
    pthread_cond_t event_cond;
    int reading;
    char queue[4096];
    int queue_len;
    uint64_t request_expected;
    uint64_t request_read;
    uint64_t request_completed;
    struct reply_list *current_reply;
    struct reply_list **current_reply_tail;
    void*  replies;
    struct event_list *events;
    struct event_list **events_tail;
    struct reader_list *readers;
    struct special_list *special_waiters;
    struct pending_reply *pending_replies;
    struct pending_reply **pending_replies_tail;
    my_xcb_fd_t in_fd;
    struct xcb_special_event *special_events;
} my_xcb_in_t;

typedef struct x64_xcb_out_s {
    pthread_cond_t cond;
    int writing;
    pthread_cond_t socket_cond;
    void (*return_socket)(void *closure);
    void *socket_closure;
    int socket_moving;
    char queue[16384];
    int queue_len;
    uint64_t request;
    uint64_t request_written;
    uint8_t reqlenlock[40];
    int maximum_request_length_tag;
    uint32_t maximum_request_length;
    my_xcb_fd_t out_fd;
} x64_xcb_out_t;

typedef struct my_xcb_out_s {
    pthread_cond_t cond;
    int writing;
    pthread_cond_t socket_cond;
    void (*return_socket)(void *closure);
    void *socket_closure;
    int socket_moving;
    char queue[16384];
    int queue_len;
    uint64_t request;
    uint64_t request_written;
    pthread_mutex_t reqlenlock;
    int maximum_request_length_tag;
    uint32_t maximum_request_length;
    my_xcb_fd_t out_fd;
} my_xcb_out_t;

typedef struct my_xcb_connection_s {
    int has_error;
    void *setup;
    int fd;
    pthread_mutex_t iolock;
    my_xcb_in_t in;
    my_xcb_out_t out;
    my_xcb_ext_t ext;
    my_xcb_xid_t xid;
} my_xcb_connection_t;

typedef struct x64_xcb_connection_s {
    int has_error;
    void *setup;
    int fd;
    uint8_t iolock[MUTEX_SIZE_X64];
    my_xcb_in_t in;
    x64_xcb_out_t out;
    x64_xcb_ext_t ext;
    x64_xcb_xid_t xid;
} x64_xcb_connection_t;

#define NXCB 8
my_xcb_connection_t* my_xcb_connects[NXCB] = {0};
x64_xcb_connection_t x64_xcb_connects[NXCB] = {0};
#ifdef CONFIG_LATX_DEBUG
/* check x64_xcb_connects had sync my_xcb_connects*/
/*
* Use align_xcb_connection and unalign_xcb_connection sync x64_xcb_connects.
* But, For latx, my_xcb_connects had changed by other FUNC, insead of xcb api.
*So we need latx_xcb_cmp for check xcb changed or not.
*/

static void latx_xcb_cmp(void* src, void* dst)
{
    my_xcb_connection_t * dest = dst;
    my_xcb_connection_t * source = src;
 #define GO(member,mtype) do{lsassertm(source->member == dest->member, "x64_xcb_connects->"#member"="#mtype" != my_xcb_connects->"#member"="#mtype"\n", source->member, dest->member );} while(0)
    GO(has_error,"%d");
    GO(setup, "%p");
    GO(fd, "%d");
    GO(ext.extensions, "%p");
    GO(ext.extensions_size, "%d");
    GO(xid.base, "%d");
    GO(xid.inc, "%d");
    GO(xid.last, "%d");
    GO(xid.max, "%d");
 #undef GO
}
#endif

EXPORT int32_t my_xcb_flush(void* v1);
void* align_xcb_connection(void* src)
{
    if(!src)
        return src;
    // find it
    my_xcb_connection_t * dest = NULL;
    for(int i=0; i<NXCB && !dest; ++i)
        if(src==&x64_xcb_connects[i])
            dest = my_xcb_connects[i];
    #if 1
    if (!(src&&dest&&((my_xcb_connection_t *)src)->xid.last == ((my_xcb_connection_t *)dest)->xid.last)) {
        my_xcb_flush(dest);
    }
#ifdef CONFIG_LATX_DEBUG
    if (dest) {
        latx_xcb_cmp(src,dest);
    }
#endif
    if(!dest)
        dest = add_xcb_connection(src);
    #else
    if(!dest) {
        printf_log(LOG_NONE, "BOX64: Error, xcb_connect %p not found\n", src);
        abort();
    }
    #endif
    #if 1
    // do not update most values
    x64_xcb_connection_t* source = src;
    dest->has_error = source->has_error;
    dest->setup = source->setup;
    dest->fd = source->fd;
    //memcpy(&dest->iolock, source->iolock, MUTEX_SIZE_X64);
    //dest->in = source->in;
    //dest->out = source->out;
    //memcpy(&dest->ext.lock, source->ext.lock, MUTEX_SIZE_X64);
    dest->ext.extensions = source->ext.extensions;
    dest->ext.extensions_size = source->ext.extensions_size;
    //memcpy(&dest->xid.lock, source->xid.lock, MUTEX_SIZE_X64);
    dest->xid.base = source->xid.base;
    dest->xid.inc = source->xid.inc;
    if (dest->xid.last > source->xid.last) {
        source->xid.last = dest->xid.last;
        //waring: dest->xid.last > source->xid.last skip.
    } else {
        dest->xid.last = source->xid.last;
    }
    dest->xid.max = source->xid.max;
    #endif
    return dest;
}

void unalign_xcb_connection(void* src, void* dst)
{
    if(!src || !dst || src==dst)
        return;
    // update values
    my_xcb_connection_t* source = src;
    x64_xcb_connection_t* dest = dst;
    dest->has_error = source->has_error;
    dest->setup = source->setup;
    dest->fd = source->fd;
    memcpy(dest->iolock, &source->iolock, MUTEX_SIZE_X64);
    dest->in = source->in;
    memcpy(dest->out.reqlenlock, &source->out.reqlenlock, MUTEX_SIZE_X64);
    dest->out.cond = source->out.cond;
    dest->out.maximum_request_length = source->out.maximum_request_length;
    dest->out.maximum_request_length_tag = source->out.maximum_request_length_tag;
    dest->out.out_fd = source->out.out_fd;
    memcpy(dest->out.queue, source->out.queue, sizeof(dest->out.queue));
    dest->out.queue_len = source->out.queue_len;
    dest->out.request = source->out.request;
    dest->out.request_written = source->out.request_written;
    dest->out.return_socket = source->out.return_socket;
    dest->out.socket_closure = source->out.socket_closure;
    dest->out.socket_cond = source->out.socket_cond;
    dest->out.socket_moving = source->out.socket_moving;
    dest->out.writing = source->out.writing;
    memcpy(dest->ext.lock, &source->ext.lock, MUTEX_SIZE_X64);
    dest->ext.extensions = source->ext.extensions;
    dest->ext.extensions_size = source->ext.extensions_size;
    memcpy(dest->xid.lock, &source->xid.lock, MUTEX_SIZE_X64);
    dest->xid.base = source->xid.base;
    dest->xid.inc = source->xid.inc;
    dest->xid.last = source->xid.last;
    dest->xid.max = source->xid.max;
}

void* add_xcb_connection(void* src)
{
    if(!src)
        return src;
    // check if already exist
    for(int i=0; i<NXCB; ++i)
        if(my_xcb_connects[i] == src) {
            unalign_xcb_connection(src, &x64_xcb_connects[i]);
            return &x64_xcb_connects[i];
        }
    // find a free slot
    for(int i=0; i<NXCB; ++i)
        if(!my_xcb_connects[i]) {
            my_xcb_connects[i] = src;
            unalign_xcb_connection(src, &x64_xcb_connects[i]);
            return &x64_xcb_connects[i];
        }
    printf_log(LOG_NONE, "BOX64: Error, no more free xcb_connect slot for %p\n", src);
    return src;
}

void del_xcb_connection(void* src)
{
    if(!src)
        return;
    // find it
    for(int i=0; i<NXCB; ++i)
        if(src==&x64_xcb_connects[i]) {
            my_xcb_connects[i] = NULL;
            memset(&x64_xcb_connects[i], 0, sizeof(x64_xcb_connection_t));
            return;
        }
    printf_log(LOG_NONE, "BOX64: Error, xcb_connect %p not found for deletion\n", src);
}
int sync_xcb_connection(void* src)
{
    for(int i=0; i<NXCB; ++i) {
        if(my_xcb_connects[i] == src) {
            unalign_xcb_connection(src, &x64_xcb_connects[i]);
            return 0;
        }
    }
    return -1;
}
static void LoadEnvPath(path_collection_t *col, const char* defpath, const char* env)
{
    const char* p = getenv(env);
    if(p) {
        printf_log(LOG_INFO, "%s: ", env);
        ParseList(p, col, 1);
    } else {
        printf_log(LOG_INFO, "Using default %s: ", env);
        ParseList(defpath, col, 1);
    }
    if(LOG_INFO<=relocation_log) {
        for(int i=0; i<col->size; i++)
            printf_log(LOG_INFO, "%s%s", col->paths[i], (i==col->size-1)?"\n":":");
    }
}

static void LoadEnvVars(box64context_t *context)
{
    // check BOX64_LD_LIBRARY_PATH and load it
    LoadEnvPath(&context->box64_ld_lib, ".:lib:lib64:x86_64:bin64:libs64", "BOX64_LD_LIBRARY_PATH");
    char tmp[PATH_MAX] = {0};
#ifdef CONFIG_LOONGARCH_NEW_WORLD
    snprintf(tmp, PATH_MAX, "%s%s:%s%s:%s%s:%s%s:%s%s", interp_prefix, "/lib64", interp_prefix,
            "/lib/x86_64-linux-gnu", interp_prefix, "/usr/lib", interp_prefix,
            "/usr/lib/x86_64-linux-gnu",  interp_prefix, "/usr/x86_64-linux-gnu/lib");
    AppendList(&context->box64_ld_lib, tmp, 1);
    AppendList(&context->box64_ld_lib, "/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu", 1);
#else
    snprintf(tmp, PATH_MAX, "%s%s:%s%s:%s%s:%s%s" , interp_prefix, "/lib64", interp_prefix, "/lib/x86_64-linux-gnu",
            interp_prefix, "/usr/lib/x86_64-linux-gnu",  interp_prefix, "/usr/x86_64-linux-gnu/lib");
    AppendList(&context->box64_ld_lib, tmp, 1);
    AppendList(&context->box64_ld_lib, "/lib64:/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:/usr/x86_64-linux-gnu/lib", 1);
#endif

    if(getenv("LD_LIBRARY_PATH"))
        PrependList(&context->box64_ld_lib, getenv("LD_LIBRARY_PATH"), 1);   // in case some of the path are for x86 world

    // check BOX64_PATH and load it
    LoadEnvPath(&context->box64_path, ".:bin", "BOX64_PATH");
    if(getenv("PATH"))
        AppendList(&context->box64_path, getenv("PATH"), 1);   // in case some of the path are for x86 world
}

static void free_contextargv(void)
{
    for(int i=0; i<my_context->argc; ++i)
        box_free(my_context->argv[i]);
}

#ifndef PN_XNUM
#define PN_XNUM (0xffff)
#endif
static elfheader_t* LoadFromNative(struct linux_binprm *bprm, struct image_info *info)
{
   Elf64_Ehdr *header = (Elf64_Ehdr *)bprm->exec_hdr;

    elfheader_t *h =Init_Elfheader();
    h->name =box_strdup(bprm->exec_filename);
    h->path =box_strdup(realpath(bprm->exec_filename, NULL));
    h->entrypoint = header->e_entry;
    h->numPHEntries = header->e_phnum;
    h->numSHEntries = header->e_shnum;
    h->SHIdx = header->e_shstrndx;
    h->e_type = header->e_type;
    if(info->load_addr > h->entrypoint)
        h->delta = info->load_addr;

    if(header->e_shentsize && header->e_shnum) {
        // special cases for nums
        if(h->numSHEntries == 0) {
            printf_log(LOG_INFO, "Read number of Sections in 1st Section\n");
            // read 1st section header and grab actual number from here
            Elf64_Shdr section;
            if(pread(bprm->exec_fd, &section, sizeof(Elf64_Shdr), header->e_shoff) ==-1) {
                box_free(h);
                printf_log(LOG_INFO, "Cannot read Initial Section Header\n");
                return NULL;
            }
            h->numSHEntries = section.sh_size;
        }
        // now read all section headers
        printf_log(LOG_INFO, "Read %zu Section header\n", h->numSHEntries);
        h->SHEntries = (Elf64_Shdr*)box_calloc(h->numSHEntries, sizeof(Elf64_Shdr));
        if(pread(bprm->exec_fd, h->SHEntries, sizeof(Elf64_Shdr)*h->numSHEntries, header->e_shoff) == -1) {
                FreeElfHeader(&h);
                printf_log(LOG_INFO, "Cannot read all Section Header\n");
                return NULL;
        }

        if(h->numPHEntries == PN_XNUM) {
            printf_log(LOG_INFO, "Read number of Program Header in 1st Section\n");
            // read 1st section header and grab actual number from here
            h->numPHEntries = h->SHEntries[0].sh_info;
        }
    }
    printf_log(LOG_INFO, "Read %zu Program header\n", h->numPHEntries);
    h->PHEntries = (Elf64_Phdr*)box_calloc(h->numPHEntries, sizeof(Elf64_Phdr));
    if(pread(bprm->exec_fd, h->PHEntries, sizeof(Elf64_Phdr)*h->numPHEntries, header->e_phoff) == -1) {
            FreeElfHeader(&h);
            printf_log(LOG_INFO, "Cannot read all Program Header\n");
            return NULL;
    }
    if(header->e_shentsize && header->e_shnum) {
        if(h->SHIdx == SHN_XINDEX) {
            printf_log(LOG_INFO, "Read number of String Table in 1st Section\n");
            h->SHIdx = h->SHEntries[0].sh_link;
        }
        if(h->SHIdx > h->numSHEntries) {
            printf_log(LOG_INFO, "Incoherent Section String Table Index : %zu / %zu\n", h->SHIdx, h->numSHEntries);
            FreeElfHeader(&h);
            return NULL;
        }
        // load Section table
        printf_log(LOG_INFO, "Loading Sections Table String (idx = %zu)\n", h->SHIdx);
        if(LoadSHNative(bprm->exec_fd, h->SHEntries+h->SHIdx, (void*)&h->SHStrTab, ".shstrtab", SHT_STRTAB) == -1) {
            FreeElfHeader(&h);
            return NULL;
        }
        if(relocation_dump) DumpMainHeader(header, h);

        LoadNamedSectionNative(bprm->exec_fd, h->SHEntries, h->numSHEntries, h->SHStrTab, ".strtab", "SymTab Strings", SHT_STRTAB, (void**)&h->StrTab, NULL);
        LoadNamedSectionNative(bprm->exec_fd, h->SHEntries, h->numSHEntries, h->SHStrTab, ".symtab", "SymTab", SHT_SYMTAB, (void**)&h->SymTab, &h->numSymTab);
        if(relocation_dump && h->SymTab) DumpSymTab(h);

        LoadNamedSectionNative(bprm->exec_fd, h->SHEntries, h->numSHEntries, h->SHStrTab, ".dynamic", "Dynamic", SHT_DYNAMIC, (void**)&h->Dynamic, &h->numDynamic);
        if(relocation_dump && h->Dynamic) DumpDynamicSections(h);
        // grab DT_REL & DT_RELA stuffs
        // also grab the DT_STRTAB string table
        {
            for (size_t i=0; i<h->numDynamic; ++i) {
                Elf64_Dyn d = h->Dynamic[i];
                Elf64_Word val = d.d_un.d_val;
                Elf64_Addr ptr = d.d_un.d_ptr;
                switch (d.d_tag) {
                case DT_REL:
                    h->rel = ptr;
                    break;
                case DT_RELSZ:
                    h->relsz = val;
                    break;
                case DT_RELENT:
                    h->relent = val;
                    break;
                case DT_RELA:
                    h->rela = ptr;
                    break;
                case DT_RELASZ:
                    h->relasz = val;
                    break;
                case DT_RELAENT:
                    h->relaent = val;
                    break;
                case DT_PLTGOT:
                    h->pltgot = ptr;
                    break;
                case DT_PLTREL:
                    h->pltrel = val;
                    break;
                case DT_PLTRELSZ:
                    h->pltsz = val;
                    break;
                case DT_JMPREL:
                    h->jmprel = ptr;
                    break;
                case DT_STRTAB:
                    h->DynStrTab = (char*)(ptr);
                    break;
                case DT_STRSZ:
                    h->szDynStrTab = val;
                    break;
                case DT_INIT: // Entry point
                    h->initentry = ptr;
                    printf_log(LOG_INFO, "The DT_INIT is at address %p\n", (void*)h->initentry);
                    break;
                case DT_INIT_ARRAY:
                    h->initarray = ptr;
                    printf_log(LOG_INFO, "The DT_INIT_ARRAY is at address %p\n", (void*)h->initarray);
                    break;
                case DT_INIT_ARRAYSZ:
                    h->initarray_sz = val / sizeof(Elf64_Addr);
                    printf_log(LOG_INFO, "The DT_INIT_ARRAYSZ is %zu\n", h->initarray_sz);
                    break;
                case DT_PREINIT_ARRAYSZ:
                    if(val)
                        printf_log(LOG_INFO, "Warning, PreInit Array (size=%d) present and ignored!\n", val);
                    break;
                case DT_FINI: // Exit hook
                    h->finientry = ptr;
                    printf_log(LOG_INFO, "The DT_FINI is at address %p\n", (void*)h->finientry);
                    break;
                case DT_FINI_ARRAY:
                    h->finiarray = ptr;
                    printf_log(LOG_INFO, "The DT_FINI_ARRAY is at address %p\n", (void*)h->finiarray);
                    break;
                case DT_FINI_ARRAYSZ:
                    h->finiarray_sz = val / sizeof(Elf64_Addr);
                    printf_log(LOG_INFO, "The DT_FINI_ARRAYSZ is %zu\n", h->finiarray_sz);
                    break;
                case DT_VERNEEDNUM:
                    h->szVerNeed = val;
                    printf_log(LOG_INFO, "The DT_VERNEEDNUM is %d\n", h->szVerNeed);
                    break;
                case DT_VERNEED:
                    h->VerNeed = (Elf64_Verneed*)ptr;
                    printf_log(LOG_INFO, "The DT_VERNEED is at address %p\n", h->VerNeed);
                    break;
                case DT_VERDEFNUM:
                    h->szVerDef = val;
                    printf_log(LOG_INFO, "The DT_VERDEFNUM is %d\n", h->szVerDef);
                    break;
                case DT_VERDEF:
                    h->VerDef = (Elf64_Verdef*)ptr;
                    printf_log(LOG_INFO, "The DT_VERDEF is at address %p\n", h->VerDef);
                    break;
                }
            }
            if(h->rel) {
                if(h->relent != sizeof(Elf64_Rel)) {
                    printf_log(LOG_INFO, "Rel Table Entry size invalid (0x%x should be 0x%zx)\n", h->relent, sizeof(Elf64_Rel));
                    FreeElfHeader(&h);
                    return NULL;
                }
                printf_log(LOG_INFO, "Rel Table @%p (0x%zx/0x%x)\n", (void*)h->rel, h->relsz, h->relent);
            }
            if(h->rela) {
                if(h->relaent != sizeof(Elf64_Rela)) {
                    printf_log(LOG_INFO, "RelA Table Entry size invalid (0x%x should be 0x%zx)\n", h->relaent, sizeof(Elf64_Rela));
                    FreeElfHeader(&h);
                    return NULL;
                }
                printf_log(LOG_INFO, "RelA Table @%p (0x%zx/0x%x)\n", (void*)h->rela, h->relasz, h->relaent);
            }
            if(h->jmprel) {
                if(h->pltrel == DT_REL) {
                    h->pltent = sizeof(Elf64_Rel);
                } else if(h->pltrel == DT_RELA) {
                    h->pltent = sizeof(Elf64_Rela);
                } else {
                    printf_log(LOG_INFO, "PLT Table type is unknown (size = 0x%zx, type=%ld)\n", h->pltsz, h->pltrel);
                    FreeElfHeader(&h);
                    return NULL;
                }
                if((h->pltsz / h->pltent)*h->pltent != h->pltsz) {
                    printf_log(LOG_INFO, "PLT Table Entry size invalid (0x%zx, ent=0x%x, type=%ld)\n", h->pltsz, h->pltent, h->pltrel);
                    FreeElfHeader(&h);
                    return NULL;
                }
                printf_log(LOG_INFO, "PLT Table @%p (type=%ld 0x%zx/0x%0x)\n", (void*)h->jmprel, h->pltrel, h->pltsz, h->pltent);
            }
            if(h->DynStrTab && h->szDynStrTab) {
                //DumpDynamicNeeded(h); cannot dump now, it's not loaded yet
            }
        }
        // look for PLT Offset
        int ii = FindSection(h->SHEntries, h->numSHEntries, h->SHStrTab, ".got.plt");
        if(ii) {
            h->gotplt = h->SHEntries[ii].sh_addr;
            h->gotplt_end = h->gotplt + h->SHEntries[ii].sh_size;
            printf_log(LOG_INFO, "The GOT.PLT Table is at address %p\n", (void*)h->gotplt);
        }
        ii = FindSection(h->SHEntries, h->numSHEntries, h->SHStrTab, ".got");
        if(ii) {
            h->got = h->SHEntries[ii].sh_addr;
            h->got_end = h->got + h->SHEntries[ii].sh_size;
            printf_log(LOG_INFO, "The GOT Table is at address %p..%p\n", (void*)h->got, (void*)h->got_end);
        }
        ii = FindSection(h->SHEntries, h->numSHEntries, h->SHStrTab, ".plt");
        if(ii) {
            h->plt = h->SHEntries[ii].sh_addr;
            h->plt_end = h->plt + h->SHEntries[ii].sh_size;
            printf_log(LOG_INFO, "The PLT Table is at address %p..%p\n", (void*)h->plt, (void*)h->plt_end);
        }
        // grab version of symbols
        ii = FindSection(h->SHEntries, h->numSHEntries, h->SHStrTab, ".gnu.version");
        if(ii) {
            h->VerSym = (Elf64_Half*)(h->SHEntries[ii].sh_addr);
            printf_log(LOG_INFO, "The .gnu.version is at address %p\n", h->VerSym);
        }
        // grab .text for main code
        ii = FindSection(h->SHEntries, h->numSHEntries, h->SHStrTab, ".text");
        if(ii) {
            h->text = (uintptr_t)(h->SHEntries[ii].sh_addr);
            h->textsz = h->SHEntries[ii].sh_size;
            printf_log(LOG_INFO, "The .text is at address %p, and is %zu big\n", (void*)h->text, h->textsz);
        }
        ii = FindSection(h->SHEntries, h->numSHEntries, h->SHStrTab, ".eh_frame");
        if(ii) {
            h->ehframe = (uintptr_t)(h->SHEntries[ii].sh_addr);
            h->ehframe_end = h->ehframe + h->SHEntries[ii].sh_size;
            printf_log(LOG_INFO, "The .eh_frame section is at address %p..%p\n", (void*)h->ehframe, (void*)h->ehframe_end);
        }
        ii = FindSection(h->SHEntries, h->numSHEntries, h->SHStrTab, ".eh_frame_hdr");
        if(ii) {
            h->ehframehdr = (uintptr_t)(h->SHEntries[ii].sh_addr);
            printf_log(LOG_INFO, "The .eh_frame_hdr section is at address %p\n", (void*)h->ehframehdr);
        }

        LoadNamedSectionNative(bprm->exec_fd, h->SHEntries, h->numSHEntries, h->SHStrTab, ".dynstr", "DynSym Strings", SHT_STRTAB, (void**)&h->DynStr, NULL);
        LoadNamedSectionNative(bprm->exec_fd, h->SHEntries, h->numSHEntries, h->SHStrTab, ".dynsym", "DynSym", SHT_DYNSYM, (void**)&h->DynSym, &h->numDynSym);
    }

    return h;

}
static int CalcLoadAddrNative(elfheader_t* head, size_t align)
{
    head->memsz = 0;
    head->paddr = head->vaddr = ~(uintptr_t)0;
    head->align = align;
    for (size_t i=0; i<head->numPHEntries; ++i)
        if(head->PHEntries[i].p_type == PT_LOAD) {
            if(head->paddr > (uintptr_t)head->PHEntries[i].p_paddr)
                head->paddr = (uintptr_t)head->PHEntries[i].p_paddr;
            if(head->vaddr > (uintptr_t)head->PHEntries[i].p_vaddr)
                head->vaddr = (uintptr_t)head->PHEntries[i].p_vaddr;
        }
    if(head->vaddr==~(uintptr_t)0 || head->paddr==~(uintptr_t)0) {
        printf_log(LOG_INFO, "Error: v/p Addr for Elf Load not set\n");
        return 1;
    }
    head->stacksz = 1024*1024;          //1M stack size default?
    head->stackalign = 16;   // default align for stack
    return 0;
}
static void init_main_elf(elfheader_t* elf_header,int fd, uintptr_t load_addr,
        size_t align)
{
    AddElfHeader(my_context, elf_header);
    elf_header->latx_type = LATX_ELF_TYPE_MAIN;
    ElfHeadReFix(elf_header, load_addr);
    collectX86free(elf_header);
    if (CalcLoadAddrNative(elf_header, align)) {
        printf_log(LOG_INFO, "Error: reading elf header of %s\n", my_context->argv[0]);
        close(fd);
        free_contextargv();
        FreeBox64Context(&my_context);
        exit(-1);
    }
    close(fd);

    AddSymbols(my_context->maplib, GetMapSymbol(my_context->maplib), GetWeakSymbol(my_context->maplib), GetLocalSymbol(my_context->maplib), elf_header);

    AddMainElfToLinkmap(elf_header);

    if (LoadNeededLibs(elf_header, my_context->maplib, &my_context->neededlibs, NULL, 0, 0, my_context)) {
        printf_log(LOG_INFO, "Error: loading needed libs in elf %s\n", my_context->argv[0]);
    }
    ResetSpecialCaseMainElf(elf_header);
}
int wine_option_kzt;
int kzt_init(char** argv, int argc,char** target_argv, int target_argc,
        struct linux_binprm* bprm) {
    if (!option_kzt && !wine_option_kzt) {
        return -1;
    }
    my_context = NewBox64Context(bprm->argc);
    if (option_kzt && info->interpreter_path) {
        elf_header = LoadFromNative(bprm, info);
    }
    if (option_kzt == 1 && elf_header) {
        option_kzt = CheckEnableKZT(elf_header, target_argv, target_argc);
    }
    const char* prog = argv[1];
    LoadEnvVars(my_context);
    my_context->box64path = ResolveFile(argv[0], &my_context->box64_path);
    my_context->envc = bprm->envc;
    my_context->envv = bprm->envp ;
    my_context->argv[0] = ResolveFile(prog, &my_context->box64_path);

    for(int i=1;i<target_argc; i++) {
        my_context->argv[i] = target_argv[i-1];
    }

    if(!(my_context->fullpath = realpath(my_context->argv[0], NULL))) {
        my_context->fullpath = box_strdup(my_context->argv[0]);
    }

    if(elf_header != NULL){
        init_main_elf(elf_header, bprm->exec_fd, info1.load_addr, info->alignment);
    }
    return 0;
}

struct x86_ld_info {
    int reg;
    intptr_t addr;
};
static struct x86_ld_info * ld_info = NULL;
extern void* x86free;
extern void* x86realloc;
extern void* x86pthread_setcanceltype;
static int x64free_fini = 0;
//before _init, free possibly be call.so x86free must be refleshed everytime.
int collectX86free(elfheader_t* h)
{
    if (x64free_fini) {
        return 0;
    }
    int cnt;
    Elf64_Rela *rela;
    void* found_malloc = NULL;
    void* found_free = NULL;
    void* found_realloc = NULL;
    struct malloc_map * m;
    Elf64_Sym *sym = NULL;
    for (size_t i=0; i<h->numDynSym; ++i) {
        sym = h->DynSym+i;
        if (h->DynSym[i].st_shndx != SHN_UNDEF && sym->st_value) {
            const char * symname = h->DynStr+sym->st_name;
            if (!strcmp(symname, "malloc")) {
                found_malloc = (void*)sym->st_value+h->delta;
                printf_log(LOG_DEBUG, "latx x86malloc=%p type=0x%x from %s\n", x86free, ELF64_ST_TYPE(sym->st_info), h->path);
                if (found_free && found_realloc) {
                    goto found;
                }
            } else if (!strcmp(symname, "free")) {
                found_free = (void*)sym->st_value+h->delta;
                printf_log(LOG_DEBUG, "latx x86free=%p type=0x%x from %s\n", x86free, ELF64_ST_TYPE(sym->st_info), h->path);
                if (found_malloc && found_realloc) {
                    goto found;
                }
            } else if (!strcmp(symname, "realloc")) {
                found_realloc = (void*)sym->st_value+h->delta;
                printf_log(LOG_DEBUG, "latx x86realloc=%p type=0x%x from %s\n", x86realloc, ELF64_ST_TYPE(sym->st_info), h->path);
                if (found_malloc && found_free) {
                    goto found;
                }
            }
        }
    }
    return -1;
found:
    /*
      * If the plt of this elf file has free, __libc_free, __free jump_slot, this jump_slot will be rewrited by kzt bridge, a moment later.
      * So this file should be skiped. If do not do this, when x86free be called, Target exe will fall into dead loop.
    */
    cnt = h->pltsz / h->pltent;
    rela = (Elf64_Rela *)(h->jmprel + h->delta);
    for (int i=0; i<cnt; ++i) {
        int t = ELF64_R_TYPE(rela[i].r_info);
        if(t == R_X86_64_JUMP_SLOT){
            Elf64_Sym *sym = &h->DynSym[ELF64_R_SYM(rela[i].r_info)];
            const char* symname = SymName(h, sym);
            if (!strcmp(symname, "free") ||
                !strcmp(symname, "__libc_free") ||
                !strcmp(symname, "__free")) {
                return -1;
            }
        }
    }
    m = malloc(sizeof(struct malloc_map));
    m->mallocp = found_malloc;
    m->freep = found_free;
    m->reallocp = found_realloc;
    m->h = h;
    AddMallocMap(my_context, m);
    return 0;
}
static int findx86pthread_setcanceltype(elfheader_t* h)
{
    if (x86pthread_setcanceltype ||x64free_fini) {
        return 0;
    }
    Elf64_Sym *sym = NULL;
    for (size_t i=0; i<h->numDynSym; ++i) {
        sym = h->DynSym+i;
        if (h->DynSym[i].st_shndx != SHN_UNDEF && sym->st_value) {
            const char * symname = h->DynStr+sym->st_name;
            if (!strcmp(symname, "malloc")) {
                x86pthread_setcanceltype = (void*)sym->st_value+h->delta;
                printf_log(LOG_DEBUG, "latx x86pthread_setcanceltype=%p type=0x%x from %s\n", x86pthread_setcanceltype, ELF64_ST_TYPE(sym->st_info), h->path);
                return 0;
            }
        }
    }
    return -1;
}
static char* kzt_find_realsofilepath(char * filepath, char *filetmp)
{
    snprintf(filetmp , PATH_MAX, "%s%s", interp_prefix, filepath);
    if (FileExist(filetmp, IS_FILE)) {
        printf_log(LOG_DEBUG, "%s filename change to \"%s\"\n", __func__, filepath);
        return filetmp;
    }
    //file must exist for kzt_tb_callback.
    return filepath;
}
extern const char* libcName;
static void kzt_tb_callback(CPUX86State *env)
{
    struct link_map_x64 * my_lm = (struct link_map_x64 *)env->regs[R_EAX + ld_info->reg];
    elfheader_t *h = NULL;
    if ((!my_lm ||!my_lm->l_name ||!strlen(my_lm->l_name) || ! my_lm->l_addr)&& my_lm->l_addr != info1.load_addr) {
        printf_log(LOG_DEBUG, "error %d debug %s link_map = %p{0x%lx, %s}\n", getpid(), __func__, my_lm, my_lm->l_addr, my_lm->l_name);
        return;
    }
    printf_log(LOG_DEBUG, "%d debug %s link_map = %p{0x%lx, %s}\n", getpid(), __func__, my_lm, my_lm->l_addr, my_lm->l_name);
    char * rfilename = my_lm->l_name;
    if (strstr(basename(rfilename), "ld-linux-x86-64.so.2")) {
        AddDebugInfo(LIB_EMULATED, my_lm->l_name, my_lm->l_map_start, my_lm->l_map_end);
        return;
    }
    char filetmp[PATH_MAX] = {0};
    if (rfilename[0] == '/') {
        rfilename = kzt_find_realsofilepath(rfilename, filetmp);
    }
    char * rbasename = basename(rfilename);
    library_t* lib = NewLibrary(rbasename, my_context);
    if (lib) {
        const char* libs[] = {rbasename};
        AddNeededLib(my_context->maplib, &my_context->neededlibs, NULL, 0, 1, libs, 1, my_context);
    }
    if (!lib && (!strncmp(rbasename, "libSDL", 6)||!strncmp(rbasename, "libCgGL.so", strlen("libCgGL.so")))) {
        printf_log(LOG_DEBUG, "%s libSDL need libGL.so.1\n", __func__);
        const char* libs[] = {"libGL.so.1"};
        AddNeededLib(my_context->maplib, &my_context->neededlibs, NULL, 0, 1, libs, 1, my_context);
    }
    FILE *f = fopen(rfilename, "rb");
    if(!f) {
        printf_log(LOG_INFO, "%s Error: Cannot open \"%s\"\n", __func__, rfilename);
        return;
    }
    h = LoadAndCheckElfHeader(f, rfilename, 0);
    ElfHeadReFix(h, my_lm->l_addr);
    fclose(f);
    collectX86free(h);
    if(!x86free &&!strcmp(rbasename, libcName)) {
        struct malloc_map * m = SearchMallocMap(my_context, (char *)libcName);
        lsassert(m);
        x86free = m->freep;
        x86realloc = m->reallocp;
    }
    if(!x86pthread_setcanceltype &&!strcmp(rbasename, libcName)) {
        findx86pthread_setcanceltype(h);
    }
    AddElfHeader(my_context, h);
    LoadNeededLibs(h, my_context->maplib, &my_context->neededlibs, NULL, 0, 0, my_context);
    RelocateElf(my_context->maplib, NULL, 0, h);
    RelocateElfPlt(my_context->maplib, NULL, 0, h);
    if (lib) {
        AddDebugInfo(LIB_WRAPPED, my_lm->l_name, my_lm->l_map_start, my_lm->l_map_end);
    } else {
        AddDebugInfo(LIB_EMULATED, my_lm->l_name, my_lm->l_map_start, my_lm->l_map_end);
    }
}
static TranslationBlock* test_tb;
static void test_x86free(CPUX86State *env)
{
    static int cnt;
    static uintptr_t ptr;
    static int has_found;
    if (has_found) {
        return;
    }
    if (!ptr) {
        ptr = env->regs[R_EDI];
    } else if (ptr != env->regs[R_EDI]) {
        //destroy callback
        mmap_lock();
#ifdef CONFIG_USER_ONLY
        tb_phys_invalidate(test_tb, test_tb->itree.start);
#else
        tb_phys_invalidate(test_tb, test_tb->page_addr[0]);
#endif
        mmap_unlock();
        has_found = 1;
        printf_log(LOG_DEBUG, "%s latx final find free=%p, realloc=%p\n", __func__, x86free, x86realloc);
        return;
    }

    if (cnt > 0) {
        for (int i = 0; i < my_context->mallocmapsize;i++) {
            if (x86free == my_context->mallocmaps[i]->freep && i <my_context->mallocmapsize -1) {
                struct malloc_map * m = my_context->mallocmaps[i + 1];
                static uint32 test_ld [2] = {0};
                x86free = m->freep;
                x86realloc = m->reallocp;
                target_ulong eip = env->eip;
                CPUState *cpu = env_cpu(env);
                memset(&test_ld, 0, sizeof(test_ld));
                mmap_lock();
#ifdef CONFIG_USER_ONLY
		tb_phys_invalidate(test_tb, test_tb->itree.start);
#else
		tb_phys_invalidate(test_tb, test_tb->page_addr[0]);
#endif

                mmap_unlock();
                printf_log(LOG_DEBUG, "%s latx try find free=%p, realloc=%p from %s\n", __func__, x86free, x86realloc, ((elfheader_t*)m->h)->path);
                test_tb = kzt_add_fucn_by_addr((uint32 *)&test_ld,  cpu, (uintptr_t)x86free, target_latx_ld_callback, test_x86free);
                env->eip = eip;
                cnt = 0;
                return;
            }
        }
    }
    cnt++;
}
static inline gint tb_sort_cmp(const void *ap, const void *bp)
{
    const struct malloc_map *a = *(const struct malloc_map **)ap;
    const struct malloc_map *b = *(const struct malloc_map **)bp;
    if (a->freep < b->freep) {
	return 1;
    }
    return -1;
}
static void finiReFlesh(elfheader_t* exech)
{
    CPUState *cpu;
    static uint32 test_ld [2] = {0};
    lsassert(my_context->mallocmapsize);
    struct malloc_map * m;

    if (my_context->mallocmapsize == 1) {
        m = my_context->mallocmaps[0];
        x86free = m->freep;
        x86realloc = m->reallocp;
        return;
    }
    /* For select x86free, x86realloc from my_context->mallocmaps
      * Sort in descending order for freep.
      * Because:
      * When an executable program depends on multiple libraries and
      * all of them provide symbols with the same name (such as free functions),
      * the dynamic linker will parse the symbols according to the loading order of the libraries.
      * The symbols defined in the first loaded library have higher priority.
    */
    if (my_context->mallocmaps[0]->h == my_context->elfs[0]) {//elf[0] is exe file
        if (my_context->mallocmapsize > 2) {
            qsort(my_context->mallocmaps[1], my_context->mallocmapsize - 1, sizeof(struct malloc_map *), tb_sort_cmp);
        }
    } else {
        qsort(my_context->mallocmaps, my_context->mallocmapsize, sizeof(struct malloc_map *), tb_sort_cmp);
    }
    m = my_context->mallocmaps[0];
    x86free = m->freep;
    x86realloc = m->reallocp;
    CPU_FOREACH(cpu) {
        if (cpu) {
            break;
        }
    }
    CPUArchState *env = cpu->env_ptr;
    target_ulong eip = env->eip;
    test_tb = kzt_add_fucn_by_addr((uint32 *)&test_ld,  cpu, (uintptr_t)x86free, target_latx_ld_callback, test_x86free);
    env->eip = eip;
    printf_log(LOG_DEBUG, "%s latx try find free=%p, realloc=%p from %s\n", __func__, x86free, x86realloc, ((elfheader_t*)m->h)->path);
}
static void kzt_exectb_callback(CPUX86State *env)
{
    finiReFlesh(elf_header);
    x64free_fini = 1;
    RelocateElf(my_context->maplib, NULL, 0, elf_header);
    RelocateElfPlt(my_context->maplib, NULL, 0, elf_header);
    AddDebugInfo(LIB_EMULATED, elf_header->name, info1.start_code, info1.end_code);
}
static TranslationBlock* kzt_add_fucn_by_addr(uint32* inst_old, CPUState *cpu, uintptr_t addr, int (*latx_ld_callback)(void *, void (*)(CPUX86State *)), void (*kzt_tb_callback)(CPUX86State *))
{
    uint32 jmpinst [2] = {0};
    uint32 * tmp, *pinsn;
    CPUArchState *env = cpu->env_ptr;
    int isOverWrite = 0;
    int pos = 0;
    void * ld_callback_context = tcg_ctx->code_gen_ptr;
    int backlen = latx_ld_callback(tcg_ctx->code_gen_ptr, kzt_tb_callback);
    backlen *= 4;
    tcg_ctx->code_gen_ptr += backlen;
    env->eip = addr - env->segs[R_CS].base;
    printf_log(LOG_DEBUG, "------------init_tb_callback_bridge--cpu=%p--pid=%d---latx_ld_callback %p to %p----\n", cpu, getpid(), ld_callback_context, tcg_ctx->code_gen_ptr);
    TranslationBlock * tb = kzt_tb_find_exp(cpu, NULL, 0, cpu->tcg_cflags);
    printf_log(LOG_DEBUG, "%s add pc = %lx jmp blok=%p tb=%p\n", __func__, addr, ld_callback_context, tb);
    light_tb_target_set_jmp_target(0, (uintptr_t)tb->tc.ptr, (uintptr_t)&jmpinst, (uintptr_t)ld_callback_context);
    tmp = (uint32 *)((uintptr_t)ld_callback_context + backlen - 4*4);
    pinsn =  (uint32 *)tb->tc.ptr;
    if (*pinsn == 0x50000800 && *(pinsn + 1) == 0x03400000) {//skip insts:--> b 8(0x8)  ;andi $r0,$r0,0x0
        pos = 2;
        pinsn += pos;
    }
    #if 0
    case: pinsn is direct insn (e.g.: b 8)
    #endif
    if (((*pinsn) & 0xfc000000) == 0x50000000 ||// b
        (((*pinsn) & 0xfe000000) == 0x1e000000 &&/* pcaddu18i */
        (*(pinsn + 1) & 0xfc000000) == 0x4c000000)) {
        printf_log(LOG_DEBUG, "------------kzt_add_fucn_by_addr tb has fixed return --cpu=%p--pid=%d------\n", cpu, getpid());
        isOverWrite = 1;
        lsassert(jmpinst[0] && jmpinst[1]);
    }
    if (jmpinst[1]) {//long jmp
        if (!isOverWrite) {
            *tmp = *((uint32 *)tb->tc.ptr + pos);
            *(tmp + 1) = *((uint32 *)tb->tc.ptr + 1 + pos);
            if (!inst_old[0]) inst_old[0] = *((uint32 *)tb->tc.ptr + pos);
            if (!inst_old[1]) inst_old[1] = *((uint32 *)tb->tc.ptr + 1 + pos);
        } else {
            *tmp = inst_old[0];
            *(tmp + 1) = inst_old[1];
        }
        *((uint32 *)tb->tc.ptr + pos) = jmpinst[0];
       *((uint32 *)tb->tc.ptr + 1 + pos) = jmpinst[1];
       memset(&jmpinst, 0, sizeof(jmpinst));
        light_tb_target_set_jmp_target(0, (uintptr_t)(tmp + 2), (uintptr_t)&jmpinst, (uintptr_t)((uint32 *)tb->tc.ptr + 2 + pos));
         *(tmp + 2) =  jmpinst[0];
         *(tmp + 3) =  jmpinst[1];
    } else {
         if (!isOverWrite) {
            lsassert(*((uint32 *)tb->tc.ptr + pos));
            *tmp = *((uint32 *)tb->tc.ptr + pos);
             if (!inst_old[0]) inst_old[0] = *((uint32 *)tb->tc.ptr + pos);
        } else {
            lsassert(inst_old[0]);
            *tmp = inst_old[0];
        }
        *((uint32 *)tb->tc.ptr + pos) = jmpinst[0];
        memset(&jmpinst, 0, sizeof(jmpinst));
        light_tb_target_set_jmp_target(0, (uintptr_t)(tmp + 1), (uintptr_t)&jmpinst, (uintptr_t)((uint32 *)tb->tc.ptr + 1 + pos));
         *(tmp + 1) =  jmpinst[0];
    }
    return tb;
}
struct ins_part_s {
    char search[5];
    int search_len;
    uint32_t offset;
};
#define MY_PARTY_OFFSET 0x32 - 1
#define MY_PARTY_OFFSET_UBUNTU 0x36 - 1

#define MY_PARTY_INS_MAX 8
static struct x86_ld_info * find_ld_part(char * start, int len)
{
    struct x86_ld_info * ret = NULL;
    char * old_start = start;
    char searchpopret_part[] = {
        0x5b,                   //pop    rbx
        0x41, 0x5c,        //pop    r12
        0x41, 0x5d,        //pop    r13
        0x41, 0x5e,       //pop    r14
        0x41, 0x5f,        //pop    r15
        0x5d,                 //pop    rbp
        0xc3                  //ret
    };
    char *ld_find = NULL;
    static char* _dl_relocate_object_end;
#define K_OR(offset_s)          {.search = {0x41, 0x80}, .search_len = 2, .offset = offset_s}
#define K_CMP_48(offset_s)      {.search = {0x48, 0x83}, .search_len = 2, .offset = offset_s}
#define K_CMP_49(offset_s)      {.search = {0x49, 0x83}, .search_len = 2, .offset = offset_s}
#define K_JNE(offset_s)         {.search = {0x0f, 0x85}, .search_len = 2, .offset = offset_s}
#define K_LEA(offset_s)         {.search = {0x48, 0x8d}, .search_len = 2, .offset = offset_s}
#define K_MOV(offset_s)         {.search = {0x49, 0x8b}, .search_len = 2, .offset = offset_s}
#define K_TEST(offset_s)        {.search = {0x48, 0x85}, .search_len = 2, .offset = offset_s}
#define K_SKIP(offset_s)        {.search = {0, 0}, .search_len = 0, .offset = offset_s}

#define K_ISNOT_SKIP(p)         (p.offset != 0)
#define MAX_INS_PART_S          7

	struct ld_part_s {
	    struct ins_part_s ins [MAX_INS_PART_S];
	    int part_offset;
	};

	struct ld_part_s all_part[] = {
        {
            .ins = {
                K_OR(0),
                K_CMP_48(8),
                K_JNE(8),
                K_CMP_49(6),
                K_JNE(8),
                K_LEA(6),
                K_SKIP(0),
            },
            .part_offset = MY_PARTY_OFFSET//for debian
        },
        {
            .ins = {
                K_OR(0),
                K_CMP_48(8),
                K_JNE(8),
                K_MOV(6),
                K_TEST(7),
                K_JNE(3),
                K_LEA(6),
            },
            .part_offset = MY_PARTY_OFFSET_UBUNTU//for Ubuntu 21.04
        },
        {   // AOSC 25.05.16 GNU C Library (GNU libc) stable release version 2.40.
            .ins = {
                K_OR(0),          // 0x41 0x80 0x8e 0x54 0x03 0x00 0x00 0x08    (orb $0x8,0x354(%r14))
                K_CMP_48(8),      // 0x48 0x83 0xbd 0x10 0xff 0xff 0xff 0x00    (cmpq   $0x0,-0xf0(%rbp))
                K_JNE(8),         // 0x0f 0x85 0xe0 0x17 0x00 0x00              (jne)
                K_CMP_49(6),      // 0x49 0x83 0xbe 0xa8 0x04 0x00 0x00 0x00    (cmpq   $0x0,0x4a8(%r14))
                K_JNE(8),         // 0x0f 0x85 0xea 0x0b 0x00 0x00              (jne)
                K_LEA(6),         // 0x48 0x8d 0x65 0xd8                        (lea    -0x28(%rbp),%rsp)
                K_SKIP(0),
            },
            .part_offset = 0x33
        },
        {   // AOSC OS (Core 12.2.2), glibc 2.40 (EmuKit 20250909~pre20250911T080911Z)
            .ins = {
                K_OR(0),          // 0x41 0x80 0x8e 0x54 0x03 0x00 0x00 0x08    (orb $0x8,0x354(%r14))
                K_TEST(8),        // 0x48 0x85 0xdb                             (test %rbx,%rbx)
                K_JNE(3),         // 0x0f 0x85 0xe0 0x17 0x00 0x00              (jne)
                K_CMP_49(6),      // 0x49 0x83 0xbe 0xa8 0x04 0x00 0x00 0x00    (cmpq   $0x0,0x4a8(%r14))
                K_JNE(8),         // 0x0f 0x85 0x50 0x0b 0x00 0x00              (jne)
                K_LEA(6),         // 0x48 0x8d 0x65 0xd8                        (lea    -0x28(%rbp),%rsp)
                K_SKIP(0),
            },
            .part_offset = 0x2e
        },
        {   // debian sid GNU C Library (Debian GLIBC 2.41-7) stable release version 2.41.
            .ins = {
                K_OR(0),          // 0x41 0x80 0x8e 0x54 0x03 0x00 0x00 0x08    (orb $0x8,0x354(%r14))
                K_TEST(8),        // 0x48 0x85 0xdb                             (test %rbx,%rbx)
                K_JNE(3),         // 0x0f 0x85 0xbd 0x14 0x00 0x00              (jne)
                K_LEA(6),         // 0x48 0x8d 0x64 0xd8                        (lea -0x28(%rbp),%rsp)
            },
            .part_offset = 0x20
        },
        {   // glibc 2.42 (Yongbao x86_64_runtime)
            .ins = {
                K_OR(0),          // 0x41 0x80 0x8e 0x54 0x03 0x00 0x00 0x08    (orb $0x8,0x354(%r14))
                K_CMP_48(8),      // 0x48 0x83 0xbd 0x28 0xff 0xff 0xff 0x00    (cmpq   $0x0,-0xd8(%rbp))
                K_JNE(8),         // 0x0f 0x85 0x2a 0x18 0x00 0x00              (jne)
                K_LEA(6),         // 0x48 0x8d 0x65 0xd8                        (lea    -0x28(%rbp),%rsp)
                K_SKIP(0),
            },
            .part_offset = 0x25
        }
    };
    while((_dl_relocate_object_end = memmem(start, len - (start - old_start), searchpopret_part,
    sizeof(searchpopret_part)))) {
        start = _dl_relocate_object_end + 1;
        for (int j = 0; j < sizeof(all_part) / sizeof(struct ld_part_s); j++) {
            char *my_start = _dl_relocate_object_end - all_part[j].part_offset + sizeof(searchpopret_part);
            int no_match = 0;
            for (int i = 0; i < MAX_INS_PART_S; i++) {
                my_start += all_part[j].ins[i].offset;
                if (K_ISNOT_SKIP(all_part[j].ins[i]) &&
                    memmem(my_start, MY_PARTY_INS_MAX, all_part[j].ins[i].search, all_part[j].ins[i].search_len) != my_start) {
                    no_match = 1;
                    break;
                }
            }
            if (!no_match) {
                ld_find = _dl_relocate_object_end - all_part[j].part_offset + sizeof(searchpopret_part);
                goto suc;
            }
        }
    }
    fprintf(stderr, "Lat can't find ldinfo.\n");
    return NULL;
suc:
    ret = malloc(sizeof(struct x86_ld_info));
    ret->addr = (uintptr_t) ld_find;
    ret->reg = (*(ld_find+ 2)) & 0xf;
    printf_log(LOG_DEBUG, "debug find ld so 0x%lx reg = %d\n", ret->addr, ret->reg);
    return ret;
}
static struct x86_ld_info *find_ld_bridge(void* info)
{
    struct image_info * execinfo = (struct image_info *)info;
    struct x86_ld_info * ret = NULL;
    struct stat st;
    uint8_t *ld_start = (uint8_t *)execinfo->load_bias;
    char *real_dl_file = ResolveFile(basename(execinfo->interpreter_path), &my_context->box64_ld_lib);
    if (stat(real_dl_file, &st)) {
        return NULL;
    }
    size_t file_len = st.st_size;

    uint8_t* fix_addr = (uint8_t *)(ld_start+0xbc15);
    if ((*(uint64_t *)fix_addr & 0xffffffffff00ffff) == 0x040000031c008041) {
        ret = malloc(sizeof(struct x86_ld_info));
        ret->addr = (uintptr_t) fix_addr;
        ret->reg = (*(fix_addr+ 2)) & 0xf;
        printf_log(LOG_DEBUG, "debug find ld so 0x%lx reg = %d\n", ret->addr, ret->reg);
        return ret;
    }
    fix_addr = (uint8_t *)(ld_start+ 0xdcc2);
    if ((*(uint64_t *)fix_addr & 0xffffffffff00ffff) == 0x040000031c008041) {//for Ubuntu 21.04 ld-2.31.so
        ret = malloc(sizeof(struct x86_ld_info));
        ret->addr = (uintptr_t) fix_addr;
        ret->reg = (*(fix_addr+ 2)) & 0xf;
        printf_log(LOG_DEBUG, "debug find ld so 0x%lx reg = %d\n", ret->addr, ret->reg);
        return ret;
    }
    fix_addr = (uint8_t *)(ld_start+0xe6d8);
    if ((*(uint64_t *)fix_addr & 0xffffffffff00ffff) == 0x0800000334008041) {
        ret = malloc(sizeof(struct x86_ld_info));
        ret->addr = (uintptr_t) fix_addr;
        ret->reg = (*(fix_addr+ 2)) & 0xf;
        printf_log(LOG_DEBUG, "debug find ld so 0x%lx reg = %d\n", ret->addr, ret->reg);
        return ret;
    }
    fix_addr = (uint8_t *)(ld_start+ 0xe7cd);
    if ((*(uint64_t *)fix_addr & 0xffffffffff00ffff) == 0x0800000354008041) {
        ret = malloc(sizeof(struct x86_ld_info));
        ret->addr = (uintptr_t) fix_addr;
        ret->reg = (*(fix_addr+ 2)) & 0xf;
        printf_log(LOG_DEBUG, "debug find ld so 0x%lx reg = %d\n", ret->addr, ret->reg);
        return ret;
    }
    // AOSC 25.05.16 GNU C Library ( GLIBC 2.40)
    fix_addr = (uint8_t *)(ld_start+ 0xdb72);
    if ((*(uint64_t *)fix_addr & 0xffffffffff00ffff) == 0x0800000354008041) {
        ret = malloc(sizeof(struct x86_ld_info));
        ret->addr = (uintptr_t) fix_addr;
        ret->reg = (*(fix_addr+ 2)) & 0xf;
        printf_log(LOG_DEBUG, "debug find ld so 0x%lx reg = %d\n", ret->addr, ret->reg);
        return ret;
    }
    // debian sid GNU C Library (Debian GLIBC 2.41-7) stable release version 2.41. addr 0x4008403ffe
    fix_addr = (uint8_t *)(ld_start+ 0xdffe);
    if ((*(uint64_t *)fix_addr & 0xffffffffff00ffff) == 0x0800000354008041) {
        ret = malloc(sizeof(struct x86_ld_info));
        ret->addr = (uintptr_t) fix_addr;
        ret->reg = (*(fix_addr+ 2)) & 0xf;
        printf_log(LOG_DEBUG, "debug find ld so 0x%lx reg = %d\n", ret->addr, ret->reg);
        return ret;
    }
    return find_ld_part((char *)ld_start, file_len);
    return NULL;
}

void init_tb_callback_bridge(CPUState *cpu, void* info)
{
    struct image_info * execinfo = (struct image_info *)info;
    static uint32 jmpinst_exec [2] = {0};
    static uint32 jmpinst_ld [2] = {0};
    if (!ld_info) {
        ld_info = find_ld_bridge(info);
    }
    if (!ld_info) {
        lsassertm(0,"can't find ld callback tb.");
        option_kzt = 0;
        return;
    }
    CPUArchState *env = cpu->env_ptr;
    target_ulong eip = env->eip;
    kzt_add_fucn_by_addr((uint32 *)&jmpinst_exec, cpu, execinfo->exec_entry, target_latx_ld_callback, kzt_exectb_callback);
    kzt_add_fucn_by_addr((uint32 *)&jmpinst_ld, cpu, ld_info->addr, target_latx_ld_callback, kzt_tb_callback);
    env->eip = eip;
}
void kzt_bridge_init(void)
{
    CPUState *cpu_tmp;
    kzt_tbbridge_init();
    CPU_FOREACH(cpu_tmp) {
        if (cpu_tmp && elf_header  && option_kzt) {
            init_tb_callback_bridge(cpu_tmp, &info1);
        }
    }
}

static elfheader_t* wine_elf_header;
static void m_handle_wine64(char * file_name, abi_ulong start)
{
    FILE *f = fopen(file_name, "rb");
    if(!f) {
        printf_log(LOG_INFO, "%s Error: Cannot open \"%s\"\n", __func__, file_name);
        return;
    }
    wine_elf_header = LoadAndCheckElfHeader(f, file_name, 0);
    init_main_elf(wine_elf_header, -1, start, info->alignment);
    fclose(f);
}
static void m_handle_ld(char * file_name, abi_ulong start)
{
    elf_header = wine_elf_header;
    option_kzt = wine_option_kzt;
    struct image_info execinfo = {
        .interpreter_path = file_name,
        .exec_entry = elf_header->entrypoint + elf_header->delta,
        .load_bias = start
    };
    CPUState *cpu_tmp;
    CPU_FOREACH(cpu_tmp) {
            if (cpu_tmp  && option_kzt) {
                init_tb_callback_bridge(cpu_tmp, &execinfo);
            }
    }
}
static my_foundfd_t m_f_fd[2] = {
    {
        .type = M_F_FD_WINE,
        .inited = 0,
        .match = "wine64",
        .mhanle = m_handle_wine64
    },
    {
        .type = M_F_FD_LD,
        .inited = 0,
        .match = "ld-linux-x86-64.so.2",
        .mhanle = m_handle_ld
    },
};

void kzt_wine_bridge(abi_ulong start, int fd)
{
    if (!option_kzt && !wine_option_kzt) {
        return;
    }
    if (is_user_map && !elf_header && fd >= 3) {
        char *proc_link, *file_name;
        int len;
        proc_link = g_strdup_printf("/proc/self/fd/%d", fd);
        file_name = g_malloc0(PATH_MAX);
        len = readlink(proc_link, file_name, PATH_MAX - 1);
        if (len < 0) {
            len = 0;
        }
        file_name[len] = '\0';
        for (int i = 0; i < sizeof(m_f_fd)/sizeof(my_foundfd_t);i++) {
            if (m_f_fd[i].inited) {
                continue;
            }
            if(strstr(basename(file_name), m_f_fd[i].match)) {
                m_f_fd[i].inited = 1;
                m_f_fd[i].mhanle(file_name, start);
            }
        }
        g_free(proc_link);
        g_free(file_name);
    }
}

void kzt_wine_init_x86(void)
{
    if (!option_kzt ||!latx_wine  ||my_context->mallocmapsize) {
        return;
    }
    struct malloc_map* m = malloc(sizeof(struct malloc_map));
    m->mallocp = (void *)(uintptr_t)RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlsym, 2,
    0, "malloc");
    ;
    m->freep = (void *)(uintptr_t)RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlsym, 2,
    0, "free");
    m->reallocp = (void *)(uintptr_t)RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlsym, 2,
    0, "realloc");
    m->h = wine_elf_header;
    AddMallocMap(my_context, m);
    x86free = m->freep;
    x86realloc = m->reallocp;
}

elfheader_t* loadElfFromFile(const char* name)
{
    elfheader_t* h = NULL;
    char *tmp = ResolveFile(name, &my_context->box64_ld_lib);
    if (FileExist(tmp, IS_FILE)) {
        FILE *f = fopen(tmp, "rb");
        if (!f) {
            printf_log(LOG_NONE, "Error: Cannot open %s\n", tmp);
            return NULL;
        }
        h = LoadAndCheckElfHeader(f, tmp, 0);
        ElfHeadReFix(h, loadSoaddrFromMap(tmp));
        if ((uintptr_t)h->VerSym > (uintptr_t)h->delta) {
            h->delta = 0;
        }
    } else {
        lsassertm(0, "cannot find %s\n", tmp);
    }
    return h;
}
#pragma GCC diagnostic pop
