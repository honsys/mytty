#if !defined(__PosixRuntime_h__)
#define __PosixRuntime_h__ "$Name:  $ $Id: PosixRuntime.h,v 0.4 2004/08/16 16:28:28 hon Exp $"
#define __PosixRuntime_H__(arg) const char arg##PosixRuntime_h__rcsId[] = __PosixRuntime_h__;

#if defined(Linux) || defined(LINUX)
#include "ctime" // this is pretty kludgy
// once _POSIX_SOURCE is set these are lost:
//extern int ftruncate(int fd, off_t length);
//extern int nanosleep(const struct timespec *req, struct timespec *rem);
//extern int putenv(const char *string);
//extern int gethostname(char *name, size_t len);
#endif
/*
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif
*/
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199506L
#endif
 
#if defined(vxWorks) || defined(VxWorks) || defined(VXWORKS) || defined(Vx) 
#include "vxWorks.h"
#include "sockLib.h"
#include "taskLib.h"
#include "ioLib.h"
#include "fioLib.h"
#include "msgQLib.h"
#endif // vx

#include "fcntl.h"
#include "float.h"
#include "limits.h"
#include "math.h"
#include "pthread.h"
#include "signal.h"
#include "stdlib.h"
#include "strings.h"
#include "semaphore.h"
#include "unistd.h"
#include "values.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/mman.h"

#if defined(SOLARIS)
//#include "mqueue.h"
#include "values.h"
#include "sys/feature_tests.h"
#include "sys/siginfo.h"
#include "ctime"
#elif defined(Linux) || (LINUX)
#include "values.h"
#include "features.h"
typedef char *caddr_t;
#define __restrict 
#include "time.h"
#include "sys/time.h"
#undef __restrict
#endif

// c++
using namespace std;
#include "cerrno"
#include "iostream"
#include "strstream"
#include "cstdio"
#include "cmath"
#include "vector"
#include "map"
#include "string"
#include "cstring"

class PosixRuntime { 
// encapsulates semaphore, shared memory, signal, thread routines & maybe msgQueus
// this is an abstract class that supports "singleton" sub-classes. 
public:
  static bool _verbose;

  //typedef basic_string< char > string;
  //typedef struct { int fd; int size; string* semName; void* base; } ShmSeg;
  //typedef struct { int fd; int size; string semName; caddr_t base; } ShmSeg;
  // gdb complains about incomplete data type when i examine this, so let's try:
  struct ShmSeg {
    int fd; int size; string semName; caddr_t base;
    inline ShmSeg() : fd(-1), size(0), semName("unkown"), base(0) {}
  };
  struct MMapFile { 
    int fd; int size; string name; unsigned char* buf;
    inline MMapFile() : fd(-1), size(0), name("unkown"), buf(0) {}
  };

  typedef void (*SigHandler)(int);
  typedef void* (*ThreadStart)(void*);
  //typedef void (*SigAction)(int, siginfo_t*, void*);
  //typedef vector< SigHandler > SigHandlerVec;
  typedef std::vector< string > StrVec;
#if defined(Linux) || defined(LINUX) || defined(SOLARIS)
  typedef std::map< string, ShmSeg* > ShmTable;
#endif
  typedef std::map< string, string > NameTable;
  typedef std::map< string, sem_t* > SemTable;
  typedef std::map< string, FILE* > ChildTable;
  typedef std::map< string, pthread_t > ThreadTable;
  //typedef map< string, mqd_t > MsgQTable;

#if defined(vxWorks) || defined(VxWorks) || defined(VXWORKS) || defined(Vx)
  typedef map< string, int > TaskTable;
#endif 

  enum { _MaxInterrupts_= 32 };

  inline static bool threaded() { return _threaded; }
  static string errStr();
 
  // delete contents of all tables, then remove all elements from all tables
  static int clearAndDestroy();

  static sem_t* semAttach(string& name); // sem_open
  static sem_t* semCreate(string& name, short val= 0); // sem_open
  static int semClose(sem_t* sem); // sem_close;
  static int semValue(string& name); // sem_getvalue
  static int semWaitCnt(string& name); // if sem < 0 return abs() else 0
  static int semPost(string& name); // increment +1 and return value
  static int semRelease(string& name); // sem_post until no waiters left -- neg. val increm. to 0
  static int semClear(string& name); // sem_wait until positive value gets decrem. to 0
  static int semTakeNoWait(string& name); // sem_trywait, non-block try to decrement -1
  static int semTake(string& name); // sem_wait, block until decrement -1
  static int semDelete(string& name); // sem_unlink
#if defined(Linux) || defined(LINUX) || defined(SOLARIS)
  // shm_open, mmap
  static ShmSeg* shmAttach(string& name, int size, string& semName); 
  // shm_open, mmap, ftruncate
  static ShmSeg* shmCreate(string& name, int size, string& semName); 
  static int shmDelete(string& name); // shm_unlink
  static int shmLock(string& name); // take sem assoc. with shm name
  static int shmUnlock(string& name); // release sem assoc. with shm name
  static ShmSeg* shmGetSeg(string& name);
#endif
  // file i/o
  static MMapFile mmapFileCreate(const string& file, int sz);
  static MMapFile mmapFileOpenR(const string& file, int sz);
  static MMapFile mmapFileOpenRW(const string& file, int sz);
  static int mmapFileClose(MMapFile& mf);

  static void defaultSigHandler(int sig=0);
  static SigHandler setSignalHandler(SigHandler handler=defaultSigHandler, bool threaded= false);
  static SigHandler setSignalHandler(vector< int >& sigs,
				     SigHandler handler=defaultSigHandler, bool threaded= false);
  inline static SigHandler getSignalHandler() { return _theSigHandler; }
  inline static SigHandler getOldSignalHandler() { return _theOldSigHandler; }
  inline static SigHandler setDefaultSigHandler(bool threaded= false) {
    return setSignalHandler(defaultSigHandler, threaded);
  }
  // pthread stuff:
  static void yield();
  static int getInterrupt();
  static int setInterrupt(int signum);
  // prevent these signals from being delivered to process (all threads):
  static int sigMask(sigset_t* prev = 0);
  static int sigUnMask(sigset_t* prev = 0);
  static int sigRestoreMask(const sigset_t sigset);
  static void* sigWait(void* arg=0);
  // convenience func. creates & runs thread that performs sigwait and
  // calls sig. handler:
  static pthread_t sigWaitThread(SigHandler handler=defaultSigHandler);
  // convenience func. creates & runs thread with startfunc:
  static pthread_t newThread(ThreadStart startfunc, void *arg=0, pthread_attr_t *attr=0);
  static int setMaxPriority(pthread_attr_t *attr, pthread_t thread = 0);
  static int setMinPriority(pthread_attr_t *attr, pthread_t thread = 0);

  // named pipe (fifo) functions...
  static int fifoCreate(string& name);
  static pthread_t fifoReadThread(string& fifoname, ThreadStart startfunc, pthread_attr_t *attr=0); 
  static pthread_t fifoWriteThread(string& fifoname, ThreadStart startfunc, pthread_attr_t *attr=0); 

  // optional msgQ functions...
  static int mqCreate(string& name, int sz);
  static int mqWrite(string& name, char*& msg, int idx=0);
  static int mqRead(string& name, char*& msg, int idx=0);
  static int mqWaitForNew(string& name, char*& msg);

  // default is to sleep until interrupted...
  static int sleep(float sec = -1.0);

public: // globals
#if defined(Linux) || defined(LINUX) || defined(SOLARIS)
  static ShmTable* theShmTable;
#endif
  static SemTable* theSemTable;
  static ChildTable* theChildTable;
  static ThreadTable* theThreadTable;
  //static MsgQTable theMsgQTable; // optional
#if defined(vxWorks) || defined(VxWorks) || defined(VXWORKS) || defined(Vx)
  static TaskTable* theTaskTable;
#endif

protected:
  // setSignalHandler returns "old/previous" handler as it sets the "new/current"
  static SigHandler _theSigHandler;
  static SigHandler _theOldSigHandler;
  static sigset_t _theSigSet;
  // the last/latest interrupt:
  static int _theInterrupt;
  static pthread_mutex_t* _interruptMutex;
  static bool _threaded;

  // probably don't need this
  //static SigHandlerVec* _theSigHandlerList; 

  // no need to ever instance this because all elements are static
  PosixRuntime();
  inline virtual ~PosixRuntime() {}
};

#endif // __PosixRuntime_h__
