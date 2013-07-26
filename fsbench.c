/********************************************************/
/*                                                      */
/*   F I L E - S Y S T E M     B E N C H M A R K S      */
/*                                                      */
/*        Radim Kolar , 2:423/66.111@FidoNet            */
/*       radim_kolar@p111.f66.n423.z2.fido.cz           */
/*           F R E E W A R E                            */
/*                                                      */
/*            version 0.22                              */
/*                                                      */
/* History:                                             */
/* 0.01 - 28/02/1996 - Project started...               */
/* 0.10 - 03/03/1996 - Fully functional FSB kernel      */
/* 0.20 - 09/03/1996 - Config parsing + display         */
/* 0.21 - 21/04/1996 - Emergency exit on SIGINT         */
/* 0.22 - 13/06/1996 - DosForceDelete on Emergency exit */
/*                     updated comments in config file  */
/*                     corrected bug in emergency exit  */
/*                      sometimes fsb don't exit        */
/********************************************************/

#define FSBENCH_V "0.22"

#define INCL_DOSMISC
#define INCL_DOSFILEMGR
#define INCL_BASE
#include <os2.h>
#include <malloc.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <io.h>
#include <signal.h>

/* Global vars. */
#define TEST_FILENAME "test.tmp"
#define TEST_FILESIZE 1024*1024*4
HFILE h=0; /* handle prave otevreneho souboru */
char *fn=NULL;  /* jmeno prave otevreneho souboru */



/* Vytvori testovaci soubor fn o velikosti fs bajtu */
int CreateTestFile(char *fn,unsigned long fs)
{
 APIRET rc;
 HFILE hnd;
 ULONG act;
 void *blok;
 int i;
 blok=malloc(512);
 rc=DosOpen(fn,&hnd,&act,fs,FILE_NORMAL,OPEN_ACTION_CREATE_IF_NEW|
            OPEN_ACTION_REPLACE_IF_EXISTS,OPEN_SHARE_DENYREADWRITE|
            OPEN_ACCESS_READWRITE,NULL);
 h=hnd;           
 for(i=0;i<fs/512;i++)
   DosWrite(hnd,blok,512,&act);
 /* dodatek */
 DosWrite(hnd,blok,fs-(fs/512)*512,&act);
 DosClose(hnd);
 h=0;
 return rc;
}

/* smaze testovaci soubor */
int DeleteTestFile(char *fn)
{
 APIRET rc;
 rc=DosForceDelete(fn);
 return rc;
} 

void sync(void)
{
 DosShutdown(1L);
 DosSleep(5000);
}

/* File-System testing Engine */

/* Cache flagy */
#define C_NONE 1
#define C_THR  2
#define C_FULL 3
/* seq. flagy */
#define S_RND 1
#define S_FOR 2
#define S_BACK 3
/* access flagy */
#define A_READ 1
#define A_WRITE 2
#define A_REWRITE 3

int OpenTF(HFILE *hnd,char *fn,int cache,int seq,int acc)
{
 APIRET rc;
 ULONG act;
 ULONG omode;
 /* acc flahy */
 if (acc==A_READ) omode=OPEN_ACCESS_READONLY;
          else    omode=OPEN_ACCESS_READWRITE;
 /* cache flagy */
 if (cache==C_THR) omode=omode|OPEN_FLAGS_WRITE_THROUGH;
 if (cache==C_NONE) omode=omode|OPEN_FLAGS_NO_CACHE;
 /* seq. flagy */
 if (seq==S_RND) omode=omode|OPEN_FLAGS_RANDOM;
 if (seq==S_FOR) omode=omode|OPEN_FLAGS_SEQUENTIAL;
 if (seq==S_BACK) omode=omode|OPEN_FLAGS_RANDOMSEQUENTIAL;
 
 rc=DosOpen(fn,hnd,&act,0,FILE_NORMAL,OPEN_ACTION_FAIL_IF_NEW|
    OPEN_ACTION_OPEN_IF_EXISTS,omode|OPEN_FLAGS_FAIL_ON_ERROR|
    OPEN_SHARE_DENYREADWRITE,NULL);
 return rc;       
}            

int EngineFT(char *fn, unsigned long fs,unsigned long blocksize,
           unsigned long times,int cache, int seq,int acc,double *etime)
{
 APIRET rc;
 unsigned long loop;
 void *buff;
 ULONG bread;
/* unsigned long times2;*/
 double t1,t2;
 struct timeval tv;
/* times2=times;*/
 if((rc=OpenTF(&h,fn,cache,seq,acc))) return 1;
/* printf("File handle je %ld\n",h);*/
/* printf("%ld Blocks/loop\n",fs/blocksize);*/
/*printf("Blocksize: %ld\n",blocksize);*/
 buff=malloc(blocksize);
 if(!buff) return 5;
 srand(1);
 if(gettimeofday(&tv,NULL)) return 6;
 t1=tv.tv_sec+tv.tv_usec/1000000.0;
/* printf("Start: %f\n",t1);*/
 while (times>0)
 {
  loop=fs/blocksize;
/*  printf("%ld blocks to go\n",times);*/
  if(seq==S_FOR) if(DosSetFilePtr(h,0,FILE_BEGIN,&bread)) { DosClose(h);h=0;free(buff);return 2;}
  if(seq==S_BACK) if(DosSetFilePtr(h,-blocksize,FILE_END,&bread)) { DosClose(h);h=0;free(buff);return 2;}

  while ((times>0)&&(loop>0))
  {
   if (acc==A_READ) if(DosRead(h,buff,blocksize,&bread)) { DosClose(h);h=0;free(buff);return 3;}
   if (acc==A_WRITE) if(DosWrite(h,buff,blocksize,&bread)) { DosClose(h);h=0;free(buff);return 3;}
   if (acc==A_REWRITE)
          {
           if(DosRead(h,buff,blocksize,&bread)) { DosClose(h);h=0;free(buff);return 3;}
           if(DosSetFilePtr(h,-blocksize,FILE_CURRENT,&bread)) { DosClose(h);h=0;free(buff);return 2;}
           if(DosWrite(h,buff,blocksize,&bread)) { DosClose(h);h=0;free(buff);return 3;}
          }
   if(bread<blocksize-2) { DosClose(h);h=0;free(buff);return 4;}
/* printf("%ld.",bread);*/
   loop--;
   times--;
   if(seq==S_BACK) if(DosSetFilePtr(h,-blocksize*2,FILE_CURRENT,&bread)) { DosClose(h);free(buff);return 2;};
   if(seq==S_RND) { 
                   if(DosSetFilePtr(h,blocksize*(rand()/(1+(RAND_MAX/(fs/blocksize)))),FILE_BEGIN,&bread)) 
                     { DosClose(h);h=0;free(buff);return 2;}
/*                 printf("NP: %ld",bread);*/
                  }
  };
 };  
 if(gettimeofday(&tv,NULL)) return 6; 
 t2=tv.tv_sec+tv.tv_usec/1000000.0;
/* printf("Stop : %f\n",t2);*/
 *etime=t2-t1;
/*printf("  time %.2fs - ",t2-t1);*/
/* printf("Precteno %ld K bajtu\n",times2*blocksize/1024L);*/
/* printf("Transfer speed %.1f KB/s\n",((blocksize*times2)/1024L)/(t2-t1));*/
 DosClose(h);
 h=0;
 free(buff);
 return rc;
} 

unsigned long pnumb(const char *n)
{
 int i;
 unsigned long res;
 res=0;
 for (i=0;i<strlen(n);i++)
   {
   if (isdigit(n[i])) {
                        res=res*10+((n[i])-'0');
                        continue;
                      }
   if (toupper(n[i])=='K')
                      {
                       res=res*1024;
                       continue;
                      }
                      
   if (toupper(n[i])=='M')
                      {
                       res=res*1024*1024;
                       continue;
                      }
                                        
   }                       
   return res;
} 
int config(void)
{
  FILE *c;
  char *line;
  char *keyw;
  char *ar,*tmp;
  unsigned long fs;
  int i,j,n,p;
  double etime,timer;
  unsigned long blocksize;
  unsigned long times;
  int cache, seq, acc;

  c=fopen("fsbench.cnf","rt");
  if (c==NULL) return 1;
  if(DosQuerySysInfo(QSV_TIMER_INTERVAL,QSV_TIMER_INTERVAL,&times,4)) return 2;
  timer=(times/10.0)/1000.0;
/*  printf("Timer=%f\n",timer);*/
  line=malloc(2048);
  ar  =malloc(2048);
  keyw=malloc(2048);
  fn=malloc(256);
  tmp=malloc(2048);
  if((!line)||(!ar)||(!keyw)||(!fn)||(!tmp)) { free(line);free(ar);free(keyw);free(fn);
                                free(tmp);
                                fclose(c); return 2;}
  printf("\nAction  Direction   cache    times    blocksize   time   Filesystem speed\n");
  printf("-------------------------------------------------------------------------------\n");
  strcpy(fn,"fsbench.tmp");
  fs=8*1024*1024;
  while ((EOF!=fscanf(c,"%[^\n]%*[\n]",line)))
    {
    if (line[0]==';') continue;
/*    printf("!%s\n",line);*/
    /* predej param */
    strcpy(keyw,"");
    strcpy(ar,"");
    sscanf(line,"%s %[^\n]",keyw,ar);
/*  printf("KW: %s\nARG: %s\n",keyw,ar);*/
    if (!stricmp(keyw,"SIZE")) fs=pnumb(ar);
    if (!stricmp(keyw,"NAME")) strcpy(fn,ar);
    if (!stricmp(keyw,"CREATE")) CreateTestFile(fn,fs);
    if (!stricmp(keyw,"DELETE")) DeleteTestFile(fn);
    if (!stricmp(keyw,"SAY")) printf("\n%s\n",ar);
    if (!stricmp(keyw,"TEST"))
    { 
      j=0;n=1;acc=0;seq=0;cache=0;p=0;
      times=0;blocksize=0;
      /* pridam mezeru na konec */
      ar[strlen(ar)+1]='\0';
      ar[strlen(ar)]=' ';
      for(i=0;i<strlen(ar);i++)
        {  
          if (ar[i]!=' ') {tmp[j++]=ar[i];p=0;continue;}
          if (p==1) continue;
          tmp[j]='\0';j=0;
          /* ted'ka to zpracuju */
          switch(n)
          {
           case 1: blocksize=pnumb(tmp);break;
           case 2: times=pnumb(tmp);break;
           case 3: if (tmp[0]=='R') acc=A_READ;
                   if (tmp[0]=='W') acc=A_WRITE;
                   if (tmp[0]=='E') acc=A_REWRITE;
                   break;
           case 4: if (tmp[0]=='F') seq=S_FOR;
                   if (tmp[0]=='B') seq=S_BACK;
                   if (tmp[0]=='R') seq=S_RND;
                   break;
           case 5: if (tmp[0]=='F') cache=C_FULL;
                   if (tmp[0]=='N') cache=C_NONE;
                   if (tmp[0]=='W') cache=C_THR;
                   break;
          }
          n++;p=1;
        }  
       if ((!acc)||(!seq)||(!cache)||(!blocksize)||(!times)) continue;
       sync();
       /* vytiskni co delas */
       switch(acc)
       {
       case A_READ   : printf("Read     ");break;
       case A_WRITE  : printf("Write    ");break;
       case A_REWRITE: printf("Rewrite  ");
       }
       switch(seq)
       {
       case S_FOR : printf("Forward  ");break;
       case S_BACK: printf("Backward ");break;
       case S_RND:  printf("Random   ");
       }
       switch(cache)
       {
       case C_FULL:  printf("use cache ");break;
       case C_NONE:  printf("no cache  ");break;
       case C_THR:   printf("w through ");
       }
       printf("%6ldx ",times);
       printf("%5ld bytes ",blocksize);
       j=EngineFT(fn,fs,blocksize,times,cache,seq,acc,&etime);
       if(j) printf ("ERROR(%d)\n",j);
        else
        { 
         printf("%6.2fs ",etime);
         printf("(%6.0f ",(blocksize*times/1024)/etime);
         /* pocitani chyby mereni v KB */
         printf("+/- %5.1f) KB/s\n",(((blocksize*times/1024)/(etime-timer))-
                ((blocksize*times/1024)/(etime+timer)))/2);
        } 
    }
    if(EOF==(i=getc(c))) break;
                  else 
                    ungetc(i,c);

    
/*  if (eof(c)) break;*/
    }
/*printf("FSIZE=%ld\n",fs);  */
  free(line);free(ar);free(keyw);free(fn);free(tmp);
  fclose(c);
  return 0;
}

void Uklid(int i)
{
 printf("\nEmergency exit..\n");
 if (h) { DosClose(h);}
 DosForceDelete(fn);
 exit(255);
/* signal(i,SIG_ACK);*/
}

void main(void)
{
 if(DosSetPriority(PRTYS_PROCESS,PRTYC_FOREGROUNDSERVER,PRTYD_MAXIMUM,0)) 
  printf("Error setting priority\n");
 if(SIG_ERR==signal(SIGINT,Uklid)) printf("Error catching signal SIGINT\n"); 
 printf("File system benchmark program, version "FSBENCH_V"\n");
 printf("Compiled "__DATE__" using GCC "__VERSION__"\n");
 config();
 return;
}
