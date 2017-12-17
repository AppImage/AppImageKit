#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>

/* Try to show a notification on the GUI, fall back to the command line
 * timeout is the timeout in milliseconds. timeout = NULL seems to trigger a
 * GUI error dialog rather than a notification */
int notify(char *title, char *body, int timeout)
{
    /* http://stackoverflow.com/questions/13204177/how-to-find-out-if-running-from-terminal-or-gui */
    if (isatty(fileno(stdin))){
        /* We were launched from the command line. */
        printf("\n%s\n", title);
        printf("%s\n", body);
    }
    else
    {
        /* We were launched from inside the desktop */
        printf("\n%s\n", title);
        printf("%s\n", body);
        /* https://debian-administration.org/article/407/Creating_desktop_notifications */
        void *handle, *n;
        typedef void  (*notify_init_t)(char *);
        typedef void *(*notify_notification_new_t)( char *, char *, char *, char *);
        typedef void  (*notify_notification_set_timeout_t)( void *, int );
        typedef void (*notify_notification_show_t)(void *, char *);
        handle = NULL;
        if(handle == NULL)
            handle= dlopen("libnotify.so.3", RTLD_LAZY);
        if(handle == NULL)
            handle= dlopen("libnotify.so.4", RTLD_LAZY);
        if(handle == NULL)
            handle= dlopen("libnotify.so.5", RTLD_LAZY);
        if(handle == NULL)
            handle= dlopen("libnotify.so.6", RTLD_LAZY);
        if(handle == NULL)
            handle= dlopen("libnotify.so.7", RTLD_LAZY);
        if(handle == NULL)
            handle= dlopen("libnotify.so.8", RTLD_LAZY);
        
        if(handle == NULL)
        {
            printf("Failed to open libnotify.\n\n" );
        }
        notify_init_t init = (notify_init_t)dlsym(handle, "notify_init");
        if ( init == NULL  )
        {
            dlclose( handle );
            return 1;
        }
        init("AppImage");
        
        notify_notification_new_t nnn = (notify_notification_new_t)dlsym(handle, "notify_notification_new");
        if ( nnn == NULL  )
        {
            dlclose( handle );
            return 1;
        }
        n = nnn(title, body, NULL, NULL);
        notify_notification_set_timeout_t nnst = (notify_notification_set_timeout_t)dlsym(handle, "notify_notification_set_timeout");
        if ( nnst == NULL  )
        {
            dlclose( handle );
            return 1;
        }
        nnst(n, timeout); 
        notify_notification_show_t show = (notify_notification_show_t)dlsym(handle, "notify_notification_show");
        if ( init == NULL  )
        {
            dlclose( handle );
            return 1;
        }
        show(n, NULL );
        dlclose(handle );
    }
    return 0;
}

/*
int main( int argc, char *argv[] )
{
    char *title;
    char *body;
    printf("%s\n", "Test");
    title = "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua.";
    body = "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.";
    notify(title, body, 3000); // 3 seconds timeout
    return 0;
}
*/
