#include <syscall.h>
#include "../syscall-nr.h"

/* Invokes syscall NUMBER, passing no arguments, and returns the
   return value as an `int'. */
#define syscall0(NUMBER)                                        \
        ({                                                      \
          int retval;                                           \
          asm volatile                                          \
            ("pushl %[number]; int $0x30; addl $4, %%esp"       \
               : "=a" (retval)                                  \
               : [number] "i" (NUMBER)                          \
               : "memory");                                     \
          retval;                                               \
        })

/* Invokes syscall NUMBER, passing argument ARG0, and returns the
   return value as an `int'. */
#define syscall1(NUMBER, ARG0)                                           \
        ({                                                               \
          int retval;                                                    \
          asm volatile                                                   \
            ("pushl %[arg0]; pushl %[number]; int $0x30; addl $8, %%esp" \
               : "=a" (retval)                                           \
               : [number] "i" (NUMBER),                                  \
                 [arg0] "g" (ARG0)                                       \
               : "memory");                                              \
          retval;                                                        \
        })

/* Invokes syscall NUMBER, passing arguments ARG0 and ARG1, and
   returns the return value as an `int'. */
#define syscall2(NUMBER, ARG0, ARG1)                            \
        ({                                                      \
          int retval;                                           \
          asm volatile                                          \
            ("pushl %[arg1]; pushl %[arg0]; "                   \
             "pushl %[number]; int $0x30; addl $12, %%esp"      \
               : "=a" (retval)                                  \
               : [number] "i" (NUMBER),                         \
                 [arg0] "r" (ARG0),                             \
                 [arg1] "r" (ARG1)                              \
               : "memory");                                     \
          retval;                                               \
        })

/* Invokes syscall NUMBER, passing arguments ARG0, ARG1, and
   ARG2, and returns the return value as an `int'. */
#define syscall3(NUMBER, ARG0, ARG1, ARG2)                      \
        ({                                                      \
          int retval;                                           \
          asm volatile                                          \
            ("pushl %[arg2]; pushl %[arg1]; pushl %[arg0]; "    \
             "pushl %[number]; int $0x30; addl $16, %%esp"      \
               : "=a" (retval)                                  \
               : [number] "i" (NUMBER),                         \
                 [arg0] "r" (ARG0),                             \
                 [arg1] "r" (ARG1),                             \
                 [arg2] "r" (ARG2)                              \
               : "memory");                                     \
          retval;                                               \
        })

void
halt (void) 
{
  syscall0 (SYS_HALT);
  NOT_REACHED ();
}

void
exit (int status)
{
  syscall1 (SYS_EXIT, status);
  NOT_REACHED ();
}

pid_t
exec (const char *file)
{
  return (pid_t) syscall1 (SYS_EXEC, file);
}

int
wait (pid_t pid)
{
  return syscall1 (SYS_WAIT, pid);
}

bool
create (const char *file, unsigned initial_size)
{
  return syscall2 (SYS_CREATE, file, initial_size);
}

bool
remove (const char *file)
{
  return syscall1 (SYS_REMOVE, file);
}

int
open (const char *file)
{
  return syscall1 (SYS_OPEN, file);
}

int
filesize (int fd) 
{
  return syscall1 (SYS_FILESIZE, fd);
}

int
read (int fd, void *buffer, unsigned size)
{
  return syscall3 (SYS_READ, fd, buffer, size);
}

int
write (int fd, const void *buffer, unsigned size)
{
  return syscall3 (SYS_WRITE, fd, buffer, size);
}

void
seek (int fd, unsigned position) 
{
  syscall2 (SYS_SEEK, fd, position);
}

unsigned
tell (int fd) 
{
  return syscall1 (SYS_TELL, fd);
}

void
close (int fd)
{
  syscall1 (SYS_CLOSE, fd);
}

mapid_t
mmap (int fd, void *addr)
{
  return syscall2 (SYS_MMAP, fd, addr);
}

void
munmap (mapid_t mapid)
{
  syscall1 (SYS_MUNMAP, mapid);
}

bool
chdir (const char *dir)
{
  return syscall1 (SYS_CHDIR, dir);
}

bool
mkdir (const char *dir)
{
  return syscall1 (SYS_MKDIR, dir);
}

bool
readdir (int fd, char name[READDIR_MAX_LEN + 1]) 
{
  return syscall2 (SYS_READDIR, fd, name);
}

bool
isdir (int fd) 
{
  return syscall1 (SYS_ISDIR, fd);
}

int
inumber (int fd) 
{
  return syscall1 (SYS_INUMBER, fd);
}

tid_t 
sys_pthread_create (wrapper_func wf, start_routine sr, void *args) {
  return syscall3 (SYS_PTHREAD_CREATE, wf, sr, args);
}

void 
sys_pthread_exit () {
  syscall0(SYS_PTHREAD_EXIT);
  NOT_REACHED();
}

bool 
sys_pthread_join (tid_t tid) {
  return syscall1(SYS_PTHREAD_JOIN, tid);
}

void *
sbrk (intptr_t increment) {
  return (void *) syscall1(SYS_SBRK, increment);
}

pthread_lock_t lock_init(void){
  return syscall0(SYS_INITLOCK);
}

void lock_acquire(pthread_lock_t lock){
  syscall1(SYS_LOCK, lock);
}

void lock_release(pthread_lock_t lock){
  syscall1(SYS_UNLOCK, lock);
}

pthread_sema_t sema_init(int value){
  return syscall1(SYS_INITSEMA, value);
}

void sema_up(pthread_sema_t sema){
  syscall1(SYS_SEMAUP, sema);
}

void sema_down(pthread_sema_t sema){
  syscall1(SYS_SEMADOWN, sema);
}

pthread_cond_t cond_init() {
    return syscall0(SYS_INITCOND);
}

void cond_wait(pthread_cond_t cond, pthread_lock_t lock) {
    syscall2(SYS_COND_WAIT, cond, lock);
}

void cond_signal(pthread_cond_t cond) {
    syscall1(SYS_COND_SIGNAL, cond);
}

void futex_wait(void * addr) {
    syscall1(SYS_FUTEX_WAIT, addr);
}

void futex_wake(void * addr, int val) {
    syscall2(SYS_FUTEX_WAIT, addr, val);
}