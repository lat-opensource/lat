/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct { long x,y; } Vector;
typedef struct { short n_contours,n_points; Vector *points; char *tags;
                 short *contours; int flags; } Outline;
typedef struct {
    int (*move)(const Vector *,void *);
    int (*line)(const Vector *,void *);
    int (*conic)(const Vector *,const Vector *,void *);
    int (*cubic)(const Vector *,const Vector *,const Vector *,void *);
    int shift; long delta;
} Functions;
extern int FT_Outline_Decompose(const Outline *,const Functions *,void *);
static unsigned hits;
static int guest_error;
static int point(const Vector *v,void *unused) {
    (void)v;(void)unused;hits++;return guest_error ? 42 : 0;
}
static int conic(const Vector *a,const Vector *b,void *u) {
    (void)a;return point(b,u);
}
static int cubic(const Vector *a,const Vector *b,const Vector *c,void *u) {
    (void)a;(void)b;return point(c,u);
}
int main(int argc,char **argv) {
    if (argc<2) return 2;
    guest_error=!strcmp(argv[1],"guest-error");
    int failed_prepare=!strcmp(argv[1],"prepare-fail");
    unsigned iterations=argc>2 ? strtoul(argv[2],NULL,10) : 1;
    Vector points[3]={{0,0},{64,0},{0,64}};
    char tags[3]={1,1,1};short contours[1]={2};
    Outline outline={1,3,points,tags,contours,0};
    Functions funcs={point,point,conic,cubic,0,0};
    struct timespec start,end;int result=0;
    clock_gettime(CLOCK_MONOTONIC,&start);
    for (unsigned i=0;i<iterations;i++) {
        result=FT_Outline_Decompose(&outline,&funcs,NULL);
        if (result) break;
    }
    clock_gettime(CLOCK_MONOTONIC,&end);
    fprintf(stderr,"RESULT mode=%s status=%d hits=%u ns=%lld\n",argv[1],result,hits,
            (long long)(end.tv_sec-start.tv_sec)*1000000000+end.tv_nsec-start.tv_nsec);
    if (failed_prepare ? !(result!=0 && hits==0) :
        guest_error ? !(result==42 && hits==1) : !(result==0 && hits==4*iterations)) return 1;
    puts("PASS: FreeType callback execution/result boundary");
    return 0;
}
