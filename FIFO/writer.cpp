#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

#define PIPENAME "/tmp/my_pype"
char empty[] = {"Empty file(0 symbols)"};

int FileOpen(const char * filename, int flags)
{
  int fd;
  if ((fd = open(filename, flags)) < 0)
  {
    printf("Cannot open file: %s", filename);
    exit(EXIT_FAILURE);
  }
  return fd;
}

ssize_t ReadFile(int fd, void *buf, size_t count)
{
  errno = 0;
  ssize_t read_ret;
  if ((read_ret = read(fd, buf, count)) < 0) //&& (errno != EAGAIN))
  {
    printf("Cannot read from file descriptor: %d\n", fd);
    exit(EXIT_FAILURE);
  }
  return read_ret;
}

ssize_t WriteFile(int fd, const void *buf, size_t count)
{
  errno = 0;
  ssize_t wrt_ret;
  if ((wrt_ret = write(fd, buf, count)) < 0) //&& (errno != EAGAIN))
  {
    printf("Cannot write to file descriptor: %d\n", fd);
    exit(EXIT_FAILURE);
  }

  return wrt_ret;
}

void FifoCreate(const char * fifoname, mode_t mode)
{
  (void)umask(0);
  if (mkfifo(fifoname, mode) < 0 && errno != EEXIST)
  {
    printf("Cannot create FIFO: %s\n (not EEXIST)", fifoname);
    exit(EXIT_FAILURE);
  }
}

template <typename T> T* BuffAlloc (int size, T type)
{
  T* buff = (T*)calloc(size, sizeof(type));
  if (buff == NULL)
  {
    printf("Cannot allocate buffer\n");
    exit(EXIT_FAILURE);
  }
  return buff;
}

char * FifoName(pid_t pid)
{
  char * buff = BuffAlloc(20, 'c');
  memset(buff, '\0', 20);

  sprintf(buff, "%d", pid);
  strcat(buff, ".p");

  return buff;
}

ssize_t PrintText(char * buff, ssize_t buf_sz)
{
  assert(buff != NULL);

  ssize_t wrt_ret = WriteFile(STDOUT_FILENO, buff, buf_sz*sizeof(buff[0]));

  return wrt_ret;
}

void DisableNONBLOCK(int fd)
{
  int flags;

  flags = fcntl(fd, F_GETFL);
  flags &= ~O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);
}

int main(int argc, char const *argv[])
{
  char * buff = BuffAlloc(256, 'c');

  FifoCreate(PIPENAME, 0666);
  int pipe_fd = FileOpen(PIPENAME, O_WRONLY);

  pid_t my_pid = getpid();

  char * fifo_name = FifoName(my_pid);
  FifoCreate(fifo_name, 0666);
  int fifo_fd = FileOpen(fifo_name, O_RDONLY | O_NONBLOCK);

  WriteFile(pipe_fd, &my_pid, sizeof(my_pid));

  ssize_t read_ret;
  int i = 0;
  for(; i < 3; i++)
  {
    errno = 0;
    read_ret = ReadFile(fifo_fd, buff, 256*sizeof(buff[0]));

    if (read_ret > 0)
    {
      PrintText(buff, read_ret);
      break;
    }

    sleep(1);
  }

  if (i == 3 && errno == EPIPE)
  {
    printf("Cannot connect to FIFO named %s\n", fifo_name);
    exit(EXIT_FAILURE);
  }
  else if (i == 3)
  {
    printf("File is empty!\n");
    exit(EXIT_SUCCESS);
  }

  DisableNONBLOCK(fifo_fd);

  do
  {
    errno = 0;
    read_ret = ReadFile(fifo_fd, buff, 256*sizeof(buff[0]));
    if (read_ret == 0) break;

    PrintText(buff, read_ret);

  } while (read_ret > 0);

  close(pipe_fd);
  close(fifo_fd);
  remove(PIPENAME);
  remove(fifo_name);
  free(buff);
  free(fifo_name);

  return 0;
}
