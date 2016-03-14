#if !defined(__PSem_cc__)
#define __PSem_cc__ "$Name:  $ $Id: PSem.cc 14 2008-06-11 01:49:45Z hon $"
const char rcsId[] = __PSem_cc__;

#include "PSem.h"
__PSem_H__(__PSem_cc);

union semun { int val; struct semid_ds *buf;
	      unsigned short* array; struct seminfo* __buf; };

#include "sys/types.h"
#include "sys/ipc.h"
#include "sys/sem.h"
#include "stdlib.h"

bool PSem::_verbose = false;

sem_t* PSem::open(const string& name, int flag, mode_t mode, int val) {
  int ret= 0;
  ret = ::umask(0);
  //mode_t md = S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH;
  sem_t* sem = 0;
#if !defined(SYSVSEM)
  if( _verbose )
    clog<<"PSem::open> name: "<<name<<", flag:"<<flag<<endl;
  if( flag == 0 ) { // attach
    sem = ::sem_open(name.c_str(), 0);
  }
  else { // create & attach
    sem = ::sem_open(name.c_str(), flag, mode, val);
    /*
    if( sem != 0 && sem != SEM_FAILED ) {
      ::sem_post(sem); // test increment
      ::sem_trywait(sem); // test decrement
      ::sem_close(sem);
      sem = ::sem_open(name.c_str(), 0);
    }
    */
  }
#else
// implement these via simple file system logic under "/usr/tmp/ufpsem/name.val"
  string semfile = "/usr/tmp/.PSem";
  if( name.find("/") != 0 ) semfile += "/";
  ret = ::mkdir(semfile.c_str(), 0777);
  semfile += name;

  sem = new sem_t; memset(sem, 0, sizeof(sem_t));
#if defined(LINUX) // creat file for ftok
  //sem->__sem_lock.__status = ::open(semfile.c_str(), O_CREAT, mode); // store fd here
  sem->__sem_lock.__status = ::creat(semfile.c_str(), mode); // store fd here
  if( sem->__sem_lock.__status > 0 && sem->__sem_lock.__status < FOPEN_MAX )
    ret = ::close(sem->__sem_lock.__status);
#else // creat file for ftok (test Linux logic on Solaris)
  //sem->sem_type = ::open(semfile.c_str(), O_CREAT, mode); // store fd here
  sem->sem_type = ::creat(semfile.c_str(), mode); // store fd here
  if( sem->sem_type > 0 && sem->sem_type < FOPEN_MAX )
    ret = ::close(sem->sem_type);
#endif
  //mode_t md = S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH;
  key_t key = ftok(semfile.c_str(), 0);
  
  if( flag == O_CREAT ) {
    flag = IPC_CREAT|mode;
  }
  else if( flag == (O_CREAT|O_EXCL) ) {
    flag = IPC_CREAT|IPC_EXCL|mode;
  }
  else { // attach only
    flag = mode;
  }
  ret = ::semget(key, 1, flag);
  if( ret < 0 ) {
    if( _verbose )
      clog<<"PSem::open> semget failed: "<<strerror(errno)<<endl;
    delete sem; sem = 0;
    return sem;
  }
#if defined(LINUX)
  sem->__sem_value = ret; // store SYSV sem Id in value field
#else
  sem->sem_count = ret; // store SYSV sem Id in value field
#endif
  if( flag != (int)mode ) { // this is a create, need to init. value
    union semun arg; arg.val = val;
#if defined(LINUX)
    ret = ::semctl(sem->__sem_value, 0, SETVAL, arg); 
#else
    ret = ::semctl(sem->sem_count, 0, SETVAL, arg); 
#endif
    if( ret < 0 ) {
      if( _verbose )
	clog<<"PSem::open> semctl failed: "<<strerror(errno)<<endl;
      delete sem; sem = 0;
      return sem;
    }
  }
#endif
  if( _verbose ) {
    //ret = getvalue(sem, val);
    clog<<"PSem::open>"<<name<<", val= "<<val<<endl;
  }
  return sem;
}

// close descriptor for named semaphore SEM.
int PSem::close(sem_t*& sem) {
  int ret= 0;
  if( sem == 0 || sem == SEM_FAILED )
    return ret;

#if !defined(SYSVSEM)
  ret = ::sem_close(sem);
#else
#if defined(LINUX)
  if( sem->__sem_lock.__status > 0 && sem->__sem_lock.__status < FOPEN_MAX )
    ret = ::close(sem->__sem_lock.__status);
#else
  if( sem->sem_type > 0 && sem->sem_type < FOPEN_MAX )
    ret = ::close(sem->sem_type);
#endif
  delete sem;
#endif

  sem = 0;
  return ret;
}

// remove named semaphore.
int PSem::unlink(const string& name) {
  int ret= -1;
#if defined(LINUX) || defined(SOLARIS)
#if !defined(SYSVSEM)
  ret = ::sem_unlink(name.c_str());
#else
  sem_t* sem = PSem::open(name, 0);
  if( sem == 0 || sem == SEM_FAILED )
    return ret;

  semid_ds seminfo;
  union semun s; s.buf = &seminfo;
#if defined(LINUX)
  ret = ::semctl(sem->__sem_value, 0, IPC_STAT, s); 
  ret = ::semctl(sem->__sem_value, 0, IPC_RMID, s); 
#else
  ret = ::semctl(sem->sem_count, 0, IPC_STAT, s); 
  ret = ::semctl(sem->sem_count, 0, IPC_RMID, s); 
#endif
  delete sem;
#endif
#endif
  return ret;
}

// post (increment) SEM. 
int PSem::post(sem_t* sem) { // increment
  int ret= -1;
  if( sem == 0 || sem == SEM_FAILED )
    return ret;

#if !defined(SYSVSEM)
  ret = ::sem_post(sem);
#else
  union semun arg;
  ret = getvalue(sem, arg.val);
  if( ret < 0 )
   return ret;
  arg.val++;
#if defined(LINUX)
  ret = ::semctl(sem->__sem_value, 0, SETVAL, arg); // set incremented value
#else
  ret = ::semctl(sem->sem_count, 0, SETVAL, arg); // set incremented value
#endif
#endif
  if( _verbose ) {
    int val= -1;
    ret = getvalue(sem, val);
    clog<<"PSem::post> incremented: "<<val<<endl;
  }

  return ret;
}

// wait for SEM being posted.
int PSem::wait(sem_t* sem) { // decrement
  int ret= -1;
  if( sem == 0 || sem == SEM_FAILED )
    return ret;

#if !defined(SYSVSEM)
  ret = ::sem_wait(sem);
#else
  union semun arg;
  ret = getvalue(sem, arg.val);
  if( ret < 0 )
   return ret;
  arg.val--;
  if( arg.val <= 0 ) arg.val = 0;
#if defined(LINUX)
  ret = ::semctl(sem->__sem_value, 0, SETVAL, arg); // set decremented value
#else
  ret = ::semctl(sem->sem_count, 0, SETVAL, arg); // set incremented value
#endif
#endif
  if( _verbose ) {
    int val= -1;
    ret = getvalue(sem, val);
    clog<<"PSem::wait> decremented: "<<val<<endl;
  }

  return ret;
}

// Test whether SEM is posted.
int PSem::trywait(sem_t* sem) {
  int ret= -1;
  if( sem == 0 || sem == SEM_FAILED )
    return ret;

#if !defined(SYSVSEM)
  ret = ::sem_trywait(sem);
#else
  union semun arg;
  ret = getvalue(sem, arg.val);
  if( ret < 0 )
   return ret;
  arg.val--;
  if( arg.val <= 0 ) arg.val = 0;
#if defined(LINUX)
  ret = ::semctl(sem->__sem_value, 0, SETVAL, arg); // set decremented value
#else
  ret = ::semctl(sem->sem_count, 0, SETVAL, arg); // set incremented value
#endif
#endif
  if( _verbose ) {
    int val= -1;
    ret = getvalue(sem, val);
    clog<<"PSem::trywait> decremented: "<<val<<endl;
  }

  return ret;
}

// get current value of SEM and store it in VAL. 
int PSem::getvalue(sem_t* sem, int& val) {
  int ret= -1;
  if( sem == 0 || sem == SEM_FAILED )
    return ret;

#if !defined(SYSVSEM)
  if( _verbose )
    clog<<"PSem::getvalue> sem: "<<(size_t) sem<<ends;
  ret = ::sem_getvalue(sem, &val);
  if( _verbose )
    clog<<"PSem::getvalue> sem: "<<(size_t) sem<<", val: "<<val<<endl;
#else
  union semun arg;
  arg.val = 0;
#if defined(LINUX)
  ret = ::semctl(sem->__sem_value, 0, GETVAL, arg); // -1 is return if this fails!
#else
  ret = ::semctl(sem->sem_count, 0, GETVAL, arg); // set incremented value
#endif
  if( ret < 0 )
    clog<<"PSem::getvalue> failed to get semvalue, "<<strerror(errno)<<endl;
  val = ret; // this is clearly a problem; negative values are not allowed for SYS5 sems.
#endif

  return ret;
}


// convenience funcs. use SEM name:
// post SEM. 
int PSem::post(const string& name) { // increment
  sem_t* sem = PSem::open(name, 0);
  if( sem == 0 || sem == SEM_FAILED )
    return -1;
  if( _verbose ) clog<<"("<<name<<") "<<ends;
  int ret = PSem::post(sem);
  PSem::close(sem);
  return ret;
}  

// wait for SEM being posted.
int PSem::wait(const string& name) { // decrement
  sem_t* sem =  PSem::open(name, 0);
  if( sem == 0 || sem == SEM_FAILED )
    return -1;
  if( _verbose ) clog<<"("<<name<<") "<<ends;
  int ret = PSem::wait(sem);
  PSem::close(sem);
  return ret;
}  

// test whether SEM is posted.
int PSem::trywait(const string& name) {
  sem_t* sem =  PSem::open(name, 0);
  if( sem == 0 || sem == SEM_FAILED )
    return -1;
  if( _verbose ) clog<<"("<<name<<") "<<ends;
  int ret = PSem::trywait(sem);
  PSem::close(sem);
  return ret;
}  

// get current value of SEM and store it in VAL. 
int PSem::getvalue(const string& name, int& val) {
  sem_t* sem =  PSem::open(name, 0);
  if( sem == 0 || sem == SEM_FAILED )
    return -1;
  if( _verbose ) clog<<"("<<name<<") "<<ends;
  int ret = PSem::getvalue(sem, val);
  PSem::close(sem);
  return ret;
}  

// get current value of SEM and store it in VAL. 
int PSem::setvalue(const string& name, int val) {
  if( val < 0 )
    return -1;

  sem_t* sem =  PSem::open(name, 0);
  if( sem == 0 || sem == SEM_FAILED )
    return -1;
  if( _verbose ) clog<<"("<<name<<") "<<ends;
  int semval, ret = PSem::getvalue(sem, semval);
  int cnt= 16;
  while( cnt > 0 && semval < val ) {
    ret = post(sem);
    if( ret < 0 ) {
      --cnt;
      PosixRuntime::sleep(0.1);
    }
    ret = PSem::getvalue(sem, semval);
  }
  while( cnt > 0 && semval > val ) {
    ret = trywait(sem);
    if( ret < 0 ) {
      --cnt;
      PosixRuntime::sleep(0.1);
    }
    ret = PSem::getvalue(sem, semval);
  }
  PSem::close(sem);
  return ret;
}  

int PSem::main(int argc, char** argv, char** envp) {
  string semname = "/testPSem";
  string usage = "ufpsem [-h[elp]] [-d[elete]] [[-cnt] +/-n] (no args does get, +/-n increments or decrement by n) [-n[ame] (use name interface)";

  bool done = false;
  int val= INT_MAX;
  // preserv old cmd-line option: (ufpsem [+/-]val)
  if( argc > 1 && (isdigit(argv[1][0]) || isdigit(argv[1][1])) ) // [+/-] val
    val = atoi(argv[1]);

  Runtime::Argv args;
  Runtime::argVec(argc, argv,args);
  string arg = Runtime::argVal("-h", args);
  if( arg != "false" ) {
    clog<<usage<<endl;
    return 0;
  }
  arg = Runtime::argVal("-help", args);
  if( arg != "false" ) {
    clog<<usage<<endl;
    return 0;
  }

  arg = Runtime::argVal("-v", args);
  if( arg != "false" ) {
    PSem::_verbose = true;
  }

  arg = Runtime::argVal("-d", args);
  if( arg != "false" ) {
    return PosixRuntime::semDelete(semname);
  }

  arg = Runtime::argVal("-cnt", args);
  if( arg != "false" ) {
    val = atoi(arg.c_str());
    clog<<"val: "<<arg<<" = "<<val<<endl;
  }

  string host = Runtime::hostname();
  // try attach, create if necessary
  sem_t* sem = PosixRuntime::semAttach(semname);
  if( val == INT_MAX && (sem == 0 || sem == SEM_FAILED) ) { // this is a getvalue (only) request that 
    // fails if sem does not request
    clog<<"unable to attach: "<<semname<<endl;
    return 1;
  }
  if( val != INT_MAX && (sem == 0 || sem == SEM_FAILED) ) {
    // only create it if this is a set request and it does not exist
    sem = PosixRuntime::semCreate(semname, abs(val));
    if( val >= 0 ) done = true;
  }
  if( sem == 0 || sem == SEM_FAILED) {
    clog<<"unable to attach or create: "<<semname<<endl;
    return 1;
  }
  PSem::close(sem);
  // ok sem exists, continue test:

  int semval= -1;
  int ret = PSem::getvalue(semname, semval);
  if( ret < 0 ) {
    clog<<"unable to getvalue of "<<host<<":"<<semname<<endl;
    return 1;
  }
  clog<<host<<":"<<semname<<"= "<<semval<<endl;
  if( done || val == INT_MAX ) { // got current value, no new value requested...
    return 0;
  }

  arg = Runtime::argVal("-n", args);
  if( arg != "false" ) { // use the name interface..
    if( val > 0 ) {
      for( int i = 0; i < val; ++i ) {
        ret = PSem::post(semname);
        if( ret < 0 ) {
          clog<<"unable to post: "<<semname<<endl;
          return 2;
        }
        ret = PSem::getvalue(semname, semval);
        if( ret < 0 ) {
          clog<<"unable to getvalue of: "<<semname<<endl;
          return 3;
        }
        clog<<semname<<": "<<semval<<endl;
        PosixRuntime::sleep(0.01);
      }
      return 0;
    }
    else if( val < 0 ) {
      for( int i = val; i < 0; ++i ) {
        ret = PSem::wait(semname);
        if( ret < 0 ) {
          clog<<"unable to wait: "<<semname<<endl;
          return 2;
        }
        ret = PSem::getvalue(semname, semval);
        if( ret < 0 ) {
          clog<<"unable to getvalue of: "<<semname<<endl;
          return 3;
        }
        clog<<semname<<": "<<semval<<endl;
        PosixRuntime::sleep(0.01);
      }
      return 0;
    }
  }

  // if we get here, assume sem has been created:
  sem = PSem::open(semname, 0);
  if( val > 0 ) {
    for( int i = 0; i < val; ++i ) {
      ret = PSem::post(sem);
      if( ret < 0 ) {
        clog<<"unable to post: "<<semname<<endl;
        return 2;
      }
      ret = PSem::getvalue(sem, semval);
      if( ret < 0 ) {
        clog<<"unable to getvalue of: "<<semname<<endl;
        return 3;
      }
      clog<<semname<<": "<<semval<<endl;
      PosixRuntime::sleep(0.01);
    }
  }
  else if( val < 0 ) {
    for( int i = val; i < 0; ++i ) {
      ret = PSem::wait(sem);
      if( ret < 0 ) {
        clog<<"unable to wait: "<<semname<<endl;
        return 2;
      }
      ret = PSem::getvalue(sem, semval);
      if( ret < 0 ) {
        clog<<"unable to getvalue of: "<<semname<<endl;
        return 3;
      }
      clog<<semname<<": "<<semval<<endl;
      PosixRuntime::sleep(0.01);
    }
  }

  return PSem::close(sem);
}

#endif // __PSem_cc__
