#if !defined(__Runtime_h__)
#define __Runtime_h__ "$Name:  $ $Id: Runtime.h,v 0.19 2006/09/19 20:41:17 hon Exp $"
#define __Runtime_H__(arg) const char arg##Runtime_h__rcsId[] = __Runtime_h__;

#include "PosixRuntime.h"
//#include "Sys5Runtime.h"
#include "vector"

//class Daemon;

//class Runtime : public PosixRuntime, public Sys5Runtime {
class Runtime : public PosixRuntime {
public:
  struct Argv : public vector< string > { 
    inline Argv() {}
    inline Argv(int argc, char**argv) {
      for( int i = 0; i < argc; ++i )
        push_back(argv[i]);
    }
  };
  inline Runtime() {}
  inline ~Runtime() {}

  inline static bool leapYear(int y) { return ((y%4 == 0 && y%100 != 0) || y%400 == 0); }

  static int checkLockFile(const string& file);

  static int upperCase(string& s);
  static int lowerCase(string& s);

  static string concat(const string& s, int i) 
    { strstream st; st<<s<<i<<ends; string ss = st.str(); delete st.str(); return ss; }

  static string concat(const string& s, double d) 
    { strstream st; st<<s<<d<<ends; string ss = st.str(); delete st.str(); return ss; }
  
  static string pathToSelf(const string& self);
  static string cwdPath();

  // exec an external application (fork & exec):
  // provide full path & all options in one string (like popen)
  static pid_t execApp(const string& appcmd, char** envp= 0);

  // primarily for use by top-level executive
  // fork then run proc->exec()
  // static pid_t createChild(Daemon* d, void* p= 0); 

  // sub-shell child can send stdout to parent via half duplex pipe:
  static FILE* pipeReadChild(const char *command); // popen(command, "r")
  static string pipeRead(FILE* fp); // popen(command, "r")

  // sub-shell child can read stdin and send stdout to parent full duplex via pipe:
  // evidently full duplex pipes are not yet supported on linux
  // so this actually manages two half duplex pipes (thus supporting all flavors of unix)
  static pid_t pipeReadWriteChild(const string& command, int& tochild, int& fromchild);

  // although linux fails to fdopen a FILE* from a pipe fd...
  static pid_t pipeReadWriteChild(const string& command, FILE*& tochild, FILE*& fromchild);

  // support send & recv on pipe:
  static int psend(int pipeFd, const string& ss);
  static int precv(int pipeFd, string& rs);

  static int parseWords(const string& text, vector< string >& words, const string& delim= " ");
  static int parseWords(const string& text, char**& cwords, const string& delim= " ");

  static string hostname();
  static string currentDate(const string& zone="utc");
  static string currentTime(const string& zone="utc");
  static string adjTime(const string& time, float delta, const string& zone="utc");
  
  static int directory(vector< string >& list, const char* dnam = "./");
#if defined(Linux) || defined(LINUX) || defined(SOLARIS)
  static void sysinfo(vector< string > * infovec= 0); // print system info.
#endif
  static bool littleEndian(); // returns true if system is Little-Endian

  // milli-second resolution sleep based on select, arg is float units
  // in seconds (like the socket timeout arg), useful if nanosleep not 
  // available:
  static void mlsleep(float sec = 0.001);

  // return FD_ISSET of write select of fd (0 or yes):
  static int writable(int fd, float timeOut= 1.0);
  static int writable(FILE* fp, float timeOut= 1.0);

  // number of bytes available to read from pipe or socket fd
  static int available(int fd, float wait= 0.01, int trycnt= 1);
  // number of bytes available to read from named pipe/fifo
  static int available(string fifoname, float wait= 0.01, int trycnt= 1);

  static void initSysLog();
  static void writeSysLog();
  static void alarmPopUp();
  static int bell(int rings= 2);

  virtual void initialize(int argc, char** argv);
  // deprecated (use argVec/Val()):
  virtual int cmdLine(int argc, char** argv);
  virtual int cmdOpt(char* opt, int argc, char** argv);

  // provide runtime parameter args as vec
  static int rmJunk(char* cs, int ntot = -1);
  static int rmJunk(string& s);
  static int argVec(string& options, Runtime::Argv& args);
  static int argVec(int argc, char** argv, Runtime::Argv& args);

  // return value if arg has been provided
  // return "true" if arg is found but has no value
  // return "false" otherwise:
  static string argVal(const char* arg, const Runtime::Argv& args);

  //helper funcs to convert a number to string:
  static string numToStr( double number );
  static string numToStr( int number );
  static string numToStr( time_t number );
};

#endif // __Runtime_h__

