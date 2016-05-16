#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "states.h"
#include "sortstate.h"
#include "instream.h"
#include "outstream.h"
#include "modulus.h"
#include "modadd.h"

// ok for width 19 since 3*19 = 57 <= 5*LOCBITS
#define LOCBITS 12

int main(int argc, char *argv[])
{
  int ncpus,cpuid,modidx,width,y,x,nextx,tsizelen;
  uint64_t msize,modulus,nin;
  char c,*tsizearg,inbase[64];
  uint64_t skipunder, nnewillcnt, newstates[3]; 
  int i,nnew,noutfiles;
  statebuf *mb;
  statecnt sn;
  jtset *jts;
  goin *gin;
  goout *go;

  assert(sizeof(uint64_t)==8);
  if (argc!=6) {
    printf("usage: %s width modulo_index y ncpus maxtreesize[kKmMgG]\n",argv[0]);
    exit(1);
  }
  setwidth(width = atoi(argv[1]));
  modidx = atoi(argv[2]);
  if (modidx < 0 || modidx >= NMODULI) {
    printf ("modulo_index %d not in range [0,%d)\n", modidx, NMODULI);
    exit(1);
  }
  modulus = -(uint64_t)modulusdeltas[modidx];
  y = atoi(argv[3]);
  nextx = (x+1) % width;
  ncpus = atoi(argv[4]);
  if (ncpus < 1 || ncpus > MAXCPUS) {
    printf ("#cpus %d not in range [0,%d]\n", ncpus, MAXCPUS);
    exit(1);
  }
  tsizelen = strlen(tsizearg = argv[5]);
  if (!isdigit(c = tsizearg[tsizelen-1]))
    tsizearg[tsizelen-1] = '\0';
   msize = atol(tsizearg);
  if (c == 'k' || c == 'K')
    msize *= 1000LL;
  if (c == 'm' || c == 'M')
    msize *= 1000000LL;
  if (c == 'g' || c == 'G')
    msize *= 1000000000LL;
  if (msize < 1000000LL) {
    printf("memsize %jd too small for comfort.\n", (intmax_t)msize);
    exit(1);
  }
  sprintf(inbase,"%d.%d/yx.%02d.%02d",width,modidx,y,x); 
  gin = openstreams(inbase, incpus, ncpus, cpuid, modulus, skipunder);
  go = goinit(width, modidx, modulus, y+!nextx, nextx, ncpus, cpuid);

  nnewillcnt = nin = 0LL;
  jts = jtalloc(msize, modulus, LOCBITS);
  if (nstreams(gin)) {
    for (; (mb = minstream(gin))->state != FINALSTATE; nin++,deletemin(gin)) {
      sn.cnt = mb->cnt;
      // printf("expanding %jo\n", (uintmax_t)mb->state);
      nnew = expandstate(mb->state, x, newstates);
      for (i=0; i<nnew; i++) {
        sn.state = newstates[i];
        //printf("inserting %jo\n", (uintmax_t)sn.state);
        jtinsert(jts, &sn);
      }
      if (nnew < 3) // nnew == 2
        modadd(modulus, &nnewillcnt, mb->cnt);
      if (jtfull(jts))
        dumpstates(go, jts, noutfiles++, mb->state);
    }
  }
  dumpstates(go, jts, noutfiles++, FINALSTATE);
  jtfree(jts);

  printf("(%d,%d) size %ju xsize %ju mod ",y,x,(uintmax_t)nin,(uintmax_t)totalread(gin));
  if (modulus)
    printf("%ju",(uintmax_t)modulus);
  else printf("18446744073709551616");

  printf("\nnewillegal %ju ", (uintmax_t)nnewillcnt);
  printf(       "needy %ju ", (uintmax_t)needywritten(go));
  printf(       "legal %ju ", (uintmax_t)legalwritten(go));
  printf("at (%d,%d)\n",y+(nextx==0),nextx);

  assert(cpuid != 0 || skipunder != 0L || nin > 0); // crash if cpu0 processes no states

  return 0;
}
