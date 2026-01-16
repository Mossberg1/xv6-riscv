// init: The initial user-level program

#include <types.h>
#include <stat.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <fs.h>
#include <file.h>
#include <user.h>
#include <fcntl.h>

char *sh_argv[] = { "sh", 0 };
char *desktop_argv[] = { "desktop", 0 };

int
main(void)
{
  int wpid;
  int sh_pid = 0;
  int desktop_pid = 0;

  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    // Run sh
    if (sh_pid == 0) 
    {
      printf("init: starting sh\n");
      sh_pid = fork();
      if(sh_pid < 0){
        printf("init: fork failed\n");
        exit(1);
      }
      if(sh_pid == 0){
        exec("sh", sh_argv);
        printf("init: exec sh failed\n");
        exit(1);
      }
    }

    // Run desktop environment
    if (desktop_pid == 0) 
    {
      desktop_pid = fork();
      if (desktop_pid < 0)
      {
        printf("Init: fork desktop failed\n");
      }

      if (desktop_pid == 0) 
      {
        exec("desktop", desktop_argv);
        printf("Init: exec desktop failed\n");
        exit(1);
      } 
    }

    wpid = wait((int*)0);

    if (wpid == sh_pid) 
    {
      sh_pid = 0;
    }
    else if (wpid == desktop_pid) 
    {
      printf("Init: desktop exited\n");
      desktop_pid = 0;
    }
  }
}
