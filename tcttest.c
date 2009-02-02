/*************************************************************************************************
 * The test cases of the table database API
 *                                                      Copyright (C) 2006-2009 Mikio Hirabayashi
 * This file is part of Tokyo Cabinet.
 * Tokyo Cabinet is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.  Tokyo Cabinet is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * You should have received a copy of the GNU Lesser General Public License along with Tokyo
 * Cabinet; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA.
 *************************************************************************************************/


#include <tcutil.h>
#include <tctdb.h>
#include "myconf.h"

#define RECBUFSIZ      48                // buffer for records


/* global variables */
const char *g_progname;                  // program name
int g_dbgfd;                             // debugging output


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void iprintf(const char *format, ...);
static void iputchar(int c);
static void eprint(TCTDB *tdb, const char *func);
static bool iterfunc(const void *kbuf, int ksiz, const void *vbuf, int vsiz, void *op);
static int myrand(int range);
static int runwrite(int argc, char **argv);
static int runread(int argc, char **argv);
static int runremove(int argc, char **argv);
static int runrcat(int argc, char **argv);
static int runmisc(int argc, char **argv);
static int runwicked(int argc, char **argv);
static int procwrite(const char *path, int rnum, int bnum, int apow, int fpow,
                     bool mt, int opts, int rcnum, int lcnum, int ncnum, int xmsiz,
                     int iflags, int omode, bool rnd);
static int procread(const char *path, bool mt, int rcnum, int lcnum, int ncnum, int xmsiz,
                    int omode, bool rnd);
static int procremove(const char *path, bool mt, int rcnum, int lcnum, int ncnum, int xmsiz,
                      int omode, bool rnd);
static int procrcat(const char *path, int rnum, int bnum, int apow, int fpow,
                    bool mt, int opts, int rcnum, int lcnum, int ncnum, int xmsiz,
                    int iflags, int omode, int pnum, bool dai, bool dad, bool rl, bool ru);
static int procmisc(const char *path, int rnum, bool mt, int opts, int omode);
static int procwicked(const char *path, int rnum, bool mt, int opts, int omode);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  g_dbgfd = -1;
  const char *ebuf = getenv("TCDBGFD");
  if(ebuf) g_dbgfd = tcatoix(ebuf);
  srand((unsigned int)(tctime() * 1000) % UINT_MAX);
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "write")){
    rv = runwrite(argc, argv);
  } else if(!strcmp(argv[1], "read")){
    rv = runread(argc, argv);
  } else if(!strcmp(argv[1], "remove")){
    rv = runremove(argc, argv);
  } else if(!strcmp(argv[1], "rcat")){
    rv = runrcat(argc, argv);
  } else if(!strcmp(argv[1], "misc")){
    rv = runmisc(argc, argv);
  } else if(!strcmp(argv[1], "wicked")){
    rv = runwicked(argc, argv);
  } else {
    usage();
  }
  return rv;
}


/* print the usage and exit */
static void usage(void){
  fprintf(stderr, "%s: test cases of the table database API of Tokyo Cabinet\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write [-mt] [-tl] [-td|-tb|-tt|-tx] [-rc num] [-lc num] [-nc num]"
          " [-xm num] [-ip] [-is] [-in] [-it] [-if] [-nl|-nb] [-rnd]"
          " path rnum [bnum [apow [fpow]]]\n", g_progname);
  fprintf(stderr, "  %s read [-mt] [-rc num] [-lc num] [-nc num] [-xm num] [-nl|-nb] [-rnd]"
          " path\n", g_progname);
  fprintf(stderr, "  %s remove [-mt] [-rc num] [-lc num] [-nc num] [-xm num] [-nl|-nb] [-rnd]"
          " path\n", g_progname);
  fprintf(stderr, "  %s rcat [-mt] [-tl] [-td|-tb|-tt|-tx] [-rc num] [-lc num] [-nc num]"
          " [-xm num] [-ip] [-is] [-in] [-it] [-if] [-nl|-nb] [-pn num] [-dai|-dad|-rl|-ru]"
          " path rnum [bnum [apow [fpow]]]\n",
          g_progname);
  fprintf(stderr, "  %s misc [-mt] [-tl] [-td|-tb|-tt|-tx] [-nl|-nb] path rnum\n", g_progname);
  fprintf(stderr, "  %s wicked [-mt] [-tl] [-td|-tb|-tt|-tx] [-nl|-nb] path rnum\n", g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* print formatted information string and flush the buffer */
static void iprintf(const char *format, ...){
  va_list ap;
  va_start(ap, format);
  vprintf(format, ap);
  fflush(stdout);
  va_end(ap);
}


/* print a character and flush the buffer */
static void iputchar(int c){
  putchar(c);
  fflush(stdout);
}


/* print error message of table database */
static void eprint(TCTDB *tdb, const char *func){
  const char *path = tctdbpath(tdb);
  int ecode = tctdbecode(tdb);
  fprintf(stderr, "%s: %s: %s: error: %d: %s\n",
          g_progname, path ? path : "-", func, ecode, tctdberrmsg(ecode));
}


/* iterator function */
static bool iterfunc(const void *kbuf, int ksiz, const void *vbuf, int vsiz, void *op){
  unsigned int sum = 0;
  while(--ksiz >= 0){
    sum += ((char *)kbuf)[ksiz];
  }
  while(--vsiz >= 0){
    sum += ((char *)vbuf)[vsiz];
  }
  return myrand(100 + (sum & 0xff)) > 0;
}


/* get a random number */
static int myrand(int range){
  return (int)((double)range * rand() / (RAND_MAX + 1.0));
}


/* parse arguments of write command */
static int runwrite(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  bool mt = false;
  int opts = 0;
  int rcnum = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int iflags = 0;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-tl")){
        opts |= TDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= TDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= TDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= TDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= TDBTEXCODEC;
      } else if(!strcmp(argv[i], "-rc")){
        if(++i >= argc) usage();
        rcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-lc")){
        if(++i >= argc) usage();
        lcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nc")){
        if(++i >= argc) usage();
        ncnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-ip")){
        iflags |= 1 << 0;
      } else if(!strcmp(argv[i], "-is")){
        iflags |= 1 << 1;
      } else if(!strcmp(argv[i], "-in")){
        iflags |= 1 << 2;
      } else if(!strcmp(argv[i], "-it")){
        iflags |= 1 << 3;
      } else if(!strcmp(argv[i], "-if")){
        iflags |= 1 << 4;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else if(!bstr){
      bstr = argv[i];
    } else if(!astr){
      astr = argv[i];
    } else if(!fstr){
      fstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procwrite(path, rnum, bnum, apow, fpow, mt, opts, rcnum, lcnum, ncnum, xmsiz,
                     iflags, omode, rnd);
  return rv;
}


/* parse arguments of read command */
static int runread(int argc, char **argv){
  char *path = NULL;
  bool mt = false;
  int rcnum = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-rc")){
        if(++i >= argc) usage();
        rcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-lc")){
        if(++i >= argc) usage();
        lcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nc")){
        if(++i >= argc) usage();
        ncnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  if(!path) usage();
  int rv = procread(path, mt, rcnum, lcnum, ncnum, xmsiz, omode, rnd);
  return rv;
}


/* parse arguments of remove command */
static int runremove(int argc, char **argv){
  char *path = NULL;
  bool mt = false;
  int rcnum = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-rc")){
        if(++i >= argc) usage();
        rcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-lc")){
        if(++i >= argc) usage();
        lcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nc")){
        if(++i >= argc) usage();
        ncnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  if(!path) usage();
  int rv = procremove(path, mt, rcnum, lcnum, ncnum, xmsiz, omode, rnd);
  return rv;
}


/* parse arguments of rcat command */
static int runrcat(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  bool mt = false;
  int opts = 0;
  int rcnum = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int iflags = 0;
  int omode = 0;
  int pnum = 0;
  bool dai = false;
  bool dad = false;
  bool rl = false;
  bool ru = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-tl")){
        opts |= TDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= TDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= TDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= TDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= TDBTEXCODEC;
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-rc")){
        if(++i >= argc) usage();
        rcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-lc")){
        if(++i >= argc) usage();
        lcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nc")){
        if(++i >= argc) usage();
        ncnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-ip")){
        iflags |= 1 << 0;
      } else if(!strcmp(argv[i], "-is")){
        iflags |= 1 << 1;
      } else if(!strcmp(argv[i], "-in")){
        iflags |= 1 << 2;
      } else if(!strcmp(argv[i], "-it")){
        iflags |= 1 << 3;
      } else if(!strcmp(argv[i], "-if")){
        iflags |= 1 << 4;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-pn")){
        if(++i >= argc) usage();
        pnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-dai")){
        dai = true;
      } else if(!strcmp(argv[i], "-dad")){
        dad = true;
      } else if(!strcmp(argv[i], "-rl")){
        rl = true;
      } else if(!strcmp(argv[i], "-ru")){
        ru = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else if(!bstr){
      bstr = argv[i];
    } else if(!astr){
      astr = argv[i];
    } else if(!fstr){
      fstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procrcat(path, rnum, bnum, apow, fpow, mt, opts, rcnum, lcnum, ncnum, xmsiz,
                    iflags, omode, pnum, dai, dad, rl, ru);
  return rv;
}


/* parse arguments of misc command */
static int runmisc(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  bool mt = false;
  int opts = 0;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-tl")){
        opts |= TDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= TDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= TDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= TDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= TDBTEXCODEC;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int rv = procmisc(path, rnum, mt, opts, omode);
  return rv;
}


/* parse arguments of wicked command */
static int runwicked(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  bool mt = false;
  int opts = 0;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-tl")){
        opts |= TDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= TDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= TDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= TDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= TDBTEXCODEC;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int rv = procwicked(path, rnum, mt, opts, omode);
  return rv;
}


/* perform write command */
static int procwrite(const char *path, int rnum, int bnum, int apow, int fpow,
                     bool mt, int opts, int rcnum, int lcnum, int ncnum, int xmsiz,
                     int iflags, int omode, bool rnd){
  iprintf("<Writing Test>\n  path=%s  rnum=%d  bnum=%d  apow=%d  fpow=%d  mt=%d"
          "  opts=%d  rcnum=%d  lcnum=%d  ncnum=%d  xmsiz=%d  iflags=%d  omode=%d  rnd=%d\n\n",
          path, rnum, bnum, apow, fpow, mt, opts, rcnum, lcnum, ncnum, xmsiz,
          iflags, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
    eprint(tdb, "tctdbsetmutex");
    err = true;
  }
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(tdb, "tctdbsetcodecfunc");
    err = true;
  }
  if(!tctdbtune(tdb, bnum, apow, fpow, opts)){
    eprint(tdb, "tctdbtune");
    err = true;
  }
  if(!tctdbsetcache(tdb, rcnum, lcnum, ncnum)){
    eprint(tdb, "tctdbsetcache");
    err = true;
  }
  if(xmsiz >= 0 && !tctdbsetxmsiz(tdb, xmsiz)){
    eprint(tdb, "tctdbsetxmsiz");
    err = true;
  }
  if(!rnd) omode |= TDBOTRUNC;
  if(!tctdbopen(tdb, path, TDBOWRITER | TDBOCREAT | omode)){
    eprint(tdb, "tctdbopen");
    err = true;
  }
  if((iflags & (1 << 0)) && !tctdbsetindex(tdb, "", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 1)) && !tctdbsetindex(tdb, "str", TDBITLEXICAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 2)) && !tctdbsetindex(tdb, "num", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 3)) && !tctdbsetindex(tdb, "type", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 4)) && !tctdbsetindex(tdb, "flag", TDBITLEXICAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    int id = rnd ? myrand(rnum) + 1 : (int)tctdbgenuid(tdb);
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", id);
    TCMAP *cols = tcmapnew2(7);
    char vbuf[RECBUFSIZ*5];
    int vsiz = sprintf(vbuf, "%d", id);
    tcmapput(cols, "str", 3, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%d", myrand(i) + 1);
    tcmapput(cols, "num", 3, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%d", myrand(32) + 1);
    tcmapput(cols, "type", 4, vbuf, vsiz);
    int num = myrand(5);
    int pt = 0;
    char *wp = vbuf;
    for(int j = 0; j < num; j++){
      pt += myrand(5) + 1;
      if(wp > vbuf) *(wp++) = ',';
      wp += sprintf(wp, "%d", pt);
    }
    *wp = '\0';
    if(*vbuf != '\0') tcmapput(cols, "flag", 4, vbuf, wp - vbuf);
    if(!tctdbput(tdb, pkbuf, pksiz, cols)){
      eprint(tdb, "tctdbput");
      err = true;
      break;
    }
    tcmapdel(cols);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  iprintf("size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  if(!tctdbclose(tdb)){
    eprint(tdb, "tctdbclose");
    err = true;
  }
  tctdbdel(tdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform read command */
static int procread(const char *path, bool mt, int rcnum, int lcnum, int ncnum, int xmsiz,
                    int omode, bool rnd){
  iprintf("<Reading Test>\n  path=%s  mt=%d  rcnum=%d  lcnum=%d  ncnum=%d  xmsiz=%d"
          "  omode=%d  rnd=%d\n\n", path, mt, rcnum, lcnum, ncnum, xmsiz, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
    eprint(tdb, "tctdbsetmutex");
    err = true;
  }
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(tdb, "tctdbsetcodecfunc");
    err = true;
  }
  if(!tctdbsetcache(tdb, rcnum, lcnum, ncnum)){
    eprint(tdb, "tctdbsetcache");
    err = true;
  }
  if(xmsiz >= 0 && !tctdbsetxmsiz(tdb, xmsiz)){
    eprint(tdb, "tctdbsetxmsiz");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOREADER | omode)){
    eprint(tdb, "tctdbopen");
    err = true;
  }
  int rnum = tctdbrnum(tdb);
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", rnd ? myrand(rnum) + 1 : i);
    TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
    if(cols){
      tcmapdel(cols);
    } else if(!rnd || tctdbecode(tdb) != TCENOREC){
      eprint(tdb, "tctdbget");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  iprintf("size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  if(!tctdbclose(tdb)){
    eprint(tdb, "tctdbclose");
    err = true;
  }
  tctdbdel(tdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform remove command */
static int procremove(const char *path, bool mt, int rcnum, int lcnum, int ncnum, int xmsiz,
                      int omode, bool rnd){
  iprintf("<Removing Test>\n  path=%s  mt=%d  rcnum=%d  lcnum=%d  ncnum=%d  xmsiz=%d"
          "  omode=%d  rnd=%d\n\n", path, mt, rcnum, lcnum, ncnum, xmsiz, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
    eprint(tdb, "tctdbsetmutex");
    err = true;
  }
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(tdb, "tctdbsetcodecfunc");
    err = true;
  }
  if(!tctdbsetcache(tdb, rcnum, lcnum, ncnum)){
    eprint(tdb, "tctdbsetcache");
    err = true;
  }
  if(xmsiz >= 0 && !tctdbsetxmsiz(tdb, xmsiz)){
    eprint(tdb, "tctdbsetxmsiz");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOWRITER | omode)){
    eprint(tdb, "tctdbopen");
    err = true;
  }
  int rnum = tctdbrnum(tdb);
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", rnd ? myrand(rnum) + 1 : i);
    if(!tctdbout(tdb, pkbuf, pksiz) && !(rnd && tctdbecode(tdb) == TCENOREC)){
      eprint(tdb, "tctdbout");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  iprintf("size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  if(!tctdbclose(tdb)){
    eprint(tdb, "tctdbclose");
    err = true;
  }
  tctdbdel(tdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform rcat command */
static int procrcat(const char *path, int rnum, int bnum, int apow, int fpow,
                    bool mt, int opts, int rcnum, int lcnum, int ncnum, int xmsiz,
                    int iflags, int omode, int pnum, bool dai, bool dad, bool rl, bool ru){
  iprintf("<Random Concatenating Test>\n"
          "  path=%s  rnum=%d  bnum=%d  apow=%d  fpow=%d  mt=%d  opts=%d"
          "  rcnum=%d  lcnum=%d  ncnum=%d  xmsiz=%d  iflags=%d"
          "  omode=%d  pnum=%d  dai=%d  dad=%d  rl=%d  ru=%d\n\n",
          path, rnum, bnum, apow, fpow, mt, opts, rcnum, lcnum, rcnum, xmsiz,
          iflags, omode, pnum, dai, dad, rl, ru);
  if(pnum < 1) pnum = rnum;
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
    eprint(tdb, "tctdbsetmutex");
    err = true;
  }
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(tdb, "tctdbsetcodecfunc");
    err = true;
  }
  if(!tctdbtune(tdb, bnum, apow, fpow, opts)){
    eprint(tdb, "tctdbtune");
    err = true;
  }
  if(!tctdbsetcache(tdb, rcnum, lcnum, ncnum)){
    eprint(tdb, "tctdbsetcache");
    err = true;
  }
  if(xmsiz >= 0 && !tctdbsetxmsiz(tdb, xmsiz)){
    eprint(tdb, "tctdbsetxmsiz");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOWRITER | TDBOCREAT | TDBOTRUNC | omode)){
    eprint(tdb, "tctdbopen");
    err = true;
  }
  if((iflags & (1 << 0)) && !tctdbsetindex(tdb, "", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 1)) && !tctdbsetindex(tdb, "str", TDBITLEXICAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 2)) && !tctdbsetindex(tdb, "num", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 3)) && !tctdbsetindex(tdb, "type", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 4)) && !tctdbsetindex(tdb, "flag", TDBITLEXICAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    int id = myrand(pnum) + 1;
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", id);
    TCMAP *cols = tcmapnew2(7);
    char vbuf[RECBUFSIZ*5];
    int vsiz = sprintf(vbuf, "%d", id);
    tcmapput(cols, "str", 3, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%d", myrand(i) + 1);
    tcmapput(cols, "num", 3, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%d", myrand(32) + 1);
    tcmapput(cols, "type", 4, vbuf, vsiz);
    int num = myrand(5);
    int pt = 0;
    char *wp = vbuf;
    for(int j = 0; j < num; j++){
      pt += myrand(5) + 1;
      if(wp > vbuf) *(wp++) = ',';
      wp += sprintf(wp, "%d", pt);
    }
    *wp = '\0';
    if(*vbuf != '\0') tcmapput(cols, "flag", 4, vbuf, wp - vbuf);
    char nbuf[RECBUFSIZ];
    int nsiz = sprintf(nbuf, "c%d", myrand(pnum) + 1);
    vsiz = sprintf(vbuf, "%d", myrand(rnum) + 1);
    tcmapput(cols, nbuf, nsiz, vbuf, vsiz);
    if(ru){
      switch(myrand(7)){
      case 0:
        if(!tctdbput(tdb, pkbuf, pksiz, cols)){
          eprint(tdb, "tctdbput");
          err = true;
        }
        break;
      case 1:
        if(!tctdbputkeep(tdb, pkbuf, pksiz, cols) && tctdbecode(tdb) != TCEKEEP){
          eprint(tdb, "tctdbputkeep");
          err = true;
        }
        break;
      case 2:
        if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
          eprint(tdb, "tctdbout");
          err = true;
        }
        break;
      case 3:
        if(tctdbaddint(tdb, pkbuf, pksiz, 1) == INT_MIN && tctdbecode(tdb) != TCEKEEP){
          eprint(tdb, "tctdbaddint");
          err = true;
        }
        break;
      case 4:
        if(isnan(tctdbadddouble(tdb, pkbuf, pksiz, 1.0)) && tctdbecode(tdb) != TCEKEEP){
          eprint(tdb, "tctdbadddouble");
          err = true;
        }
        break;
      default:
        if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
          eprint(tdb, "tctdbputcat");
          err = true;
        }
        break;
      }
      if(err) break;
    } else {
      if(dai){
        if(tctdbaddint(tdb, pkbuf, pksiz, myrand(3)) == INT_MIN){
          eprint(tdb, "tctdbaddint");
          err = true;
          break;
        }
      } else if(dad){
        if(isnan(tctdbadddouble(tdb, pkbuf, pksiz, myrand(30) / 10.0))){
          eprint(tdb, "tctdbadddouble");
          err = true;
          break;
        }
      } else if(rl){
        char fbuf[PATH_MAX];
        int fsiz = myrand(PATH_MAX);
        for(int j = 0; j < fsiz; j++){
          fbuf[j] = myrand(0x100);
        }
        tcmapput(cols, "bin", 3, fbuf, fsiz);
        if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
          eprint(tdb, "tctdbputcat");
          err = true;
          break;
        }
      } else {
        if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
          eprint(tdb, "tctdbputcat");
          err = true;
          break;
        }
      }
    }
    tcmapdel(cols);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  iprintf("size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  if(!tctdbclose(tdb)){
    eprint(tdb, "tctdbclose");
    err = true;
  }
  tctdbdel(tdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform misc command */
static int procmisc(const char *path, int rnum, bool mt, int opts, int omode){
  iprintf("<Miscellaneous Test>\n  path=%s  rnum=%d  mt=%d  opts=%d  omode=%d\n\n",
          path, rnum, mt, opts, omode);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
    eprint(tdb, "tctdbsetmutex");
    err = true;
  }
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(tdb, "tctdbsetcodecfunc");
    err = true;
  }
  if(!tctdbtune(tdb, rnum / 50, 2, -1, opts)){
    eprint(tdb, "tctdbtune");
    err = true;
  }
  if(!tctdbsetcache(tdb, rnum / 10, 128, 256)){
    eprint(tdb, "tctdbsetcache");
    err = true;
  }
  if(!tctdbsetxmsiz(tdb, rnum * sizeof(int))){
    eprint(tdb, "tctdbsetxmsiz");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOWRITER | TDBOCREAT | TDBOTRUNC | omode)){
    eprint(tdb, "tctdbopen");
    err = true;
  }
  if(!tctdbsetindex(tdb, "", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "str", TDBITLEXICAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "num", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "type", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "flag", TDBITLEXICAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  iprintf("writing:\n");
  for(int i = 1; i <= rnum; i++){
    int id = (int)tctdbgenuid(tdb);
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", id);
    TCMAP *cols = tcmapnew2(7);
    char vbuf[RECBUFSIZ*5];
    int vsiz = sprintf(vbuf, "%d", id);
    tcmapput(cols, "str", 3, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%d", myrand(i) + 1);
    tcmapput(cols, "num", 3, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%d", myrand(32) + 1);
    tcmapput(cols, "type", 4, vbuf, vsiz);
    int num = myrand(5);
    int pt = 0;
    char *wp = vbuf;
    for(int j = 0; j < num; j++){
      pt += myrand(5) + 1;
      if(wp > vbuf) *(wp++) = ',';
      wp += sprintf(wp, "%d", pt);
    }
    *wp = '\0';
    if(*vbuf != '\0') tcmapput(cols, "flag", 4, vbuf, wp - vbuf);
    char nbuf[RECBUFSIZ];
    int nsiz = sprintf(nbuf, "c%d", myrand(32) + 1);
    vsiz = sprintf(vbuf, "%d", myrand(32) + 1);
    tcmapput(cols, nbuf, nsiz, vbuf, vsiz);
    if(i % 3 == 0){
      if(!tctdbputkeep(tdb, pkbuf, pksiz, cols)){
        eprint(tdb, "tctdbputkeep");
        err = true;
        break;
      }
    } else {
      if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
        eprint(tdb, "tctdbputcat");
        err = true;
        break;
      }
    }
    tcmapdel(cols);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("reading:\n");
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", i);
    TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
    if(cols){
      const char *vbuf = tcmapget2(cols, "str");
      if(!vbuf || strcmp(vbuf, pkbuf)){
        eprint(tdb, "(validation)");
        tcmapdel(cols);
        err = true;
        break;
      }
      tcmapdel(cols);
    } else {
      eprint(tdb, "tctdbget");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("erasing:\n");
  for(int i = 1; i <= rnum; i++){
    if(i % 2 == 1){
      char pkbuf[RECBUFSIZ];
      int pksiz = sprintf(pkbuf, "%d", i);
      if(!tctdbout(tdb, pkbuf, pksiz)){
        eprint(tdb, "tctdbout");
        err = true;
        break;
      }
      if(tctdbout(tdb, pkbuf, pksiz) || tctdbecode(tdb) != TCENOREC){
        eprint(tdb, "tctdbout");
        err = true;
        break;
      }
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(tctdbrnum(tdb) != rnum / 2){
    eprint(tdb, "tctdbrnum");
    err = true;
  }
  if(tctdbuidseed(tdb) != rnum){
    eprint(tdb, "tctdbuidseed");
    err = true;
  }
  iprintf("searching:\n");
  TDBQRY *qry = tctdbqrynew(tdb);
  TCLIST *res = tctdbqrysearch(qry);
  if(tclistnum(res) != rnum / 2){
    eprint(tdb, "tctdbqrysearch");
    err = true;
  }
  tclistdel(res);
  tctdbqrydel(qry);
  const char *names[] = { "", "str", "num", "type", "flag", "c1" };
  int ops[] = { TDBQCSTREQ, TDBQCSTRINC, TDBQCSTRBW, TDBQCSTREW, TDBQCSTRAND, TDBQCSTROR,
                TDBQCSTROREQ, TDBQCSTRRX, TDBQCNUMEQ, TDBQCNUMGT, TDBQCNUMGE, TDBQCNUMLT,
                TDBQCNUMLE, TDBQCNUMBT, TDBQCNUMOREQ };
  int types[] = { TDBQOSTRASC, TDBQOSTRDESC, TDBQONUMASC, TDBQONUMDESC };
  qry = tctdbqrynew(tdb);
  for(int i = 1; i <= rnum; i++){
    if(myrand(100) != 0){
      tctdbqrydel(qry);
      qry = tctdbqrynew(tdb);
      if(myrand(10) != 0){
        char expr[RECBUFSIZ];
        sprintf(expr, "%d", myrand(i) + 1);
        switch(myrand(6)){
        default:
          tctdbqryaddcond(qry, "str", TDBQCSTREQ, expr);
          break;
        case 1:
          tctdbqryaddcond(qry, "str", TDBQCSTRBW, expr);
          break;
        case 2:
          tctdbqryaddcond(qry, "str", TDBQCSTROREQ, expr);
          break;
        case 3:
          tctdbqryaddcond(qry, "num", TDBQCNUMEQ, expr);
          break;
        case 4:
          tctdbqryaddcond(qry, "num", TDBQCNUMGT, expr);
          break;
        case 5:
          tctdbqryaddcond(qry, "num", TDBQCNUMLT, expr);
          break;
        }
        switch(myrand(5)){
        case 0:
          tctdbqrysetorder(qry, "str", TDBQOSTRASC);
          break;
        case 1:
          tctdbqrysetorder(qry, "str", TDBQOSTRDESC);
          break;
        case 2:
          tctdbqrysetorder(qry, "num", TDBQONUMASC);
          break;
        case 3:
          tctdbqrysetorder(qry, "num", TDBQONUMDESC);
          break;
        }
        tctdbqrysetmax(qry, 10);
      } else {
        int cnum = myrand(4);
        if(cnum < 1 && myrand(5) != 0) cnum = 1;
        for(int j = 0; j < cnum; j++){
          const char *name = names[myrand(sizeof(names) / sizeof(*names))];
          int op = ops[myrand(sizeof(ops) / sizeof(*ops))];
          if(myrand(20) == 0) op |= TDBQCNEGATE;
          if(myrand(20) == 0) op |= TDBQCNOIDX;
          char expr[RECBUFSIZ*3];
          char *wp = expr;
          wp += sprintf(expr, "%d", myrand(i));
          if(myrand(10) == 0) wp += sprintf(wp, ",%d", myrand(i));
          if(myrand(10) == 0) wp += sprintf(wp, ",%d", myrand(i));
          tctdbqryaddcond(qry, name, op, expr);
        }
        if(myrand(3) != 0){
          const char *name = names[myrand(sizeof(names) / sizeof(*names))];
          int type = types[myrand(sizeof(types) / sizeof(*types))];
          tctdbqrysetorder(qry, name, type);
        }
        if(myrand(3) != 0) tctdbqrysetmax(qry, myrand(i));
      }
    }
    TCLIST *res = tctdbqrysearch(qry);
    tclistdel(res);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  tctdbqrydel(qry);
  iprintf("random writing and reopening:\n");
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", myrand(rnum) + 1);
    switch(myrand(4)){
    default:
      if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
        eprint(tdb, "tctdbout");
        err = true;
      }
      break;
    case 1:
      if(!tctdbaddint(tdb, pkbuf, pksiz, 1)){
        eprint(tdb, "tctdbaddint");
        err = true;
      }
      break;
    case 2:
      if(!tctdbadddouble(tdb, pkbuf, pksiz, 1.0)){
        eprint(tdb, "tctdbadddouble");
        err = true;
      }
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tctdbclose(tdb)){
    eprint(tdb, "tctdbclose");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOWRITER | omode)){
    eprint(tdb, "tctdbopen");
    err = true;
  }
  iprintf("checking iterator:\n");
  int inum = 0;
  if(!tctdbiterinit(tdb)){
    eprint(tdb, "tctdbiterinit");
    err = true;
  }
  char *pkbuf;
  int pksiz;
  for(int i = 1; (pkbuf = tctdbiternext(tdb, &pksiz)) != NULL; i++, inum++){
    TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
    if(!cols){
      eprint(tdb, "tctdbget");
      err = true;
      tcfree(pkbuf);
      break;
    }
    tcmapdel(cols);
    tcfree(pkbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rnum > 250) iprintf(" (%08d)\n", inum);
  if(tctdbecode(tdb) != TCENOREC || inum != tctdbrnum(tdb)){
    eprint(tdb, "(validation)");
    err = true;
  }
  iprintf("checking transaction commit:\n");
  if(!tctdbtranbegin(tdb)){
    eprint(tdb, "tctdbtranbegin");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", myrand(rnum));
    switch(myrand(4)){
    default:
      if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
        eprint(tdb, "tctdbout");
        err = true;
      }
      break;
    case 1:
      if(!tctdbaddint(tdb, pkbuf, pksiz, 1)){
        eprint(tdb, "tctdbaddint");
        err = true;
      }
      break;
    case 2:
      if(!tctdbadddouble(tdb, pkbuf, pksiz, 1.0)){
        eprint(tdb, "tctdbadddouble");
        err = true;
      }
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tctdbtrancommit(tdb)){
    eprint(tdb, "tctdbtrancommit");
    err = true;
  }
  iprintf("checking transaction abort:\n");
  uint64_t ornum = tctdbrnum(tdb);
  uint64_t ofsiz = tctdbfsiz(tdb);
  if(!tctdbtranbegin(tdb)){
    eprint(tdb, "tctdbtranbegin");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", myrand(rnum));
    switch(myrand(4)){
    default:
      if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
        eprint(tdb, "tctdbout");
        err = true;
      }
      break;
    case 1:
      if(!tctdbaddint(tdb, pkbuf, pksiz, 1)){
        eprint(tdb, "tctdbaddint");
        err = true;
      }
      break;
    case 2:
      if(!tctdbadddouble(tdb, pkbuf, pksiz, 1.0)){
        eprint(tdb, "tctdbadddouble");
        err = true;
      }
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tctdbtranabort(tdb)){
    eprint(tdb, "tctdbtranabort");
    err = true;
  }
  if(tctdbrnum(tdb) != ornum || tctdbfsiz(tdb) != ofsiz){
    eprint(tdb, "(validation)");
    err = true;
  }
  if(!tctdbvanish(tdb)){
    eprint(tdb, "tctdbvanish");
    err = true;
  }
  if(!tctdbput3(tdb, "mikio", "str\tMIKIO\tbirth\t19780211")){
    eprint(tdb, "tctdbput3");
    err = true;
  }
  if(!tctdbtranbegin(tdb)){
    eprint(tdb, "tctdbtranbegin");
    err = true;
  }
  if(!tctdbput3(tdb, "mikio", "str\tMEKEO\tsex\tmale")){
    eprint(tdb, "tctdbput3");
    err = true;
  }
  for(int i = 0; i < 10; i++){
    char pkbuf[RECBUFSIZ];
    sprintf(pkbuf, "%d", myrand(10) + 1);
    char vbuf[RECBUFSIZ*2];
    sprintf(vbuf, "%d\t%d", myrand(10) + 1, myrand(rnum) + 1);
    if(!tctdbput3(tdb, pkbuf, vbuf)){
      eprint(tdb, "tctdbput");
      err = true;
    }
  }
  if(!tctdbforeach(tdb, iterfunc, NULL)){
    eprint(tdb, "tctdbforeach");
    err = true;
  }
  iprintf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  iprintf("size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  if(!tctdbclose(tdb)){
    eprint(tdb, "tctdbclose");
    err = true;
  }
  tctdbdel(tdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform wicked command */
static int procwicked(const char *path, int rnum, bool mt, int opts, int omode){
  iprintf("<Wicked Writing Test>\n  path=%s  rnum=%d  mt=%d  opts=%d  omode=%d\n\n",
          path, rnum, mt, opts, omode);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
    eprint(tdb, "tctdbsetmutex");
    err = true;
  }
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(tdb, "tctdbsetcodecfunc");
    err = true;
  }
  if(!tctdbtune(tdb, rnum / 50, 2, -1, opts)){
    eprint(tdb, "tctdbtune");
    err = true;
  }
  if(!tctdbsetcache(tdb, rnum / 10, 128, 256)){
    eprint(tdb, "tctdbsetcache");
    err = true;
  }
  if(!tctdbsetxmsiz(tdb, rnum * sizeof(int))){
    eprint(tdb, "tctdbsetxmsiz");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOWRITER | TDBOCREAT | TDBOTRUNC | omode)){
    eprint(tdb, "tctdbopen");
    err = true;
  }
  if(!tctdbsetindex(tdb, "", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "str", TDBITLEXICAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "num", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "type", TDBITDECIMAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "flag", TDBITLEXICAL)){
    eprint(tdb, "tctdbsetindex");
    err = true;
  }
  const char *names[] = { "", "str", "num", "type", "flag", "c1" };
  int ops[] = { TDBQCSTREQ, TDBQCSTRINC, TDBQCSTRBW, TDBQCSTREW, TDBQCSTRAND, TDBQCSTROR,
                TDBQCSTROREQ, TDBQCSTRRX, TDBQCNUMEQ, TDBQCNUMGT, TDBQCNUMGE, TDBQCNUMLT,
                TDBQCNUMLE, TDBQCNUMBT, TDBQCNUMOREQ };
  int types[] = { TDBQOSTRASC, TDBQOSTRDESC, TDBQONUMASC, TDBQONUMDESC };
  for(int i = 1; i <= rnum; i++){
    int id = myrand(2) == 0 ? myrand(rnum) + 1 : (int)tctdbgenuid(tdb);
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", id);
    TCMAP *cols = tcmapnew2(7);
    char vbuf[RECBUFSIZ*5];
    int vsiz = sprintf(vbuf, "%d", id);
    tcmapput(cols, "str", 3, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%d", myrand(i) + 1);
    tcmapput(cols, "num", 3, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%d", myrand(32) + 1);
    tcmapput(cols, "type", 4, vbuf, vsiz);
    int num = myrand(5);
    int pt = 0;
    char *wp = vbuf;
    for(int j = 0; j < num; j++){
      pt += myrand(5) + 1;
      if(wp > vbuf) *(wp++) = ',';
      wp += sprintf(wp, "%d", pt);
    }
    *wp = '\0';
    if(*vbuf != '\0') tcmapput(cols, "flag", 4, vbuf, wp - vbuf);
    char nbuf[RECBUFSIZ];
    int nsiz = sprintf(nbuf, "c%d", myrand(i) + 1);
    vsiz = sprintf(vbuf, "%d", myrand(i) + 1);
    tcmapput(cols, nbuf, nsiz, vbuf, vsiz);
    char *cbuf;
    int csiz;
    TCMAP *ncols;
    TDBQRY *qry;
    TCLIST *res;
    switch(myrand(17)){
    case 0:
      iputchar('0');
      if(!tctdbput(tdb, pkbuf, pksiz, cols)){
        eprint(tdb, "tctdbput");
        err = true;
      }
      break;
    case 1:
      iputchar('1');
      cbuf = tcstrjoin4(cols, &csiz);
      if(!tctdbput2(tdb, pkbuf, pksiz, cbuf, csiz)){
        eprint(tdb, "tctdbput2");
        err = true;
      }
      tcfree(cbuf);
      break;
    case 2:
      iputchar('2');
      cbuf = tcstrjoin3(cols, '\t');
      if(!tctdbput3(tdb, pkbuf, cbuf)){
        eprint(tdb, "tctdbput3");
        err = true;
      }
      tcfree(cbuf);
      break;
    case 3:
      iputchar('3');
      if(!tctdbputkeep(tdb, pkbuf, pksiz, cols) && tctdbecode(tdb) != TCEKEEP){
        eprint(tdb, "tctdbputkeep");
        err = true;
      }
      break;
    case 4:
      iputchar('4');
      cbuf = tcstrjoin4(cols, &csiz);
      if(!tctdbputkeep2(tdb, pkbuf, pksiz, cbuf, csiz) && tctdbecode(tdb) != TCEKEEP){
        eprint(tdb, "tctdbputkeep2");
        err = true;
      }
      tcfree(cbuf);
      break;
    case 5:
      iputchar('5');
      cbuf = tcstrjoin3(cols, '\t');
      if(!tctdbputkeep3(tdb, pkbuf, cbuf) && tctdbecode(tdb) != TCEKEEP){
        eprint(tdb, "tctdbputkeep3");
        err = true;
      }
      tcfree(cbuf);
      break;
    case 6:
      iputchar('6');
      if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
        eprint(tdb, "tctdbputcat");
        err = true;
      }
      break;
    case 7:
      iputchar('7');
      cbuf = tcstrjoin4(cols, &csiz);
      if(!tctdbputcat2(tdb, pkbuf, pksiz, cbuf, csiz)){
        eprint(tdb, "tctdbputcat2");
        err = true;
      }
      tcfree(cbuf);
      break;
    case 8:
      iputchar('8');
      cbuf = tcstrjoin3(cols, '\t');
      if(!tctdbputcat3(tdb, pkbuf, cbuf)){
        eprint(tdb, "tctdbputcat3");
        err = true;
      }
      tcfree(cbuf);
      break;
    case 9:
      iputchar('9');
      if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
        eprint(tdb, "tctdbout");
        err = true;
      }
      break;
    case 10:
      iputchar('A');
      if(!tctdbout2(tdb, pkbuf) && tctdbecode(tdb) != TCENOREC){
        eprint(tdb, "tctdbout2");
        err = true;
      }
      break;
    case 11:
      iputchar('B');
      ncols = tctdbget(tdb, pkbuf, pksiz);
      if(ncols){
        tcmapdel(ncols);
      } else if(tctdbecode(tdb) != TCENOREC){
        eprint(tdb, "tctdbget");
        err = true;
      }
      break;
    case 12:
      iputchar('C');
      cbuf = tctdbget2(tdb, pkbuf, pksiz, &csiz);
      if(cbuf){
        tcfree(cbuf);
      } else if(tctdbecode(tdb) != TCENOREC){
        eprint(tdb, "tctdbget2");
        err = true;
      }
      break;
    case 13:
      iputchar('D');
      cbuf = tctdbget3(tdb, pkbuf);
      if(cbuf){
        tcfree(cbuf);
      } else if(tctdbecode(tdb) != TCENOREC){
        eprint(tdb, "tctdbget3");
        err = true;
      }
      break;
    case 14:
      iputchar('E');
      if(myrand(rnum / 50) == 0){
        if(!tctdbiterinit(tdb)){
          eprint(tdb, "tctdbiterinit");
          err = true;
        }
      }
      for(int j = myrand(rnum) / 1000 + 1; j >= 0; j--){
        int iksiz;
        char *ikbuf = tctdbiternext(tdb, &iksiz);
        if(ikbuf){
          tcfree(ikbuf);
        } else {
          int ecode = tctdbecode(tdb);
          if(ecode != TCEINVALID && ecode != TCENOREC){
            eprint(tdb, "tctdbiternext");
            err = true;
          }
        }
      }
      break;
    case 15:
      iputchar('F');
      qry = tctdbqrynew(tdb);
      if(myrand(10) != 0){
        char expr[RECBUFSIZ];
        sprintf(expr, "%d", myrand(i) + 1);
        switch(myrand(6)){
        default:
          tctdbqryaddcond(qry, "str", TDBQCSTREQ, expr);
          break;
        case 1:
          tctdbqryaddcond(qry, "str", TDBQCSTRBW, expr);
          break;
        case 2:
          tctdbqryaddcond(qry, "str", TDBQCSTROREQ, expr);
          break;
        case 3:
          tctdbqryaddcond(qry, "num", TDBQCNUMEQ, expr);
          break;
        case 4:
          tctdbqryaddcond(qry, "num", TDBQCNUMGT, expr);
          break;
        case 5:
          tctdbqryaddcond(qry, "num", TDBQCNUMLT, expr);
          break;
        }
        switch(myrand(5)){
        case 0:
          tctdbqrysetorder(qry, "str", TDBQOSTRASC);
          break;
        case 1:
          tctdbqrysetorder(qry, "str", TDBQOSTRDESC);
          break;
        case 2:
          tctdbqrysetorder(qry, "num", TDBQONUMASC);
          break;
        case 3:
          tctdbqrysetorder(qry, "num", TDBQONUMDESC);
          break;
        }
        tctdbqrysetmax(qry, 10);
      } else {
        int cnum = myrand(4);
        if(cnum < 1 && myrand(5) != 0) cnum = 1;
        for(int j = 0; j < cnum; j++){
          const char *name = names[myrand(sizeof(names) / sizeof(*names))];
          int op = ops[myrand(sizeof(ops) / sizeof(*ops))];
          if(myrand(20) == 0) op |= TDBQCNEGATE;
          if(myrand(20) == 0) op |= TDBQCNOIDX;
          char expr[RECBUFSIZ*3];
          char *wp = expr;
          wp += sprintf(expr, "%d", myrand(i));
          if(myrand(10) == 0) wp += sprintf(wp, ",%d", myrand(i));
          if(myrand(10) == 0) wp += sprintf(wp, ",%d", myrand(i));
          tctdbqryaddcond(qry, name, op, expr);
        }
        if(myrand(3) != 0){
          const char *name = names[myrand(sizeof(names) / sizeof(*names))];
          int type = types[myrand(sizeof(types) / sizeof(*types))];
          tctdbqrysetorder(qry, name, type);
        }
        if(myrand(3) != 0) tctdbqrysetmax(qry, myrand(i));
      }
      res = tctdbqrysearch(qry);
      tclistdel(res);
      tctdbqrydel(qry);
      break;
    default:
      iputchar('@');
      if(myrand(10000) == 0) srand((unsigned int)(tctime() * 1000) % UINT_MAX);
      break;
    }
    tcmapdel(cols);
    if(i % 50 == 0) iprintf(" (%08d)\n", i);
    if(i == rnum / 2){
      if(!tctdbclose(tdb)){
        eprint(tdb, "tctdbclose");
        err = true;
      }
      if(!tctdbopen(tdb, path, TDBOWRITER | omode)){
        eprint(tdb, "tctdbopen");
        err = true;
      }
    } else if(i == rnum / 4){
      char *npath = tcsprintf("%s-tmp", path);
      if(!tctdbcopy(tdb, npath)){
        eprint(tdb, "tctdbcopy");
        err = true;
      }
      TCTDB *ntdb = tctdbnew();
      if(!tctdbsetcodecfunc(ntdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
        eprint(ntdb, "tctdbsetcodecfunc");
        err = true;
      }
      if(!tctdbopen(ntdb, npath, TDBOREADER | omode)){
        eprint(ntdb, "tctdbopen");
        err = true;
      }
      tctdbdel(ntdb);
      unlink(npath);
      tcfree(npath);
      if(!tctdboptimize(tdb, rnum / 50, -1, -1, -1)){
        eprint(tdb, "tctdboptimize");
        err = true;
      }
      if(!tctdbiterinit(tdb)){
        eprint(tdb, "tctdbiterinit");
        err = true;
      }
    } else if(i == rnum / 8){
      if(!tctdbtranbegin(tdb)){
        eprint(tdb, "tctdbtranbegin");
        err = true;
      }
    } else if(i == rnum / 8 + rnum / 16){
      if(!tctdbtrancommit(tdb)){
        eprint(tdb, "tctdbtrancommit");
        err = true;
      }
    }
  }
  if(!tctdbsync(tdb)){
    eprint(tdb, "tctdbsync");
    err = true;
  }
  iprintf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  iprintf("size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  if(!tctdbclose(tdb)){
    eprint(tdb, "tctdbclose");
    err = true;
  }
  tctdbdel(tdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}



// END OF FILE