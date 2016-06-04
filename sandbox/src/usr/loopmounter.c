/*
sudo chown root:root ./a.out
sudo chmod u+s ./a.out
./a.out

http://stackoverflow.com/questions/556194/calling-a-script-from-a-setuid-root-c-program-script-does-not-run-as-root#comment7314622_2779643
Do not use system() from a program with set-user-ID or set-group-ID privileges,
because strange values for some environment variables might be used to subvert
system integrity. Use the exec(3) family of functions instead,
but not execlp(3) or execvp(3).

*/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/mount.h>

int main(int argc, char* argv[]) {
    int i;
	  printf("argc: %d\n", argc);
    for(i=0; i < argc; i++) {
      printf("argv[%d]: %s\n", i, argv[i]);
	  }
    setuid(0);

    // As root, we loop-mount or unmount
    int status;
    pid_t pid;

    pid = fork ();
    if (pid == 0)
      {
        /* This is the child process. */
        if(argc == 2) execl ("/bin/umount", "/bin/umount", argv[1], NULL);
        if(argc == 3) execl ("/bin/mount", "/bin/mount", "-o", "loop,ro", argv[1], argv[2], NULL);
        _exit (EXIT_FAILURE);
      }
    else if (pid < 0)
      /* The fork failed.  Report failure.  */
      status = -1;
    else
      /* This is the parent process.  Wait for the child to complete.  */
      if (waitpid (pid, &status, 0) != pid)
        status = -1;
}
