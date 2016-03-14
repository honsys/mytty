#if !defined(__PosixRuntime_cc__)
#define __PosixRuntime_cc__ "$Name:  $ $Id: PosixRuntime.cc 14 2008-06-11 01:49:45Z hon $"
const char rcsId[] = __PosixRuntime_cc__;
#include "PosixRuntime.h"
__PosixRuntime_H__(__PosixRuntime_cc);

#include "PSem.h"
#include "sys/mman.h"

// initialize global objects:
bool PosixRuntime::_verbose = false;
bool PosixRuntime::_threaded = false;
#if defined(LINUX) || defined(SOLARIS)
PosixRuntime::ShmTable* PosixRuntime::theShmTable=0;
#endif
PosixRuntime::SemTable* PosixRuntime::theSemTable=0;
PosixRuntime::ChildTable* PosixRuntime::theChildTable=0;
PosixRuntime::ThreadTable* PosixRuntime::theThreadTable=0;
#if defined(vxWorks) || defined(VxWorks) || defined(VXWORKS) || defined(Vx)
  PosixRuntime::TaskTable* PosixRuntime::theTaskTable=0;
#endif
PosixRuntime::SigHandler PosixRuntime::_theSigHandler = PosixRuntime::defaultSigHandler;
PosixRuntime::SigHandler PosixRuntime::_theOldSigHandler;

int PosixRuntime::_theInterrupt=0;
pthread_mutex_t* PosixRuntime::_interruptMutex= 0;

sigset_t PosixRuntime::_theSigSet;

// probably don't need this
//PosixRuntime::SigHandlerVec* PosixRuntime::_theSigHandlerList=0; 

PosixRuntime::PosixRuntime() {
  if( _interruptMutex == 0 ) {
    _interruptMutex = new pthread_mutex_t;
    ::pthread_mutex_init(_interruptMutex, (const pthread_mutexattr_t *) 0);
  }
  // always set the mask, so all threads inherit this set of BLOCKED signals.
  // (pg 228 of Butenhof), then only one designated thread UNBLOCKS...
  // (upon call to setSigHandler), by application/daemon.
  sigMask();
  /* 
  if( !theShmTable ) theShmTable = new ShmTable();
  if( !theSemTable ) theSemTable = new SemTable();
  if( !theChildTable ) theChildTable = new ChildTable();
  if( !theThreadTable ) theThreadTable = new ThreadTable();
  */
  //static MsgQTable theMsgQTable; // optional
#if defined(vxWorks) || defined(VxWorks) || defined(VXWORKS) || defined(Vx)
  if( !TaskTable ) TaskTable = new TaskTable();
#endif
}

string PosixRuntime::errStr() {
  string r = "";
  char* s = strerror(errno);
  if( s ) {
    r += s;
  }
  else {
    r += "?bad errno set? clear errno";
    errno = 0;
  }
  return r;
}

// posix threads
void PosixRuntime::yield() {
  ::sched_yield();
}

void PosixRuntime::defaultSigHandler(int signum) {
  // do we want to save off this interrupt signum?
  //setInterrupt(signum);
  switch(signum) {
    case SIGABRT: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGABRT"<<endl; break;
    case SIGALRM: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGALRM"<<endl; break;
    case SIGCHLD: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGCHLD"<<endl; break;
    case SIGCONT: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGCONT"<<endl; break;
    case SIGFPE: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGFPE"<<endl; break;
    case SIGHUP: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGHUP"<<endl; break;
    case SIGINT: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGINT"<<endl; exit(0); break;
    case SIGPIPE: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGPIPE"<<endl; break;
    //case SIGPWR: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGPWR"<<endl; break;
    case SIGTERM: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGTERM"<<endl; exit(0); break;
    case SIGURG: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGURG"<<endl; break;
    case SIGUSR1: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGG"<<endl; break;
    case SIGUSR2: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGUSR2"<<endl; break;
    case SIGVTALRM: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGVTALRM"<<endl; break;
    case SIGXFSZ: clog<<"POSIXRuntime: caught signal "<<signum<<" SIGXFSZ"<<endl; break;
    default: clog<<"POSIXRuntime: caught signal "<<signum<<" SIG???"<<endl; break;
  }
}

int PosixRuntime::sigMask(sigset_t* prev_sigset) {
  // prevent these signals from being delivered to process:
  const int sigcnt = 14;
  const int _sigref[sigcnt] = { SIGABRT, SIGALRM, SIGCHLD, SIGCONT, SIGFPE, SIGHUP, SIGINT, SIGPIPE,
                                /*SIGPWR,*/ SIGTERM, SIGURG, SIGUSR1, SIGUSR2, SIGVTALRM, SIGXFSZ };
  int stat = ::sigemptyset(&_theSigSet);
  for( int i = 0; i < sigcnt; i++ )
     stat = ::sigaddset(&_theSigSet, _sigref[i]);

  // set list of (currently) blocked signals to theSigSet
  return ::sigprocmask(SIG_SETMASK, &_theSigSet, prev_sigset);
}

// unblock _theSigSet
int PosixRuntime::sigUnMask(sigset_t* prev_sigset) {
  // set list of (currently) unblocked signals to theSigSet
  return ::sigprocmask(SIG_UNBLOCK, &_theSigSet, prev_sigset);
}

int PosixRuntime::sigRestoreMask(sigset_t sigset) {
  _theSigSet = sigset;
  return ::sigprocmask(SIG_SETMASK, &_theSigSet, 0);
}

PosixRuntime::SigHandler 
PosixRuntime::setSignalHandler(vector< int >& sigs, SigHandler handler, bool threaded) {
  const int sigcnt = 14;
  const int _sigref[sigcnt] = { SIGABRT, SIGALRM, SIGCHLD, SIGCONT, SIGFPE, SIGHUP, SIGINT, SIGPIPE,
                                /*SIGPWR,*/ SIGTERM, SIGURG, SIGUSR1, SIGUSR2, SIGVTALRM, SIGXFSZ };

  if( sigs.size() == 0 ) {
    sigs = vector< int >(sigcnt);
    for(int i=0; i<sigcnt; i++) sigs[i] = _sigref[i];
  }

  // return the prior handler? 
  if( _theSigHandler )
    _theOldSigHandler = _theSigHandler; 
  else // if it's never been set return the only handler:
    _theOldSigHandler = handler;
 
  // set the new one:
  _theSigHandler = handler; 
  if( _verbose )
    clog<<"PosixRuntime::setSignalHandler> set handler to func. ptr: "<<(int*)_theSigHandler<<endl;

  // posix signals 
  sigset_t emptyset;
  ::sigemptyset(&emptyset);
  ::sigemptyset(&_theSigSet);
  for( vector< int >::size_type i = 0; i < sigs.size(); ++i ) {
    ::sigaddset(&_theSigSet, sigs[i]);
  }

  // assuming that sigWait will be used in its own thread,
  // there should be no need to call sigaction?
  if( threaded || _threaded) { 
    if( _verbose )
      clog<<"PosixRuntime::setSignalHandler> threaded app. unblock signals in sigwait thread."<<endl;
    ::pthread_sigmask(SIG_BLOCK, &_theSigSet, 0); // sigwait thread will unblock
    // however, linux does not seem do work this way?
    // and solaris man page says calling sigaction next is ok... (i think)
    //return _theOldSigHandler;
  }
  else {
    if( _verbose )
      clog<<"PosixRuntime::setSignalHandler> unthreaded app. unblock signals here."<<endl;
    sigset_t prev_sigset; ::sigemptyset(&prev_sigset);
    ::sigprocmask(SIG_SETMASK, &_theSigSet, &prev_sigset);
    ::sigprocmask(SIG_UNBLOCK, &_theSigSet, &prev_sigset);
  }

  struct sigaction act, actold;
  act.sa_handler = _theSigHandler;
  //act.sa_sigaction = 0;
  act.sa_flags = SA_NOCLDSTOP; // only POSIX... | SA_RESTART; // | SA_SIGINFO; for SOGRTMIN-MAX
  act.sa_mask = _theSigSet; // block all the signals during handling of any
  //act.sa_mask = emptyset;
  for( vector< int >::size_type i = 0; i < sigs.size(); ++i ) {
    //clog<<"PosixRuntime::setSignalHandler> add handler for sig: "<<sigs[i]<<endl;
    int stat = ::sigaction(sigs[i], &act, &actold);
    if( stat < 0 )
      clog<<"PosixRuntime::setSignalHandler> sigaction failed for "<<sigs[i]<<", "<<errStr()<<endl;

    /* ansi c rather than posix?
    if( ::signal(sigs[i], _theSigHandler) == SIG_ERR ) 
      clog<<"PosixRuntime::setSignalHandler> signal (add handler) failed for "<<sigs[i]<<", "<<errStr()<<endl;
    */
  }

  return _theOldSigHandler;
}

PosixRuntime::SigHandler 
PosixRuntime::setSignalHandler(SigHandler handler, bool threaded) {
  vector< int > sigs;
  return setSignalHandler(sigs, handler, threaded);
}

void* PosixRuntime::sigWait(void* arg) {
  if( arg != 0 ) { // (re)set sighandler if desired
    bool threaded = true; // presumably this would only be used by a threaded application
    setSignalHandler((SigHandler) arg, threaded);
  }
  SigHandler handler = PosixRuntime::getSignalHandler();
  if( _verbose )
    clog<<"PosixRuntime::sigWait> handler func. ptr: "<<(int*)handler<<endl;
 
  int sig, status;
  const sigset_t mask = PosixRuntime::_theSigSet; // accept these delivered signals
  ::pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

  while( true ) {
    if( _verbose )
      clog << "PosixRuntime::sigWait> pid= "<<getpid()<<", threadId= "<<pthread_self()
           <<", waiting for a signal..."<<endl;
    status = ::sigwait(&mask, &sig); // sleep until signal arrives, call handler
    if( _verbose )
      clog << "PosixRuntime::sigWait> pid= "<<getpid()<<", threadId= "<<pthread_self()
           <<", signal: "<<sig<<", passing to handler: "<<(int*)handler<<endl;
    (*handler)(sig);
  }

  return 0;
}

// convenience func. creates & runs thread that performs sigwait and
// calls sig. handler:
pthread_t PosixRuntime::sigWaitThread(SigHandler handler) {
  //if( !theThreadTable ) theThreadTable = new ThreadTable();
  bool threaded = true; // presumably this would only be used by a threaded application
  setSignalHandler(handler, threaded);
  pthread_t threadId = 0;
  int retval = ::pthread_create(&threadId, (const pthread_attr_t *) 0, PosixRuntime::sigWait, 0);
  if( retval != 0 ) {
    clog<<"PosixRuntime::sigWaitThread> unable to create pthread"<<errStr()<<endl;
    return 0;
  }
        
  if( _verbose )
    clog<<"PosixRuntime::sigWaitThread> created sigwait pthread: "<<threadId<<endl;

  yield();
  return threadId;
}
 
// convenience func. creates & runs thread with startfunc:
pthread_t PosixRuntime::newThread(ThreadStart startfunc, void *arg, pthread_attr_t *attp) {
  //if( !theThreadTable ) theThreadTable = new ThreadTable();
  pthread_t threadId = 0;
  pthread_attr_t attr;
  int stat = ::pthread_attr_init(&attr);
  stat = ::pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED);

  if( attp == 0 )
    attp = &attr;
    
  stat = ::pthread_create(&threadId, attp, startfunc, arg);
  if( stat != 0 )
    clog<<"PosixRuntime> unable to create new (posix) thread."<<errStr()<<endl;

  return threadId;
}

// named pipe (fifo) functions, create:
int PosixRuntime::fifoCreate(string& name) {
  // mkfifo will fail if it already exists, so first remove it if necessary
  struct stat fifostat;
  int exists = ::stat(name.c_str(), &fifostat);
  if( exists == 0 ) 
    return 0; // ::remove(name.c_str());
  mode_t m = S_IRWXU | S_IRWXG | S_IRWXO;

  mode_t oldm = ::umask(0);
  int ret = ::mkfifo(name.c_str(), m);
  ::umask(oldm);
  return ret;
}

// named pipe (fifo) start read thread (thread function arg should be ptr to open file desc.):
pthread_t PosixRuntime::fifoReadThread(string& fifoname, ThreadStart startfunc, pthread_attr_t *attr) {
  int fd = ::open(fifoname.c_str(), O_RDONLY);
  if( fd < 0 ) {
    clog<<"PosixRuntime::fifoReadThread> O_RDONLY open failedon "<<fifoname<<errStr()<<endl;
    return 0;
  }
  return newThread(startfunc, (void*) &fd, attr);
}

// named pipe (fifo) start write thread (thread function arg should be ptr to open file desc.):
pthread_t PosixRuntime::fifoWriteThread(string& fifoname, ThreadStart startfunc, pthread_attr_t *attr) {
  int fd = ::open(fifoname.c_str(), O_WRONLY);
  if( fd < 0 ) {
    clog<<"PosixRuntime::fifoReadThread> O_WRONLY open failed on "<<fifoname<<errStr()<<endl;
    return 0;
  }
  return newThread(startfunc, (void*) &fd, attr);
} 

int PosixRuntime::setMaxPriority(pthread_attr_t *attr, pthread_t thread) {
  if( thread == 0 )
    thread = pthread_self();

  ::pthread_attr_init(attr);
  int policy;
  ::pthread_attr_getschedpolicy(attr, &policy);

  struct sched_param param;
  ::pthread_attr_getschedparam(attr, &param);

  param.sched_priority = ::sched_get_priority_max(policy);
  pthread_setschedparam(thread, policy, &param);

  return param.sched_priority;
}
 
int PosixRuntime::setMinPriority(pthread_attr_t *attr, pthread_t thread) {
  if( thread == 0 )
    thread = ::pthread_self();

  ::pthread_attr_init(attr);
  int policy;
  ::pthread_attr_getschedpolicy(attr, &policy);

  struct sched_param param;
  ::pthread_attr_getschedparam(attr, &param);

  param.sched_priority = ::sched_get_priority_min(policy);
  ::pthread_setschedparam(thread, policy, &param);

  return param.sched_priority;
}

int  PosixRuntime::semClose(sem_t* sem) {
  return PSem::close(sem);
}

// open/use existing named semaphore
// possibly created by another process or thread
// if semaphore does not exist, this should return SEM_FAILED
sem_t* PosixRuntime::semAttach(string& name) {
  /*
  if( !theSemTable )
    theSemTable = new SemTable();
  else {
    map<string, sem_t*>::iterator s = theSemTable->find(name);
    if( s != theSemTable->end() ) { // if it exists in table, assume it's attached
      //clog<<"PosixRuntime::semAttach> currently attached to: "<<name<<", val: "<<endl;
      return (*theSemTable)[name];
    }
  }
  */
  // the Posix.4 recommends name start with "/" and have only one "/" total!
  if( name.find_first_of("/") != 0 ) {
    clog<<"PosixRuntime> no initial / in name could be error: "<<name<<endl;
    //name = string("/tmp/") + name;
    //clog<<"PosixRuntime> prepend / to name: "<<name<<endl;
  }
  if( name.find_last_of("/") != name.find_first_of("/") )
    clog<<"PosixRuntime> more than 1 / in name could be error: "<<name<<endl;

  int cnt= 0;
  sem_t* sem = (sem_t*) 0;
  do {
    sem = PSem::open(name, 0);
//} while( (errno == EINTR && ++cnt < _MaxInterrupts_) || errno != ENOENT );
  } while( errno == EINTR && ++cnt < _MaxInterrupts_ );

  if( sem == 0 || sem == SEM_FAILED ) {
    if( _verbose )
      clog<<"PosixRuntime> unable to attach semaphore: "<<name<<errStr()<<endl;
    return SEM_FAILED;
  }
  //theSemTable->insert(SemTable::value_type(name, sem));
  //clog<<"PosixRuntime> attached semaphore: "<<name<<", val: "<<semValue(name)<<endl;
  return sem;
}

// create and set initial value to default of val:
// if sem already exists, attach to it and reset its value
sem_t* PosixRuntime::semCreate(string& name, short val) {
  /*
  if( !theSemTable )
    theSemTable = new SemTable();
  else {
    map<string, sem_t*>::iterator s = theSemTable->find(name);
    if( s != theSemTable->end() ) { // if it exists in table, assume it's attached
      // set to the desired val
      sem_t* sem = (*theSemTable)[name];
      int semval, stat = PSem::getvalue(sem, semval);
      clog<<"PosixRuntime::semCreate> currently attached to: "<<name<<", val: "<<semval<<endl;
      while( semval > val ) {
        // decrement to desired "create" value:
        stat = PSem::trywait(sem);
        stat = PSem::getvalue(sem, semval);
      }
      while( semval < val ) {
        // increment to desired "create" value:
        stat = PSem::post(sem);
        stat = PSem::getvalue(sem, semval);
      }
      return (*theSemTable)[name];
    }
  }
  */
  // the Posix.4 recommends name start with "/" and have only one "/" total!
  if( name.find_first_of("/") != 0 ) {
    clog<<"PosixRuntime> no initial / in name could be error: "<<name<<endl;
    //name = string("/tmp/") + name;
    //clog<<"PosixRuntime> prepend / to name: "<<name<<endl;
  }
  if( name.find_last_of("/") != name.find_first_of("/") )
    clog<<"PosixRuntime> more than 1 / in name could be error: "<<name<<endl;

  sem_t* sem = (sem_t*) 0;
  int cnt=0;
  // first try to create it if it does not exist? -- this seems to cause race conditions
  // let's first try to attach & reset the val, if that fails, then create and set val?
  sem = semAttach(name);
  if( sem != SEM_FAILED && sem != 0 ) {
    // if the attach succeeded, set the value to the desired:
    int semval= 0, cnt = abs(semval - val);
    int stat = PSem::getvalue(sem, semval);
    while( stat >= 0 && semval > val && cnt >= 0 ) {
      // decrement to desired "create" value:
      stat = PSem::trywait(sem);
      stat = PSem::getvalue(sem, semval);
      --cnt;
    }
    while( stat >= 0 && semval < val && cnt >= 0) {
      // increment to desired "create" value:
      stat = PSem::post(sem);
      stat = PSem::getvalue(sem, semval);
      --cnt;
    }
    return sem;
  }
  else if( _verbose ){ // attach failed due to non-existence
    clog<<"PosixRuntime::semCreate> attach failed, need to create: "<<name<<
    name<<errStr()<<endl;
  }

  mode_t mall = 0666; // S_IRWXU | S_IRWXG | S_IRWXO;
  mode_t mt = ::umask(0); //::umask(mall);
  do {
    sem = PSem::open(name, O_CREAT|O_EXCL, mall, val);
//} while( (errno == EINTR && ++cnt < _MaxInterrupts_ ) || errno != EEXIST );
  } while( errno == EINTR && ++cnt < _MaxInterrupts_ );
  mt = ::umask(mt); // restore original umask

  if( sem == SEM_FAILED || sem == 0 ) { // unable to find existing sem or create one
    clog<<"PosixRuntime::Create> unable to create: "<<name
        <<", sem_open error: "<<errStr()<<endl;
  }

  //theSemTable->insert(SemTable::value_type(name, sem));
  if( _verbose ) 
    clog<<"PosixRuntime::semCreate> created semaphore: "<<name<<", initval: "<<semValue(name)<<endl;
  return sem;
}

int PosixRuntime::semValue(string& name) {
  int val = INT_MAX;
  PSem::getvalue(name, val);
  //clog<<"PosixRuntime::semValue> name= "<<name<<" val = "<<val<<endl;
  return val;
}

// wake-up all waiters by posting (incrementing) until get_value returned >= 0
int PosixRuntime::semRelease(string& name) {
  // sem_post
  int val = 0;
  sem_t* sem = semAttach(name);
  if( sem == 0 || sem == SEM_FAILED ) {
    clog<<"PosixRuntime::semRelease> failed "<<name<<endl;
    return 0;
  }
  int stat = PSem::post(sem);
  stat = PSem::getvalue(sem, val);
  while( val < 0 ) {
    stat = PSem::post(sem);
    stat = PSem::getvalue(sem, val);
  }
  semClose(sem);
  //clog<<"PosixRuntime::semValue> name= "<<name<<" val = "<<val<<endl;
  return val;
}

int PosixRuntime::semClear(string& name) {
  // sem_wait
  int val = 0;
  sem_t* sem = semAttach(name);
  if( sem == 0 || sem == SEM_FAILED ) {
    clog<<"PosixRuntime::semClear> failed "<<name<<endl;
    return 0;
  }
  int stat = PSem::getvalue(sem, val);
  if( _verbose )
    clog<<"PosixRuntime::semClear> name= "<<name<<" val = "<<val<<endl;
  while( val > 0 ) {
    stat = PSem::trywait(sem);
    if( stat < 0 ) {
      sleep(0.01); continue;
    }
    else
      stat = PSem::getvalue(sem, val);
  }
  semClose(sem);
  if( _verbose )
    clog<<"PosixRuntime::semClear> name= "<<name<<" val = "<<val<<endl;
  return val;
}

// wake-up one waiter by posting (incrementing) semaphore:
int PosixRuntime::semPost(string& name) {
  // sem_post
  int val = INT_MAX;
  int stat = PSem::post(name);
  stat = PSem::getvalue(name, val);
  return val;
}

int PosixRuntime::semTakeNoWait(string& name) {
  // sem_trywait
  int stat;
  int cnt= 0;
  sem_t* sem = semAttach(name);
  if( sem == 0 || sem == SEM_FAILED ) {
    clog<<"PosixRuntime::semTakeNoWait> failed "<<name<<endl;
    return 0;
  }
  do {
    stat = PSem::trywait(sem);
  } while(stat == -1 && errno == EINTR && ++cnt < _MaxInterrupts_ );
  int val= 0;
  stat = PSem::getvalue(sem, val);
  if( stat < 0 ) {
    clog<<"PosixRuntime::semTakeNoWait> failed "<<name<<" cnt, val = "<<cnt<<", "<<val<<endl;
  }
  semClose(sem);
  return val;
}

int PosixRuntime::semTake(string& name) {
  // blocking sem_wait
  int stat, cnt= 0;
  sem_t* sem = semAttach(name);
  if( sem == 0 || sem == SEM_FAILED ) {
    clog<<"PosixRuntime::semTake> failed "<<name<<endl;
    return 0;
  }
  do {
    stat = PSem::wait(sem);
  } while(stat == -1 && errno == EINTR && ++cnt < _MaxInterrupts_);
  int val= 0;
  PSem::getvalue(sem, val);
  if( stat < 0 ) {
    clog<<"PosixRuntime::semTake> failed "<<name<<" cnt, val = "<<cnt<<", "<<val<<endl;
  }
  semClose(sem);
  return val;
}

int PosixRuntime::semDelete(string& name) {
  // sem_unlink
  if( _verbose )
    clog<<"PosixRuntime::semDelete> attach to and unlink: "<<name<<endl;
  sem_t* sem = semAttach(name);
  if( sem == 0 || sem == SEM_FAILED ) {
    // under some bizare cirsumstance the attach might fail (Solaris)
    // even though the semaphore exists.
    // and since we know where SOLARIS hides this info, we can force
    // the deletion here:
#if defined(SOLARIS)
    string spd = "/var/tmp/.SEMD" + name;
    ::remove(spd.c_str());
    string spl = "/var/tmp/.SEML" + name;
    ::remove(spl.c_str()); 
    //theSemTable->erase(name);
    //return theSemTable->size();
#endif
    return 0;
  }
  //theSemTable->erase(name);
  //return theSemTable->size();
  return PSem::unlink(name);
}

#if defined(LINUX) || defined(SOLARIS)
// open/attach to an existing shared memory segment (created by another process)
PosixRuntime::ShmSeg* PosixRuntime::shmAttach(string& name, int size, string& semName) {
  if( !theShmTable ) theShmTable = new ShmTable();
  // the Posix.4 recommends name start with "/" and have only one "/" total!
  if( name.find_first_of("/") != 0 ) {
    clog<<"PosixRuntime> no initial / in name could be error: "<<name<<endl;
    //name = string("/tmp/") + name;
    //clog<<"PosixRuntime> prepend / to name: "<<name<<endl;
  }
  if( name.find_last_of("/") != name.find_first_of("/") )
    clog<<"PosixRuntime> more than / in name could be error: "<<name<<endl;

  semName = name+"Sem.";
  ShmSeg* sm = new ShmSeg;
  sm->semName = semName;
  sem_t* sem = semAttach(sm->semName);
  if( sem == 0 ) {
    delete sm;
    clog<<"PosixRuntime> no semaphore found for shared mem. seg: "<<name<<endl;
    return 0;
  }
  sm->size = size;
  mode_t m = S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH;
  sm->fd = ::shm_open(name.c_str(), O_RDWR, m);
  if(  sm->fd < 0 ) {
    clog<<"PosixRuntime> unable to attach shared mem. seg: "<<name<<errStr()<<endl;
    delete sm;
    return 0;
  }
  sm->base = static_cast< caddr_t >
	(mmap(0, sm->size, PROT_READ|PROT_WRITE, MAP_SHARED,
		sm->fd, 0));

  theShmTable->insert(ShmTable::value_type(name,sm));

  return sm;
}

PosixRuntime::ShmSeg* PosixRuntime::shmCreate(string& name, int size, string& semName) {
  if( !theShmTable ) theShmTable = new ShmTable();
  // if one already exists, it may be the wrong size, so remove it
  ShmSeg* exists = shmAttach(name, size, semName);
  if( exists )
    shmDelete(name);

  ShmSeg* sm = new PosixRuntime::ShmSeg;
  sm->size = size;
  // be sure to create with rwx for all: 
  mode_t mall = S_IRWXU | S_IRWXG | S_IRWXO;
  mode_t mt = ::umask(0); //::umask(mall);
  sm->fd = ::shm_open(name.c_str(), O_CREAT|O_RDWR|O_TRUNC, mall);
  if( sm->fd < 0 ) {
    clog<<"PosixRuntime> unable to open shared mem. seg: "<<name<<errStr()<<endl;
    delete sm;
    return 0;
  }  
  int status = ::ftruncate(sm->fd, sm->size);
  if( status < 0) {
    clog<<"PosixRuntime> ftruncate failed unable to create shared mem. seg: "<<name<<errStr()<<endl;
    return 0;
  }
  sm->base = static_cast< caddr_t > (mmap(0, sm->size, PROT_READ|PROT_WRITE, MAP_SHARED, sm->fd, 0));
  mt = ::umask(mt); // retore orig. umask
  //theShmTable->insert(ShmTable::value_type(name,sm));

  // create an associated semaphore:
  semName = name + "Sem.";
  sem_t* sem = semCreate(semName);
  if( sem == 0 ) {
    clog<<"PosixRuntime> unable to create semaphore for shared mem. seg: "<<semName<<endl;
    return 0;
  }
  sm->semName = semName;

  clog<<"PosixRuntime> created shared mem. seg: "<<name<<", and sem: "<<semName<<endl;

  return sm;
}

int PosixRuntime::shmDelete(string& name) {
  // munmap, close, shm_unlink  
  ShmSeg* seg = shmGetSeg(name);
  munmap(seg->base, seg->size);
  close(seg->fd); // this can be done after the mmap
  semDelete(seg->semName);
  delete seg;
  theShmTable->erase(name);
  return theShmTable->size();
}

int PosixRuntime::shmLock(string& name) {
  // take sem assoc. with shm
  ShmSeg* seg = shmGetSeg(name);
  sem_t* sem = (*theSemTable)[seg->semName];
  return sem_wait(sem);
}

int PosixRuntime::shmUnlock(string& name) {
  // release sem assoc. with shm
  ShmSeg* seg = shmGetSeg(name);
  sem_t* sem = (*theSemTable)[seg->semName];
  return PSem::post(sem);
}

PosixRuntime::ShmSeg* 
PosixRuntime::shmGetSeg(string& name) {
  return (*theShmTable)[name];
}
#endif

// always create mmap with 0777 mode?
PosixRuntime::MMapFile PosixRuntime::mmapFileCreate(const string& name, int sz) {
  PosixRuntime::MMapFile mf; 
  mf.size = sz;
  mf.name = name;
  //  mode_t mall = S_ISUID | S_ISGID | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH;
  mode_t mall = S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH;
  // be sure to create with rw for all: 
  mode_t mt = ::umask(0); // ::umask(mall);
  mf.fd = ::open(mf.name.c_str(), O_RDWR|O_CREAT|O_TRUNC, mall);
  if( mf.fd < 0 ) {
    clog<<"PosixRuntime::mmapFileOpen> unable to open/creat "<<name<<errStr()<<endl;
    return mf;
  }
  //int stat = fchmod(mf.fd, m);
  /*
  int stat = chmod(mf.name.c_str(), m);
  if( stat < 0 ) {
    clog<<"PosixRuntime::mmapFileOpen> unable to chmod "<<name<<errStr()<<endl;
    ::close(mf.fd);
    mf.fd = -1;
    return mf;
  } 
  */ 
  int stat = ::ftruncate(mf.fd, sz);
  if( stat < 0 ) {
    clog<<"PosixRuntime::mmapFileOpen> unable to ftruncate"<<name<<errStr()<<endl;
    ::close(mf.fd);
    mf.fd = -1;
    mt = ::umask(mt); // restore orig. mask
    return mf;
  }

  mf.buf = (unsigned char*) ::mmap(0, sz, PROT_READ|PROT_WRITE, MAP_SHARED, mf.fd, 0);
  if( mf.buf == 0 || mf.buf == MAP_FAILED ) {
    clog<<"PosixRuntime::mmapFileOpen> unable to mmap to "<<name<<errStr()<<endl;
    ::close(mf.fd);
    mf.fd = -1;
  }

  mt = ::umask(mt); // restore orig. mask
  return mf;
}

// mmap existing file for modification
PosixRuntime::MMapFile PosixRuntime::mmapFileOpenRW(const string& name, int sz) {
  PosixRuntime::MMapFile mf; 
  mf.size = sz;
  mf.name = name;
  mode_t m = S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH;
  mf.fd = ::open(mf.name.c_str(), O_RDWR, m);
  if( mf.fd < 0 ) {
    clog<<"PosixRuntime::mmapFileOpen> unable to open"<<name<<errStr()<<endl;
    return mf;
  }
  mf.buf = (unsigned char*) ::mmap(0, sz, PROT_READ|PROT_WRITE, MAP_SHARED, mf.fd, 0);
  if( mf.buf == 0 || mf.buf == MAP_FAILED ) {
    clog<<"PosixRuntime::mmapFileOpen> unable to mmap to "<<name<<errStr()<<endl;
    ::close(mf.fd);
    mf.fd = -1;
  }
  return mf;
}

// mmap existing file for read-only
PosixRuntime::MMapFile PosixRuntime::mmapFileOpenR(const string& name, int sz) {
  PosixRuntime::MMapFile mf; 
  mf.size = sz;
  mf.name = name;
  mode_t m = S_IRUSR | S_IRGRP | S_IROTH;
  mf.fd = ::open(mf.name.c_str(), O_RDONLY, m);
  if( mf.fd < 0 ) {
    clog<<"PosixRuntime::mmapFileOpen> unable to open"<<name<<errStr()<<endl;
    return mf;
  }
  mf.buf = (unsigned char*) ::mmap(0, sz, PROT_READ, MAP_SHARED, mf.fd, 0);
  if( mf.buf == 0 || mf.buf == MAP_FAILED ) {
    clog<<"PosixRuntime::mmapFileOpen> unable to mmap to "<<name<<errStr()<<endl;
    ::close(mf.fd);
    mf.fd = -1;
  }
  return mf;
}

int PosixRuntime::mmapFileClose(PosixRuntime::MMapFile& mf) {
  int stat = ::munmap((char*)mf.buf, mf.size);
  if( stat < 0 ) {
    clog<<"PosixRuntime::mmapFileOpen> unable to munmap to "<<mf.fd<<errStr()<<endl;
    return mf.fd;
  }
  else {
    ::close(mf.fd);
    mf.fd = 0;
    mf.size = 0;
    mf.name = "";
    mf.buf = 0;
  }
  return mf.fd;
}

int PosixRuntime::getInterrupt() {
  if( _interruptMutex == 0 ) {
    _interruptMutex = new pthread_mutex_t;
    ::pthread_mutex_init(_interruptMutex, (const pthread_mutexattr_t *) 0);
  }
  int stat = pthread_mutex_lock(_interruptMutex);
  int retval = _theInterrupt;
  stat = pthread_mutex_unlock(_interruptMutex);

  return retval;
}

int PosixRuntime::setInterrupt(int signum) {
  if( _interruptMutex == 0 ) {
    _interruptMutex = new pthread_mutex_t;
    ::pthread_mutex_init(_interruptMutex, (const pthread_mutexattr_t *) 0);
  }
  int stat = ::pthread_mutex_lock(_interruptMutex);
  int retval = _theInterrupt;
  _theInterrupt = signum;
  stat = ::pthread_mutex_unlock(_interruptMutex);

  return retval;
}

// sleep until interrupted
int PosixRuntime::sleep( float sleepTime ){
  if( sleepTime < 0 ) sleepTime = MAXFLOAT;

  time_t seconds = static_cast< time_t >( sleepTime ) ; //truncate
  sleepTime -= seconds ;

  // nano seconds of timespec cannot be more than 10e9
  long nsec = static_cast< long >( sleepTime * 1e9 ) ;

  struct timespec timeout = { seconds, nsec } ;

  return nanosleep( &timeout, &timeout );
  /*
  unsigned short int cnt = 0 ;
  do {
    errno = 0 ;
    // This will continually put the remaining
    // time to be waited back into the first argument
    if( 0 == nanosleep( &timeout, &timeout ) )
      break;
  } while( (EINTR == errno) && ++cnt <= 10 );
  */
}

  
#endif // __PosixRuntime_cc__
