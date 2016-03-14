#if !defined(__Runtime_cc__)
#define __Runtime_cc__ "$Name:  $ $Id: Runtime.cc 14 2008-06-11 01:49:45Z hon $"
const char rcsId[] = __Runtime_cc__;

#include "Runtime.h"
__Runtime_H__(__Runtime_cc);

//#include "Daemon.h"
//__Daemon_H__(__Runtime_cc);
 
#if defined(vxWorks) || defined(VxWorks) || defined(VXWORKS) || defined(Vx)
#include "vxWorks.h"
#include "sockLib.h"
#include "taskLib.h"
#include "ioLib.h"
#include "fioLib.h"
#endif // vx
// sys incs
#if defined(LINUX)
#include "sys/ioctl.h" // ioctl
#include "asm/ioctls.h" // FIONREAD
#include "sys/sysinfo.h"
#define __restrict 
#include "sys/time.h"
#undef __restrict
#endif
#if defined(SOLARIS)
#include "sys/filio.h" // FIONREAD
#include "sys/systeminfo.h"
#include "sys/time.h"
#endif
#if defined(CYGWIN)
#include "fileio.h"
#include "time.h"
#include "sys/select.h"
#endif

#include "ctype.h"
#include "unistd.h"
#include "dirent.h"
#include "fcntl.h"
#include "sys/types.h"
#include "sys/stat.h"

#ifndef MAXNAMLEN
#define MAXNAMLEN 255
#endif

// dso_handle mystery of x86_64 link (unresolved ref. in stdc++ iostream):
#if defined(LINUX)
/* hon -- /lib64/libpthread.so appears to have this synbole defined...
extern "C" { void* __dso_handle = 0; }
*/
// another suggested solution  -- link via: -Wl,-init,"_ufinit"
extern "C" { void _ufinit(void) { std::cout<<"Runtime::_ufinit(void)..."<<std::endl; } }
#endif

int Runtime::checkLockFile(const string& lockfile) {
  // uf lock file symantics: lock is true (>0) if empty file exists
  // lock is false (==0) if file does not exist (stat errno == ENOENT)
  // lock is possibly unkown, but assumed false
  if( lockfile.empty() )
    return -1;

  struct stat st; memset(&st, 0, sizeof(struct stat));
  int s = ::stat(lockfile.c_str(), &st);
  if( s == 0 )
    return 1;
  if( errno == ENOENT )
    return 0;

  return -1;
}
  
int Runtime::upperCase(string& s) {
  char* cs = (char*) s.c_str();
  char* tmp = cs;
  for( size_t i = 0; i < s.length(); ++i, ++tmp )
    *tmp = ::toupper(*tmp);
 
  s = cs;
  return s.length();
}

int Runtime::lowerCase(string& s) {
  char* cs = (char*) s.c_str();
  char* tmp = cs;
  for( size_t i = 0; i < s.length(); ++i, ++tmp )
    *tmp = ::tolower(*tmp);
 
  s = cs;
  return s.length();
}

// helper func. removes any extraneous junk like '\r', '\n', '\t', ... 
int Runtime::rmJunk(char* cstr, int ntot) { 
  int slen = strlen(cstr);
  if( ntot > 0 ) slen = ntot; // if buffer size is known and may contain nulls
  if( cstr == 0 || slen <= 0 ) {
    //clog<<"Runtime::rmJunk> ?empty string"<<endl;
    return 0;
  }
  if( (ntot==1 || slen == 1) && *cstr == '\r' ) {
    *cstr = '\n'; // replace cr with nl for this special case?
    return slen;
  }
  // count carriage returns, if any:
  int ncr = 0;
  char* cs = cstr; 
    while( *cs != 0 ) {
    if( *cs++ == '\r' )
      ++ncr;
  }
  // eliminate white-space oddities != nl (dec. 10)
  // except 'final' carriage-return, if any, replace with newline?
  int nr= 0, nrep= 0, nt= 0;
  cs = cstr;
  while( *cs != 0 || (ntot > 0 && nt < ntot) ) {
    if( *cs == '\r' ) {
      if( ++nr < ncr ) {
	*cs = '?'; ++nrep;
      }
      else {
	*cs = '\n'; // replace final cr with nl
      }
    }
    else if( *cs < 10 || (*cs > 10 && *cs < 32) ) {
      *cs = '?'; ++nrep;
    }
    ++cs; ++nt;
  }
  //clog<<"Runtime::rmJunk> non-newline ascii chars < 32 (space): "<<nrep<<endl;
  return nrep;
}

// helper func. removes any extraneous junk like '\r', '\n', '\t', ... 
int Runtime::rmJunk(string& s) {
  int slen = s.length();
  if( slen <= 0 ) {
    clog<<"Runtime::rmJunk> ?empty string"<<endl;
    return 0;
  }
  if( slen == 1 && s == "\r" ) {
    s = "\n";
    return slen;
  }
  char* cstr = (char*) s.c_str();
  char* cs = cstr;
  int ncr = 0;
  while( *cs != 0 ) {
    if( *cs++ == '\r' )
      ++ncr;
  }
  // eliminate white-space oddities != nl (dec. 10)
  // except 'final' carriage-return, if any, replace with newline?
  int nr= 0, nrep= 0;
  cs = cstr;
  while( *cs != 0 ) {
    if( *cs == '\r' ) {
      if( ++nr < ncr ) {
	*cs = '?'; ++nrep;
      }
      else {
	*cs = '\n'; // replace final cr with nl
      }
    }
    else if( *cs < 10 || (*cs > 10 && *cs < 32) ) {
      *cs = '?'; ++nrep;
    }
    ++cs;
  }
  s = cstr;
  //clog<<"Runtime::rmJunk> non-newline ascii chars < 32 (space): "<<nrep<<endl;
  return nrep;
}

//helper funcs to convert a number to string:
string Runtime::numToStr( double number ) {
  strstream ss;
  ss << number << ends;
  string strnum = ss.str();
  delete ss.str();
  return strnum;
}

string Runtime::numToStr( int number ) {
  strstream ss;
  ss << number << ends;
  string strnum = ss.str();
  delete ss.str();
  return strnum;
}

string Runtime::numToStr( time_t number ) {
  strstream ss;
  ss << number << ends;
  string strnum = ss.str();
  delete ss.str();
  return strnum;
}

int Runtime::argVec(string& options, Runtime::Argv& args) {
  args.clear();
  size_t pos= 0, wpos= 0;
  rmJunk(options); // eliminate multiple white spaces

  while( wpos != string::npos && wpos < options.length() - 1 ) {
    pos = wpos;
    wpos = options.find(" ", 1+pos);
    if( wpos == string::npos )
      wpos = options.length()-1;
    int len = wpos - pos;
    if( len > 0 ) args.push_back(options.substr(pos, len));
  }

  return (int)args.size();
}

int Runtime::argVec(int argc, char** argv, Runtime::Argv& args) {
  args.clear();
  for( int i=0; i<argc; i++ )
    args.push_back(argv[i]);
  return argc;
}

FILE* Runtime::pipeReadChild(const char *command) {
  // should check/set PATH and LD_LIBRARY_PATH
  char* ldp = getenv("LD_LIBRARY_PATH");
  string ldpath = "/usr/local/lib:/share/local/lib:/opt/local/lib:/opt/EDTpdv ";
  if( ldp != 0 )
    ldpath = string(ldp) + ":" + ldpath;
 
  char* ufinst = getenv("INSTALL");
  string ufcmd, uf="/share/local/uf";
  if( ufinst ) {
    uf = ufinst; uf += "/lib:"; uf += ldpath;
  }
  string env = string("env LD_LIBRARY_PATH=") + uf;
  //ufcmd = env + " sh 2>&1 " + command; // full invocation of dynamic binary
  ufcmd = env + command; // full invocation of dynamic binary
  return ::popen(ufcmd.c_str(), "r");
}

string Runtime::pipeRead(FILE* fp) {
  string info;
  if( fp == 0 )
    return info;

  char buf[BUFSIZ]; memset(buf, 0, sizeof(buf));
  while(fgets(buf, BUFSIZ, fp) != 0) {
    info += buf; memset(buf, 0, sizeof(buf));
  }
  ::pclose(fp);
  return info;
}

int Runtime::parseWords(const string& text, vector< string >& words, const string& delim) {
  size_t len = text.length();
  if( len <= 0 )
    return (int)len;

  char* c = (char*)text.c_str();
  while( *c == ' ' || *c == '\t' )
    ++c;
  string txt = c; // eliminate leading white spaces

  // parse text via delim
  len = txt.length();
  size_t pd = txt.find(delim);
  if( pd == string::npos || pd >= len ) { // must be only one word
    words.push_back(txt);
    return (int) words.size();
  }
  do {
    if( pd != string::npos ) {
      len = pd;
      string t = txt.substr(0, len);
      words.push_back(t);
      //clog<<"Runtime::parseWords> added t: "<<t<<endl;
      if( ++pd <= txt.length() ) {
        txt = txt.substr(pd);
        c = (char*)txt.c_str();
        while( *c == ' ' || *c == '\t' )
	  ++c;
        txt = c; // eliminate leading white spaces
        pd = txt.find(delim);
        //clog<<"Runtime::parseWords> next txt: "<<txt<<", delimpos: "<<pd<<endl;
      }
    }
    if( pd == string::npos && txt.length() > 0 ) { // must be last word?
      words.push_back(txt);
      //clog<<"Runtime::parseWords> added txt: "<<txt<<endl;
    }
  } while ( pd != string::npos || pd <= txt.length() );
  
  return (int) words.size();
}

int Runtime::parseWords(const string& text, char**& cwords, const string& delim) {
  vector< string > words;
  int nw = parseWords(text, words);
  if( nw <= 0 )
    return nw;

  cwords = (char**) new char* [(1+nw)*sizeof(char*)];
  for( int i = 0; i < nw; ++i ) {
    char* cw = (char*) words[i].c_str();
    size_t wlen = 1 + words[i].length();  
    cwords[i] = (char*) new char* [wlen]; memset(cwords[i], 0, wlen);
    strcpy(cwords[i], cw);
    //clog<<"Runtime::parseWords> "<<i<<"  "<<words[i]<<endl;
  } 
  cwords[nw] = 0; // terminate with null
  return nw;
}

// evidently full duplex pipes are not yet supported on linux
// so this actually manages two half duplex pipes (thus supporting both linux & solaris)
pid_t Runtime::pipeReadWriteChild(const string& cmd, int& tochild, int& fromchild) {
  // should check/set PATH and LD_LIBRARY_PATH
  char* ldp = getenv("LD_LIBRARY_PATH");
  string ldpath = "/usr/local/lib:/gemini/epics/lib:/usr/local/epics/lib:/opt/EDTpdv";
  if( ldp != 0 )
    ldpath = string(ldp) + ":" + ldpath;
 
  char* ufinst = getenv("INSTALL");
  if( ufinst == 0 ) {
    clog<<"Runtime::pipeReadWriteChild> UNINSTALL not set?"<<endl;
  }
  string uflib = ufinst;
  uflib += "/lib";
  if( ldpath.find(uflib) == string::npos ) {
    //ldpath += ":"; ldpath += ufinst; ldpath += "/lib";
    ldpath = "/lib:" + ldpath;
    ldpath = ufinst + ldpath;
  }
  strstream s;
  s<<"LD_LIBRARY_PATH="<<ldpath<<ends;
  putenv(s.str()); // insure environ is found
  //clog<<"Runtime::pipeReadWriteChild> "<<s.str()<<endl;
  string env = "/bin/env ";
  env += s.str();
  delete s.str();

  int len = cmd.length();
  if( len <= 0 ) { // bad cmd?
    clog<<"Runtime::pipeReadWriteChild> bad cmd: "<<cmd<<endl;
    return (pid_t)-1;
  }

  string ufcmd = env;
  ufcmd += " ";
  ufcmd += ufinst;
  ufcmd += "/bin/";
  ufcmd += cmd;
  // parse cmd
  char** args= 0;
  int cc = Runtime::parseWords(ufcmd, args); 
  if( cc <= 0 || ufcmd.find(args[0]) == string::npos ) { // bad cmd or parsing?
    if( args != 0 && args[0] != 0 && args[1] != 0 )
      clog<<"Runtime::pipeReadWriteChild> bad cmd: "<<ufcmd<<", parsed argv[0]: "<<args[0]<<", args[1]: "<<args[1]<<endl;
    else if( args != 0 && args[0] != 0 )
      clog<<"Runtime::pipeReadWriteChild> bad cmd: "<<ufcmd<<", parsed argv[0]: "<<args[0]<<endl;
    return (pid_t)-1;
  }
  // Stevens full duplex (SVr4) logic does not appear to work on Linux,
  // so use two half duplex pipes:
  //int fds[2] = { -1, -1 }; // fd[0] used by child for stdin, writen by parent, fd[1] for writing by child, etc.
  //int stat = ::pipe(fds);
  int fds1[2] = { -1, -1 }; // fd[0] used by child for stdin, writen by parent, fd[1] for writing by child, etc.
  int fds2[2] = { -1, -1 }; // fd[0] used by child for stdin, writen by parent, fd[1] for writing by child, etc.
  int stat1 = ::pipe(fds1);
  int stat2 = ::pipe(fds2);

  //bool childfailed = false;
  if( stat1 < 0 ) {
    clog<<"Runtime::pipeReadWriteChild> failed to create first half duplex pipe..."<<endl;
    //childfailed = true;
    tochild = -1; fromchild = -1;
    exit(stat1);
  }
  if( stat2 < 0 ) {
    clog<<"Runtime::pipeReadWriteChild> failed to create second half duplex pipe..."<<endl;
    //childfailed = true;
    tochild = -1; fromchild = -1;
    ::close(fds1[0]); ::close(fds1[1]);
    exit(stat2);
  }

  // better to set these here for gdb:
  // parent writes to pipe1 write and reads from pipe2 
  tochild = fds1[1]; fromchild = fds2[0];

  pid_t child = ::fork();
  if( child == 0 ) {
    // this is the child context: read fds1[0] & write fds2[1]
    //::close(0); ::close(1);
    ::close(fds1[1]); ::close(fds2[0]);
    //dup2(fds[0], 0); dup2(fds[1], 1); // dup the stdin and stdout to pipe, and exec cmd...
    dup2(fds1[0], 0); dup2(fds2[1], 1); // dup the stdin to read of pipe1 and stdout to write of pipe2, and exec cmd...
    //if( args != 0 && args[0] != 0 && args[1] != 0 )
    // clog<<"Runtime::pipeReadWriteChild> Ok cmd: "<<ufcmd<<", parsed argv[0]: "<<args[0]<<", args[1]: "<<args[1]<<endl;
    //else if( args != 0 && args[0] != 0 )
    // clog<<"Runtime::pipeReadWriteChild> Ok cmd: "<<ufcmd<<", parsed argv[0]: "<<args[0]<<endl;
    int stat = ::execvp(args[0], args);
    if( stat < 0 ) {
      //childfailed = true;
      clog<<"Runtime::pipeReadWriteChild> execvp failed, child exiting, errno: "<<strerror(errno)<<endl;
      exit(stat);
    }
  }

  // parent: write fds1[0] & read fds2[1]
  if( child < 0 ) { // failed fork
    clog<<"Runtime::pipeReadWriteChild> failed to fork "<<cmd<<endl;
    //::close(fds[0]); ::close(fds[1]);
    ::close(fds1[0]); ::close(fds1[1]); ::close(fds2[0]); ::close(fds2[1]);
    tochild = -1; fromchild = -1;
    return child;
  }

  // parent should free unused fds
  ::close(fds1[0]); ::close(fds2[1]);

  //PosixRuntime::sleep(0.1); // don't worry about child failing, test io later... 
  /*
  if( childfailed ) {
    clog<<"Runtime::pipeReadWriteChild> failed to execvp "<<cmd<<endl;
    //::close(fds[0]); ::close(fds[1]);
    ::close(fds1[0]); ::close(fds1[1]); ::close(fds2[0]); ::close(fds2[1]);
    tochild = -1; fromchild = -1;
    return (pid_t)-1;
  }
  */
  //clog<<"Runtime::pipeReadWriteChild> tochild: "<<tochild<<", fromchild: "<<fromchild<<endl;
  return child;
} // pipeReadWriteChild using 2 half duplex pipes (4 fds)

// evidently creating FILE* from open fds does not work (at lest not on linux)
pid_t Runtime::pipeReadWriteChild(const string& cmd, FILE*& tochild, FILE*& fromchild) {
  int fds[2] = { -1, -1 };
  pid_t cp = pipeReadWriteChild(cmd, fds[0], fds[1]);
  if( cp < 0 )
    return cp;

  tochild = fdopen(fds[0], "w"); // used by child for stdin, written by parent
  if( tochild == 0 ) {
    clog<<"Runtime::pipeReadWriteChild> fdopen failed on "<<fds[0]<<", err: "<<strerror(errno)<<endl;
    return (pid_t)-2;
  }
  fromchild = fdopen(fds[1], "r"); // used by child for stdout, read by parent
  if( fromchild == 0 ) {
    clog<<"Runtime::pipeReadWriteChild> fdopen failed on "<<fds[1]<<", err: "<<strerror(errno)<<endl;
    return (pid_t)-3;
  }

  return cp;
}
  
// support send & recv on pipe:
int Runtime::psend(int pipeFd, const string& ss) {
  int nw = ::write(pipeFd, ss.c_str(), ss.length());
  if( nw < (int)ss.length() ) 
    clog<<"Runtime::psend> failed to write to pipe, errno: "<<strerror(errno)<<endl;

  return nw;
}

int Runtime::precv(int pipeFd, string& rs) {
  char buf[BUFSIZ]; ::memset(buf, 0, BUFSIZ);
  char c;
  int i= 0, nr= 0;
  do {
    nr = ::read(pipeFd, &c, 1);
    if( nr < 1 ) 
      clog<<"Runtime::precv> failed to read from pipe, errno: "<<strerror(errno)<<endl;
    if( c != '\n' ) buf[i] = c; 
  } while( nr >= 0 && c != '\n' && ++i < BUFSIZ );

  rs = buf;
  return nr;
}

// exec an external application (fork & exec):
// provide full path & all options in one string (like popen)
// appcmd could be: "env FOO=foo /usr/local/sbin/app"
// or "/usr/local/sbin/app" or have more than one environment var.
pid_t Runtime::execApp(const string& appcmd, char** envp) {
  clog<<"Runtime::execApp> appcmd: "<<appcmd<<endl;
  int cmdpos = 0;

  int envpos = appcmd.find("env");
  if( envpos != (int)string::npos )
    envpos = appcmd.find(" ", 1+envpos);

  int eqpos = appcmd.find("=");
  while( envpos != (int)string::npos && eqpos != (int)string::npos ) {
    cmdpos = appcmd.find(" ", eqpos);
    string envar = appcmd.substr(envpos+1, cmdpos-envpos-1);
    char* ev = (char*)envar.c_str();
    while( *ev == ' ' || *ev == '\t' ) ++ev; envar = ev; // eliminate any additional spaces
    string envnm = envar.substr(0,envar.find("="));
    char* e = ::getenv(envnm.c_str());
    if( e ) envar = envar + ":" + e;
    // oddly, once this is done, a getenv of the same env. var. fails!
    //clog<<"execApp> putenv: "<<envar<<endl;
    //if( ::putenv((char*)envar.c_str()) )
    //clog<<"Runtime::execApp> putenv failed."<<errStr()<<endl;
    envpos = appcmd.find(" ", 1+envpos);
    eqpos = appcmd.find("=", 1+eqpos);
  }

  string cmd = appcmd.substr(cmdpos, appcmd.length() - cmdpos);
  char* c = (char*)cmd.c_str();
  while( *c == ' ' || *c == '\t' )  ++c; cmd = c; // elim. whites
  clog<<"Runtime::execApp> cmd: "<<cmd<<endl;

  /*
  // although this successfully runs with the proper environment
  // it does so under a subshell, so the returned pid is useless
  // for waitpid?
  // child proc. execs the app.
  pid_t pid = ::fork();
  if( pid < 0 ) { 
    clog<<"Runtime::execApp> fork failed..."<<endl; getchar();
    return pid;
  }
  if( pid != 0 ) { // parent proceeds
  //yield();
    return pid;
  }
  int exstat = ::execle("/usr/bin/sh", "sh", "-c", cmd.c_str(), 0, envp);
  if( exstat < 0 ) clog<<"Runtime::execApp> execle failed: "<<errStr()<<endl;
  return exstat;
  */

  // init argv from cmd string
  int pos = 0;
  int rpos = cmd.find(" ");
  // deal with options:
  vector < string > args;
  char* ldpath = ::getenv("LD_LIBRARY_PATH");
  if( ldpath == 0 )
    clog<<"Runtime::execApp> LD_LIBRARY_PATH null?"<<endl;
  else {
    string envstr = "/usr/bin/env";
    string ldpstr = "LD_LIBRARY_PATH=";
    ldpstr += ldpath;
    clog<<"Runtime::execApp> "<<ldpstr<<endl;
    args.push_back(envstr);
    args.push_back(ldpstr);
  }
  while( true ) {
    if( rpos != (int) string::npos ) {
      args.push_back(cmd.substr(pos, rpos-pos));
    }
    else {
      args.push_back(cmd.substr(pos, cmd.length()-pos));
      break;
    }
    pos = rpos+1;
    rpos = cmd.find(" ", pos);
  }
  // use the heap & init. all pointers to null
  char **argv = (char**)calloc(1+args.size(), sizeof(char*)); 
  for( int i = 0; i < (int)args.size(); ++i ) {
    char* c = (char*)args[i].c_str();
    while( *c == ' ' || *c == '\t' ) ++c; // eliminate whites
    argv[i] = c;
    //clog<<"Runtime::execApp>i= "<<i<<", argv[i]= "<<argv[i]<<endl;
  }
  clog<<"Runtime::execApp> execve of:";
  for( int i = 0; i < (int)args.size(); ++i )
    clog<<" "<<argv[i];
  clog<<endl;

  // child proc. execs the app.
  pid_t pid = ::fork();
  if( pid < 0 ) { 
    clog<<"Runtime::execApp> fork failed..."<<endl; getchar();
    return pid;
  }
  if( pid != 0 ) { // parent proceeds
    yield();
    return pid;
  }

  int exstat = ::execve(argv[0], argv, envp);
  if( exstat < 0 ) clog<<"Runtime::execApp> execve failed: "<<errStr()<<endl;
  return exstat;
}

string Runtime::hostname() {
  char info[MAXNAMLEN + 1];
  ::memset(info,0,sizeof(info));
  ::gethostname(info, sizeof(info));
  string host = info;
  //clog<<"Runtime::hostname> "<<host<<"(localhost)."<<endl;
  return host;
}

int Runtime::bell(int rings) {
  const char bell = 0x07;
  while ( rings-- > 0 ) {
    ::write(2, &bell, 1);
  }
  return rings;
}

string Runtime::argVal(const char* arg, const Runtime::Argv& args) {
  string a;
  //clog<<"Runtime::argVal> "<<a<<endl;
  int acnt = args.size();
  for(int i= 1; i < acnt; i++ ) {
    a = args[i];
    if( a == arg ) { // found argument!
      // check if next argv element is its value (if not a numeric, must lack "-"):
      a = "true";
      if( ++i >= acnt )
	return a;
      char *lackdash = (char*)args[i].c_str();
      if( lackdash == 0 )
	return a;
      if( lackdash[0] != '-' ) {
	// eliminate any quotes in arg:
	int eot = ::strlen(lackdash) - 1;
	if( lackdash[eot] == '"' ) lackdash[eot] = '\0';
	if( lackdash[0] == '"' ) ++lackdash;
        a = string(lackdash);
      }
      else if( lackdash[0] == '-' && isdigit(lackdash[1]) ) { // numeric can be negative
        a = string(lackdash);
      }
      return a;
    }
  }
  // if we get here, arg not found:
  return "false";
}

// implement Kalev's test (from ansi/iso c++ handbook):
bool Runtime::littleEndian() {
  union endian { unsigned int n; unsigned char b[sizeof(int)]; };
  endian e = { 1U };
  return (e.b[0] == 1U); // big-endian: e.b[0] == 0, according to Danny Kalev p237
}

#if defined(LINUX) || defined(SOLARIS)
void Runtime::sysinfo(std::vector< string > *infovec) {
  char info[MAXNAMLEN + 1];
  ::memset(info,0,sizeof(info));
#if defined(SOLARIS)
  int cmd[12] = { SI_SYSNAME, SI_HOSTNAME, SI_SET_HOSTNAME,
	          SI_RELEASE, SI_VERSION, SI_MACHINE, SI_ARCHITECTURE,
	          SI_PLATFORM, SI_HW_PROVIDER, SI_HW_SERIAL, SI_SRPC_DOMAIN,
	          SI_SET_SRPC_DOMAIN };
  char* si[12] = { "SI_SYSNAME", "SI_HOSTNAME", "SI_SET_HOSTNAME",
	          "SI_RELEASE", "SI_VERSION", "SI_MACHINE", "SI_ARCHITECTURE",
	          "SI_PLATFORM", "SI_HW_PROVIDER", "SI_HW_SERIAL",
		  "SI_SRPC_DOMAIN", "SI_SET_SRPC_DOMAIN" };
	       
  for( int i = 0; i < 12; ++i ) { 
    ::sysinfo(cmd[i], info, sizeof(info));
    clog<<si[i]<<" : "<<info<<endl;
    if( infovec ) {
      string s = si[i]; s += " : "; s += info;
      infovec->push_back(s);
    }
  }
  string e = "BigEndian";
  if( littleEndian() ) e = "LittleEndian";
  clog<<e<<endl;
  if( infovec ) infovec->push_back(e);
  size_t sz = sizeof(long);
  strstream ss;
  ss<<"SizeOf Long: "<<sz<<endl;
  clog<<ss.str();
  if( infovec ) infovec->push_back(ss.str());
  delete ss.str();
#elif defined(LINUX)
  clog<<hostname()<<endl;
#endif
  return;
}
#endif

string Runtime::cwdPath() {
  string ret= "";
  char cwdbuf[PATH_MAX+1]; memset(cwdbuf, 0, PATH_MAX+1);
  char *cwd = ::getcwd(cwdbuf, PATH_MAX);
  if( cwd )
    ret = cwd;

  return ret;
}

string Runtime::pathToSelf(const string& self) {
  string path;
  int pos = self.rfind("/");
  if( self.find("/") == 0 ) { // full path is present in invocation, eliminate tail
    path = self.substr(0, pos);
    return path;
  }
  string Path = getenv("PATH");
  string dir;
  pos = Path.find(":");
  int rpos = Path.rfind(":");
  int ppos = 0;
  int len =  pos-ppos;
  while( pos <= rpos ) {
    if( pos == rpos ) 
      len = Path.length() - pos;
    else
      len = pos - ppos;
    dir = Path.substr(ppos, len);
    ppos = 1+pos; pos = Path.find(":", ppos);
    path = dir + "/" + self;
    struct stat st;
    int s = stat(path.c_str(), &st);
    if( s == 0 ) break;
  }
  return path;
}

/*
pid_t Runtime::createChild(Daemon* d, void* p) {
  //pid_t pid = ::fork1(); // fork1 required for multi-threaded parent (Solaris 2.5.x and earlier)
  pid_t pid = ::fork(); // solaris 2.7 works for mt

  if( pid == (pid_t) -1  ) {
    clog<<"Runtime::createChild> failed to create child process for: "<<
           d->description()<<endl;
    return pid;
  }
  else if( pid != 0 ) { // this is the parent
    clog<<"Runtime::createChild> created child process for: "
        <<d->description()<<", pid= "<<pid<<endl;
    //clog<<"Runtime::createChild> type any key to cont. parent..."<<endl;
    //getchar();
    return pid;
  }
  // this is the child, returns on child exit
  //clog<<"Runtime::createChild> type any key to cont. child::exec()..."<<endl;
  //getchar();
  (Daemon*)d->exec(p);

  return pid;
}
*/

string Runtime::currentDate(const string& zone) {
  struct tm *t, tbuf;
  struct timespec tspec;

  /* according to man page, struct tm elements:
  int tm_sec;    // seconds after the minute - [0, 61] for leap seconds 
  int tm_min;    // minutes after the hour - [0, 59] 
  int tm_hour;   // hour since midnight - [0, 23] 
  int tm_mday;   // day of the month - [1, 31] 
  int tm_mon;    // months since January - [0, 11] 
  int tm_year;   // years since 1900 
  int tm_wday;   // days since Sunday - [0, 6] 
  int tm_yday;   // days since January 1 - [0, 365] 
  */

#if defined(SOLARIS)
  ::clock_gettime(CLOCK_REALTIME, &tspec); // this is in fact a posix func. 
  const time_t sec = tspec.tv_sec;
#else
  time_t sec = ::time(0); // Linux still lacks clock_gettime ?
  tspec.tv_nsec = 0;
#endif
  if( zone.find("utc") == string::npos &&
      zone.find("UTC") == string::npos &&
      zone.find("gmt") == string::npos && 
      zone.find("GMT") == string::npos ) { // use localtime
    t = ::localtime_r(&sec, &tbuf); // mt-safe
  }
  else {
    t = ::gmtime_r(&sec, &tbuf); // mt-safe
  }
  char s[] = "yyyy:ddd";
  if( t == 0 )
    return s;

  sprintf(s, "%04d:%03d", 1900+t->tm_year, t->tm_yday);
  string retval(s);
  return retval;
}

string Runtime::currentTime(const string& zone) {
  struct tm *t, tbuf;
  struct timespec tspec;

#if defined(SOLARIS)
  ::clock_gettime(CLOCK_REALTIME, &tspec); // this is in fact a posix func. 
  time_t sec = tspec.tv_sec;
#else
  time_t sec = ::time(0); // Linux still lacks clock_gettime ?
  tspec.tv_nsec = 0;
#endif

  if( zone.find("utc") == string::npos &&
      zone.find("UTC") == string::npos &&
      zone.find("gmt") == string::npos && 
      zone.find("GMT") == string::npos ) { // use localtime
    t = ::localtime_r(&sec, &tbuf); // mt-safe
  }
  else {
    t = ::gmtime_r(&sec, &tbuf); // mt-safe
  }

  char s[] = "yyyy:ddd:hh:mm:ss.uuuuuu";
  if( t == 0 )
    return s;

  sprintf(s, "%04d:%03d:%02d:%02d:%02d.%06d",
	  1900+t->tm_year, t->tm_yday, t->tm_hour, t->tm_min, t->tm_sec,
	  (int) (tspec.tv_nsec/1000));
  string retval(s);
  return retval;
}

string Runtime::adjTime(const string& time, float delta, const string& zone) {
  if( ::fabs(delta) < 0.0000001 ) // millisec resolution
    return time;

  char s[] = "yyyy:ddd:hh:mm:ss.uuuuuu";
  strncpy(s, time.c_str(), strlen(s)); 
  struct timespec tspec;
  struct tm tbuf, *t = &tbuf;
  // parse timestamp into tbuf & tspec 
  /* according to man page, struct tm elements:
  int tm_sec;    // seconds after the minute - [0, 61] for leap seconds 
  int tm_min;    // minutes after the hour - [0, 59] 
  int tm_hour;   // hour since midnight - [0, 23] 
  int tm_mday;   // day of the month - [1, 31] 
  int tm_mon;    // months since January - [0, 11] 
  int tm_year;   // years since 1900 
  int tm_wday;   // days since Sunday - [0, 6] 
  int tm_yday;   // days since January 1 - [0, 365] 
  */
  sscanf(s, "%04d:%03d:%02d:%02d:%02d.%06d",
         &tbuf.tm_year, &tbuf.tm_yday, &tbuf.tm_hour, &tbuf.tm_min, &tbuf.tm_sec, 
	 (int*)&tspec.tv_nsec);

  bool leap = leapYear(tbuf.tm_year);
  bool prevleap = leapYear(tbuf.tm_year-1);

  // adjust the time
  int secdel = 0;
  if( ::fabs(delta) >= 1.0 )
    secdel = (int) ::floor(::fabs(delta));
  int nanosec = (int)((::fabs(delta) - secdel)*1000000000.0);
  //clog<<"adjTime> delta: "<<delta<<", secdel: "<<secdel<<", nansec: "<<nanosec<<endl;

  int yrdel = 0;
  int daydel = secdel/(24*3600);
  int hrdel = secdel/3600 - daydel*24;
  int mindel = secdel/60 - hrdel*60;

  secdel -= (mindel*60 + hrdel*3600 + daydel*24*3600);

  //clog<<"adjTime> delta: "<<delta<<", mindel: "<<mindel<<", hrdel: "<<hrdel<<", daydel: "<<daydel<<endl;
  //clog<<"adjTime> delta: "<<delta<<", secdel: "<<secdel<<", nansec: "<<nanosec<<endl;

  if( delta < 0 ) { // this should be the case if used for est. frame time adjustment
    if( prevleap )
      yrdel =  secdel / (24*3600*366);
    else
      yrdel =  secdel / (24*3600*365);

    int nsd = tspec.tv_nsec - nanosec;
    if( nsd >= 0 ) {
      tspec.tv_nsec = nsd;
    }
    else {
      tspec.tv_nsec = 1000000000 + nsd;
      secdel += 1;
    }
    int sd = tbuf.tm_sec - secdel;
    if( sd >= 0 ) {
      tbuf.tm_sec = sd;
    }
    else {
      tbuf.tm_sec = 60 + sd;
      mindel += 1;
    }
    int md = tbuf.tm_min - mindel;
    if( md >= 0 ) {
      tbuf.tm_min = md;
    }
    else {
      tbuf.tm_min = 60 + md;
      hrdel += 1;
    }
    int hd = tbuf.tm_hour - hrdel;
    if( hd >= 0 ) {
      tbuf.tm_hour = hd;
    }
    else {
      tbuf.tm_hour = 24 + hd;
      daydel += 1;
    }
    int ydd= tbuf.tm_yday - daydel;
    if( ydd >= 0 ) {
      tbuf.tm_yday = ydd;
    }
    else if( prevleap ) {
      yrdel =  secdel / (24*3600*366);
      tbuf.tm_yday = 366 + ydd;
      yrdel += 1;
    }
    else {
      yrdel =  secdel / (24*3600*365);
      tbuf.tm_yday = 365 + ydd;
      yrdel += 1;
    }
    tbuf.tm_year -= yrdel;

    // no need to add 1900 to tm_year since this originated from uf timestamp...
    sprintf(s, "%04d:%03d:%02d:%02d:%02d.%06d",
	    t->tm_year, t->tm_yday, t->tm_hour, t->tm_min, t->tm_sec, (int) tspec.tv_nsec/1000);

    string retval(s);
    return retval;
  } // delta < 0

  // if we get here delta > 0
  if( leap )
    yrdel =  secdel / (24*3600*366);
  else
    yrdel =  secdel / (24*3600*365);

  secdel -= (mindel*60 + secdel*3600 + daydel*24*3600);
  int nsd = tspec.tv_nsec + nanosec;
  if( nsd < 1000000000 ) {
    tspec.tv_nsec = nsd;
  }
  else {
    tspec.tv_nsec = nsd - 1000000000;
    secdel += 1;
  }
  int sd = tbuf.tm_sec + secdel;
  if( sd < 60 ) {
    tbuf.tm_sec = sd;
  }
  else {
    tbuf.tm_sec = sd - 60;
    mindel += 1;
  }
  int md = tbuf.tm_min + mindel;
  if( md < 60 ) {
    tbuf.tm_min = md;
  }
  else {
    tbuf.tm_min = md - 60;
    hrdel += 1;
  }
  int hd = tbuf.tm_hour + hrdel;
  if( hd < 24 ) {
    tbuf.tm_hour = hd;
  }
  else {
    tbuf.tm_hour = hd - 24;
    daydel += 1;
  }
  int ydd= tbuf.tm_yday + daydel;
  if( ydd < 365 ) {
    tbuf.tm_yday = ydd;
  }
  else if( leap && ydd < 366 ) {
    tbuf.tm_yday = ydd;
  }
  else if( leap ) {
    tbuf.tm_yday = ydd;
    yrdel =  secdel / (24*3600*366);
    tbuf.tm_yday = ydd - 366;
    yrdel += 1;
  }
  else {
    yrdel =  secdel / (24*3600*365);
    tbuf.tm_yday = ydd - 365;
    yrdel += 1;
  }
  tbuf.tm_year += yrdel;

  // no need to add 1900 to tm_year since this originated from uf timestamp...
  sprintf(s, "%04d:%03d:%02d:%02d:%02d.%06d",
          t->tm_year, t->tm_yday, t->tm_hour, t->tm_min, t->tm_sec, (int)tspec.tv_nsec/1000);

  string retval(s);
  return retval;
}

int Runtime::directory(vector< string >& list, const char* dnam) {
  DIR* dir = ::opendir(dnam);
  if( !dir ) {
    clog<<"Runtime::directory> opendir failed on "<<dnam<<endl;
    return 0;
  }
  int pathlimit = ::pathconf(dnam, _PC_NAME_MAX);
  struct dirent* entry = (struct dirent*) new char[sizeof(struct dirent)+pathlimit+1];
  struct dirent* result = (struct dirent*) -1;

  string path(dnam);
  int stat = 0;
  while( stat == 0 && result != 0 ) {
#if defined(LINUX) || defined(SOLARIS)
    stat = ::readdir_r(dir, entry, &result);
#else
    result = entry = ::readdir(dir);
#endif
    if( stat == 0 && result != 0 )
      list.push_back(path+"/"+string(entry->d_name));
    else
      clog<<"setDirent> "<<strerror(errno)<<", element count= "<<list.size()<<endl;
  }
#if defined(LINUX) || defined(SOLARIS)
  delete entry;
#endif
  ::closedir(dir);
  return list.size();
}

int Runtime::writable(int fd, float timeOut) {
  if( fd < 0 )
    return fd;

  fd_set writefds, excptfds;
  FD_ZERO(&writefds); FD_ZERO(&excptfds);
  FD_SET(fd, &writefds); FD_SET(fd, &excptfds);

  struct timeval *infinit=0; // tenthsec = { 0L, 100000L }; // 0.1 sec.
  struct timeval *usetimeout= 0, timeout, poll = { 0, 0 }; // don't block
  if( timeOut > 0.00000001 ) {
    timeout.tv_sec = (long) floor(timeOut);
    timeout.tv_usec = (unsigned long) floor(1000000*(timeOut-timeout.tv_sec));
    usetimeout = &timeout;
  }
  else if( timeOut > -0.00000001 ) // ~zero timout
    usetimeout = &poll;
  else // negative timeout indicates infinite (wait forever, blocking)
    usetimeout = infinit;

  int fdcnt, cnt= PosixRuntime::_MaxInterrupts_;
  do {
    // since select may modify this area of memory, always provide fresh timeout val:
    struct timeval to, *to_p= 0;
    if( usetimeout ) { to = *usetimeout; to_p = &to; }
    fdcnt = ::select(1+fd, 0, &writefds, &excptfds, to_p);
  } while( errno == EINTR && --cnt >= 0 );

  if( errno == EBADF ) {
    clog << "Socket::writable> invalid (EBADF) socket fd= "<<fd<<endl;
    return -1;
  }

  if( fdcnt < 0 ) {
    clog << "Socket::writable> error occured on select" << endl;
    return fdcnt;
  }
  if( fdcnt == 0 ) {
    //clog << "Socket::writable> timed-out, not writable" << endl;
    return fdcnt;
  }
  if( FD_ISSET(fd, &excptfds) ) {
    clog << "Socket::writable> exception present on Fd: "<<fd<< endl;
    return -1;
  }
  //clog << "Socket::writable> ok" << endl;
  return FD_ISSET(fd, &writefds);
}

int Runtime::writable(FILE* fp, float timeOut) {
  if( fp == 0 )
    return -1;
  return writable(fileno(fp), timeOut);
}

#if defined(LINUX) || defined(SOLARIS)
int Runtime::available(int fd, float wait, int trycnt) {
  int retval = 0, stat= 0;
  while( --trycnt >= 0 && stat == 0 ) {
    // try immediately
    int stat = ::ioctl(fd, FIONREAD, &retval);
    if( stat != 0 ) {
      clog<<"Runtime::available> "<<strerror(errno)<<endl;
      return -1;
    }
    // then sleep and try it for trycnt
    mlsleep(wait);
  }
  return retval;
}

int Runtime::available(string fifoname, float wait, int trycnt) {
  int retval= 0;
  int fd = ::open(fifoname.c_str(), O_RDONLY | O_NONBLOCK );
  if( fd < 0 )
    return -1;
  retval = available(fd, wait, trycnt);
  ::close(fd);
  return retval;
}
#else
int Runtime::available(int fd, float wait, int trycnt) { return 0; }
int Runtime::available(string fifoname, float wait, int trycnt) { return 0; }
#endif

// stubs
void Runtime::initSysLog() {}
void Runtime::writeSysLog() {}
void Runtime::alarmPopUp() {}

// virtuals
void Runtime::initialize(int argc, char** argv) {}

// sleep a milli-sec or longer (nanosleep is not present in LINUX)?
void Runtime::mlsleep(float time) {
  if( time < 0.001 ) time = 0.001;
  struct timeval timeout;
  timeout.tv_sec = (long) ::floor(time);
  timeout.tv_usec = (long) ::floor(1000000*(time-timeout.tv_sec));
  int fdcnt = ::select(FD_SETSIZE, 0, 0, 0, &timeout);
  if( PosixRuntime::_verbose ) 
    clog<<"Runtime::mlsleep> time: "<<time<<", select fdcnt: "<<fdcnt<<endl;
}

int Runtime::cmdOpt(char* opt, int argc, char** argv) {
  string a(argv[0]);
 
  //clog<<"Runtime::cmdOpt> "<<a<<endl;
 
  for(int i=1; i < argc; i++ ) {
    string arg(argv[i]);
    if( arg == opt )
      return i;
  }
  return 0;
}
int Runtime::cmdLine(int argc, char** argv) { return 0; }

#endif // __Runtime_cc__
