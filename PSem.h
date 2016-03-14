#if !defined(__PSem_h__)
#define __PSem_h__ "$Name:  $ $Id: PSem.h,v 0.1 2004/09/17 19:19:12 hon Exp $"
#define __PSem_H__(arg) const char arg##PSem_h__rcsId[] = __PSem_h__;

#include "Runtime.h"

class PSem {
public:
  static bool _verbose;

  // unit test: 
  static int main(int argc, char** argv, char** envp);

  // implement these via simple file system logic under "/tmp/ufpsem/name.val"
  static sem_t* open(const string& name, int flag, mode_t mode= 0666, int val= 0);

  // remove named semaphore NAME.
  static int unlink(const string& name);

  // close descriptor for named semaphore SEM.
  static int close(sem_t*& sem);

  // post SEM. 
  static int post(sem_t* sem); // increment

  // wait for SEM being posted.
  static int wait(sem_t* sem); // decrement

  // test whether SEM is posted.
  static int trywait(sem_t* sem);

  // get current value of SEM and store it in VAL. 
  static int getvalue(sem_t* sem, int& val);

  // convenience funcs. use name:
  // post SEM. 
  static int post(const string& name); // increment

  // wait for SEM being posted.
  static int wait(const string& name); // decrement

  // test whether SEM is posted.
  static int trywait(const string& name);

  // get current value of SEM and store it in VAL. 
  static int getvalue(const string& name, int& val);
  // set current value of SEM. 
  static int setvalue(const string& name, int val);
};

#endif //  __PSem_h__
