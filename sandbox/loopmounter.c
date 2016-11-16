/*

Code review of this file is wanted.
Please open an issue if you find something insecure.
This file is intentionally kept short to allow for effective code review.

FIXME: Address privilege escalation:
https://github.com/probonopd/AppImageKit/issues/210

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
#include <unistd.h>
#include <stdlib.h>
#include <sys/mount.h>

int main(int argc, char* argv[]) {

  // Become root
  setuid(0);

  // As root, unmount if we get 1 argument
  if(argc == 2)
  {
    if (0 == umount2(argv[1], MNT_DETACH | UMOUNT_NOFOLLOW)) {
      return EXIT_SUCCESS;
    } else {
      return EXIT_FAILURE;
    }
  }

  // As root, mount if we get 2 arguments
  // TODO: Replace the call to "mount" with mount(2)
  // But it doesn't seem to do loop-mounting so this might get complex
  if(argc == 3)
  {
    if( access( "/bin/mount", F_OK ) != -1 ) {
      execl ("/bin/mount", "/bin/mount", "-o", "loop,ro", argv[1], argv[2], NULL);
    } else if( access( "/usr/bin/mount", F_OK ) != -1 ) {
      execl ("/usr/bin/mount", "/usr/bin/mount", "-o", "loop,ro", argv[1], argv[2], NULL);
    }
  }

}
