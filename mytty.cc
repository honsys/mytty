#if !defined(__MYTTY_C__)

// termio defs: http://pubs.opengroup.org/onlinepubs/009695399/basedefs/termios.h.html
// consider (newer) cfsetspeed and related c system calls (man termios)

#include "fcntl.h"
#include "stdio.h"
#include "stdlib.h"
#include "termio.h"
#include "unistd.h"

// stdc++
#include "iostream"
#include "string"

#include "Runtime.h"

static bool _reset = false;
static bool _verbose = false;
static bool _terminate = false;
static bool _cr = false; // replace '\n' with '\r'
static bool _enq = false; // follow each recv'd <ACQ> == 6  with raw <ENQ> == 5
static bool _crlf = false; // replace '\n' or '\r' with '\r\n'
static bool _prompt = false; // force initial prompt from device
static string _dev;
static struct termio _orig;
static int _theDevFd= 0;
static  pthread_t _thrdev, _thrdreq;

void sighandler(int sig) {
  clog<<"sighandler> caught: "<<sig<<endl;
  switch(sig) {
  case SIGTSTP:
    clog<<"sighandler> control-z handled as sigterm ..."<<endl;
    _terminate: true; 
    break;
  case SIGINT:
  case SIGTERM:
    _terminate = true;
    break;
  default: PosixRuntime::defaultSigHandler(sig);
    break;
  }
  if( _terminate ) {
    clog<<"sighandler> cancel device read thread: "<<_thrdev<<endl;
    ::pthread_cancel(_thrdev);
    clog<<"sighandler> cancel user input thread: "<<_thrdreq<<endl;
    ::pthread_cancel(_thrdreq); 
    clog<<"sighandler> closing: "<<_dev<<", fd: "<<_theDevFd<<endl;
#if !defined(CYGWIN)
    ::ioctl(_theDevFd, TCSETAF, &_orig);
#endif
    ::close(_theDevFd);
    clog<<"sighandler> exiting... "<<endl;
    ::exit(0);
  }
}

void* serialdev(void* p) {
  int fd = *( (int*) p);
  if( _verbose )
    clog<<"serialdev> reading "<<_dev<<" fd= "<<fd<<endl;

  string serial = "";
  int nb= 0;
  char c= 0, prev;
  while( true ) {
    prev = c;
    nb = ::read(fd, &c, 1); // block on device input
    if( nb <= 0 ) {
      clog<<"serialdev> read device failed."<<endl;
      _terminate = true;
      break;
    }
    if( _verbose && c == 6 ) clog<<"serialdev> got ACQ"<<endl;
    if( _verbose && c == 21 ) clog<<"serialdev> got NAK"<<endl;
    if( _verbose && c == '\r' ) clog<<"serialdev> got cr"<<endl;
    if( _verbose && c == '\n' ) clog<<"serialdev> got nl"<<endl;
    if( _enq && c == '\n' && prev == '\r' ) { // silently send <ENQ> == 5 back to device (probably a Pfeiffer)
      unsigned char enq= 5;
      ::write(_theDevFd, &enq, 1);
      if( _verbose )
        clog<<"serialdev> wrote ENQ" <<endl;
    }
    if( c == '\n' && prev == c ) continue; // no need for repeating \n ?
    if( c == '\r' ) continue; // no need to write out \r
    ::write(1, &c, 1);
  }

  clog<<"serialdev> closing: "<<_theDevFd<<endl;
#if !defined(CYGWIN)
  ::ioctl(_theDevFd, TCSETAF, &_orig);
#endif
  ::close(_theDevFd);
  clog<<"serialdev> exiting due to failed read ... "<<endl;
  ::exit(-1);

  return p;
}

void* usereq(void* p) {
  int fd = *( (int*) p);
  FILE* fdev = ::fdopen(fd, "w+");
  if( _verbose )
    clog<<"usereq> writing "<<_dev<<" fd= "<<fd<<endl;

  string cmd = "", cmdline = "";
  int nb= 0;
  char c= '\n';
  if( _cr )
    c = '\r'; // replace nl with cr

  if( _prompt ) // force device prompt:
    ::write(fd, &c, 1); // send it to serial dev.

  char r = 3;
  if( _reset ) // force device reset
    ::write(fd, &r, 1); // send it to serial dev.

  while( true ) {
    nb = ::read(0, &c, 1); // block on user input
    if( nb <= 0 ) {
      clog<<"usereq> read stdin failed."<<endl;
      _terminate = true;
      break;
    }
    cmd += c;
    if( c == '\n' ) {
      cmdline = cmd; 
      if( _verbose )
        clog<<"usereq> cmdline: "<<cmdline<<endl;
      cmd = "";
    }
    
    if( c == '\n' && ( _cr || _crlf ) ) 
      c = '\r'; // replace nl with cr

    nb = ::write(fd, &c, 1); // send it to serial dev.
    if( nb <= 0 ) {
      clog<<"usereq> write failed: "<<_dev<<endl;
      _terminate = true;
      break;
    }
    ::fflush(fdev); // try to flush the output (buffer)?
    if( _crlf ) { // sent \r, now do \n
      c = '\n';
      nb = ::write(fd, &c, 1); // send it to serial dev.
      if( nb <= 0 ) {
        clog<<"usereq> write failed: "<<_dev<<endl;
        _terminate = true;
        break;
      }
      ::fflush(fdev); // try to flush the output (buffer)?
    }
    /*
    if ( cmdline.find("quit") != string::npos || cmdline.find("exit") != string::npos ) {
      _terminate = true;
      break;
    }
    */
  }

  clog<<"usereq> closing: "<<_theDevFd<<endl;
#if !defined(CYGWIN)
  ::ioctl(_theDevFd, TCSETAF, &_orig);
#endif
  ::close(_theDevFd);
  clog<<"usereq> exiting due to failed write ... "<<endl;
  ::exit(-2);

  return p;
}

int main(int argc, char** argv) {
  Runtime::Argv args(argc, argv);
  string arg;

  arg = Runtime::argVal("-reset", args);
  if( arg == "true" ) 
    _reset = true;

  arg = Runtime::argVal("-v", args);
  if( arg == "true" ) 
    _verbose = true;

  arg = Runtime::argVal("-cr", args);
  if( arg == "true" ) 
    _cr = true;

  arg = Runtime::argVal("-enq", args);
  if( arg == "true" ) 
    _enq = true;

  arg = Runtime::argVal("-crlf", args);
  if( arg == "true" ) 
    _crlf = true;

  arg = Runtime::argVal("-prompt", args);
  if( arg != "false" ) 
   _prompt = true;

  // defaults for no arguments:
#if defined(Linux) || defined(LINUX)
  _dev = "/dev/ttyS0";
  if( strstr(argv[0], "tty") != 0 ) {
    _dev = "/dev/ttyS0";
    if( strstr(argv[0], "usb") != 0 ) 
      _dev = "/dev/ttyUSB0";
  }
  if( strstr(argv[0], "tty0") != 0 ) { 
    _dev = "/dev/ttyS0";
    if( strstr(argv[0], "usb") != 0 ) 
      _dev = "/dev/ttyUSB0";
  }
  if( strstr(argv[0], "tty1") != 0 ) {
    _dev = "/dev/ttyS1";
    if( strstr(argv[0], "usb") != 0 ) 
      _dev = "/dev/ttyUSB1";
  }
#else // Solaris
  _dev =  "/dev/term/a"; 
  if( strstr(argv[0], "ttyb") != 0 ) 
    _dev = "/dev/term/b";
#endif

  arg = Runtime::argVal("-dev", args);
  if( arg != "false" && arg != "true" ) 
    _dev = arg;

  arg = Runtime::argVal("-tty", args);
  if( arg != "false" && arg != "true" ) 
    _dev = arg;

  arg = Runtime::argVal("-0", args);
  if( arg != "false" ) 
    _dev = "/dev/ttyS0";

  arg = Runtime::argVal("-1", args);
  if( arg != "false" ) 
    _dev = "/dev/ttyS1";

  arg = Runtime::argVal("-a", args);
  if( arg != "false" ) 
    _dev = "/dev/term/a";

  arg = Runtime::argVal("-b", args);
  if( arg != "false" ) 
    _dev = "/dev/term/b";

  int bits = CS8;
  arg = Runtime::argVal("-7", args);
  if( arg != "false" ) 
    bits = CS7;

  int parity = 0; // none
  clog<<"uftty> default parity is none for devault device: "<<_dev<<endl;
  arg = Runtime::argVal("-even", args);
  if( arg != "false" ) {
    parity = PARENB; // default of enabling parity is even
  }
  arg = Runtime::argVal("-odd", args);
  if( arg != "false" ) {
    parity = PARENB | PARODD; // default of enabling parity is even, switch to odd
  }

  int baud = B9600;
  clog<<"uftty> default baud rate B9600 for devault device: "<<_dev<<endl;
  
  arg = Runtime::argVal("-19200", args);
  if( arg != "false" )
    baud = B19200;

  arg = Runtime::argVal("-s", args);
  if( arg != "false" && arg != "true" ) {
    int speed = atoi(arg.c_str());
    if( speed < 300 )
      baud = B200;
    else if( speed < 600 )
      baud = B300;
    else if( speed < 1200 )
      baud = B600;
    else if( speed < 1800 )
      baud = B1200;
    else if( speed < 2400 )
      baud = B1800;
    else if( speed < 4800 )
      baud = B2400;
    else if( speed < 9600 )
      baud = B4800;
    else if( speed < 19200 )
      baud = B9600;
    else if( speed < 38400 )
      baud = B19200;
    else if( speed < 57600 )
      baud = B38400;
    else if( speed < 115200 )
      baud = B57600;
    else
      baud = B115200;
    /*
    else if( speed < 150000 )
      baud = B115200;
    else if( speed < 200000 )
      baud = B150000;
    else if( speed < 250000 )
      baud = B200000;
    else if( speed < 300000 )
      baud = B250000;
    else if( speed < 350000 )
      baud = B300000;
    else if( speed < 400000 )
      baud = B350000;
    else
      baud = B400000; //  __MAX_BAUD
    */
  }
 
  int fd = ::open(_dev.c_str(), O_RDWR);
  clog<<"uftty> attempt to open serial line for read-write: "<<_dev<<endl;
  if( fd <= 0 ) {
    clog<<"uftty> serial line open failed: "<<_dev<<endl;
    return -1;
  }
  clog<<"uftty> serial line opened: "<<_dev<<endl;
  _theDevFd = fd; // for use by signalhandler on exit interrupt...

#if !defined(CYGWIN)
  static struct termio raw, stdinraw;
  int io = ::ioctl(fd, TCGETA, &_orig);
  io = ::ioctl(fd, TCGETA, &raw);
  raw.c_cflag = baud | bits | parity | CLOCAL | CREAD;
  raw.c_lflag &= ~ICANON;
  raw.c_lflag &= ~ECHO;
  raw.c_cc[VMIN] = 1; // present each character as soon as it shows up
  raw.c_cc[VTIME] = 0; // infinite timeout? = 1 for 0.1 sec. timout
  io = ::ioctl(fd, TCSETAF, &raw);

  // reset stdin
  io = ::ioctl(0, TCGETA, &stdinraw);
  stdinraw.c_cflag = CLOCAL | CREAD;
  stdinraw.c_lflag &= ~ICANON;
  stdinraw.c_lflag &= ~ECHO;
  stdinraw.c_cc[VMIN] = 1; // present each character as soon as it shows up
  stdinraw.c_cc[VTIME] = 0; // infinite timeout? = 1 for 0.1 sec. timout
  io = ::ioctl(0, TCSETAF, &stdinraw); // no echo for stdin
#endif

  // i/o loops:

  // this thread reads serial port and writes result to stdout
  _thrdev = PosixRuntime::newThread(serialdev, (void*) &fd);
  if( _thrdev <= 0 ) {
    clog<<"failed to start device recv. thread.";
    return 0;
  }

  // this thread reads userinput/req from stdin and writes it to serial port
  _thrdreq = PosixRuntime::newThread(usereq, (void*) &fd);
  if( _thrdreq <= 0 ) {
    clog<<"failed to start user req. thread.";
    return 0;
  }

  // wait for signals...
  PosixRuntime::sigWait((void*)sighandler);
  return 0;
}

#endif // __MYTTY_C__

