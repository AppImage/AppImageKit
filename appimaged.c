#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/inotify.h>
#include <signal.h>

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <dirent.h>

#include <glib.h>
#include <glib/gprintf.h>

/*
 * Compile with:
 * sudo apt-get -y install libglib2.0-dev
 * gcc appimaged.c $(pkg-config --cflags glib-2.0) $(pkg-config --libs glib-2.0) -o appimaged
 * 
 * Watch directories for AppImages and register/unregister them with the system
 * Partly based on example code from
 * http://www.ibm.com/developerworks/library/l-inotify/
 * 
 * TODO:
 * - Absolute paths - see https://github.com/jconerly/inotispy/blob/master/src/inotify.c
 * - Recursive direcory watch - https://github.com/ape-box/watchdog/blob/master/src/wdlib.c
 * - Also watch /tmp/.mount* and resolve to the launching AppImage
 * - Add and remove subdirectories on the fly
 * - Only watch for the events we are interested in
 */

/* This is the file descriptor for the inotify watch */
extern int inotify_fd;
int inotify_fd;

/* This is the watch descriptor returned for each item we are 
*        watching. A real application might keep these for some use 
*        in the application. This sample only makes sure that none of
*        the watch descriptors is less than 0.
*/
extern int wd;
int wd;
        
static gboolean verbose = FALSE;
gchar **remaining_args = NULL;

static GOptionEntry entries[] =
{
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &remaining_args, NULL },
    { NULL }
};

/* Register AppImage in the system */
int ai_register (char *cur_event_filename)
{
    printf ("-> REGISTER %s\n", cur_event_filename);
    return 0;
}

/* Unregister AppImage in the system */
int unai_register (char *cur_event_filename)
{
    printf ("-> UNREGISTER %s\n", cur_event_filename);
    return 0;
}

/* Simple queue implemented as a singly linked list with head 
 * and tail pointers maintained
 */

struct queue_entry
{
    struct queue_entry * next_ptr;   /* Pointer to next entry */
    struct inotify_event inot_ev;
};

typedef struct queue_entry * queue_entry_t;

/*struct queue_struct; */
struct queue_struct
{
    struct queue_entry * head;
    struct queue_entry * tail;
};
typedef struct queue_struct *queue_t;

int queue_empty (queue_t q)
{
    return q->head == NULL;
}

queue_t queue_create ()
{
    queue_t q;
    q = malloc (sizeof (struct queue_struct));
    if (q == NULL)
        exit (-1);
    
    q->head = q->tail = NULL;
    return q;
}

void queue_destroy (queue_t q)
{
    if (q != NULL)
    {
        while (q->head != NULL)
        {
            queue_entry_t next = q->head;
            q->head = next->next_ptr;
            next->next_ptr = NULL;
            free (next);
        }
        q->head = q->tail = NULL;
        free (q);
    }
}

void queue_enqueue (queue_entry_t d, queue_t q)
{
    d->next_ptr = NULL;
    if (q->tail)
    {
        q->tail->next_ptr = d;
        q->tail = d;
    }
    else
    {
        q->head = q->tail = d;
    }
}

queue_entry_t  queue_dequeue (queue_t q)
{
    queue_entry_t first = q->head;
    if (first)
    {
        q->head = first->next_ptr;
        if (q->head == NULL) 
        {
            q->tail = NULL;
        }
        first->next_ptr = NULL;
    }
    return first;
}

extern int keep_running;
static int watched_items;

/* Create an inotify instance and open a file descriptor
 *to access it */
int open_inotify_fd ()
{
    int fd;
    
    watched_items = 0;
    fd = inotify_init ();
    
    if (fd < 0)
    {
        perror ("inotify_init () = ");
    }
    return fd;
}

/* Close the open file descriptor that was opened with inotify_init() */
int close_inotify_fd (int fd)
{
    int r;
    
    if ((r = close (fd)) < 0)
    {
        perror ("close (fd) = ");
    }
    
    watched_items = 0;
    return r;
}

/* This method does the work of determining what happened,
 * then allows us to act appropriately
 */
void handle_event (queue_entry_t event)
{
    /* If the event was associated with a filename, we will store it here */
    char *cur_event_filename = NULL;
    char *cur_event_file_or_dir = NULL;
    /* This is the watch descriptor the event occurred on */
    int cur_event_wd = event->inot_ev.wd;
    int cur_event_cookie = event->inot_ev.cookie;
    unsigned long flags;
    
    if (event->inot_ev.len)
    {
        cur_event_filename = event->inot_ev.name;
    }
    if ( event->inot_ev.mask & IN_ISDIR )
    {
        cur_event_file_or_dir = "Dir";
    }
    else 
    {
        cur_event_file_or_dir = "File";
    }
    flags = event->inot_ev.mask & 
    ~(IN_ALL_EVENTS | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED );
    
    /* Perform event dependent handler routines */
    /* The mask is the magic that tells us what file operation occurred */
    switch (event->inot_ev.mask & 
        (IN_ALL_EVENTS | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED))
    {
        /* File was accessed */
        case IN_ACCESS:
            break;
            
        /* File was modified */
        case IN_MODIFY:
            break;
            
        /* File changed attributes */
        case IN_ATTRIB:
            break;
            
        /* File open for writing was closed */
        case IN_CLOSE_WRITE:
            if(verbose)
                printf ("CLOSE_WRITE: %s \"%s\"\n",
                        cur_event_file_or_dir, cur_event_filename);
                if(cur_event_file_or_dir == "File")
                    ai_register(cur_event_filename);
                break;
            
        /* File open read-only was closed */
        case IN_CLOSE_NOWRITE:
            break;
            
        /* File was opened */
        case IN_OPEN:
            break;
            
        /* File was moved from X */
        case IN_MOVED_FROM:
            if(verbose)
                printf ("MOVED_FROM: %s \"%s\". Cookie=%d\n",
                        cur_event_file_or_dir, cur_event_filename, 
                        cur_event_cookie);
                if(cur_event_file_or_dir == "File")
                    unai_register(cur_event_filename);
                break;
            
        /* File was moved to X */
        case IN_MOVED_TO:
            if(verbose)
                printf ("MOVED_TO: %s \"%s\". Cookie=%d\n",
                        cur_event_file_or_dir, cur_event_filename, 
                        cur_event_cookie);
                if(cur_event_file_or_dir == "File")
                    ai_register(cur_event_filename);
                break;
            
        /* Subdir or file was deleted */
        case IN_DELETE:
            if(verbose)
                printf ("DELETE: %s \"%s\"\n",
                        cur_event_file_or_dir, cur_event_filename);
                if(cur_event_file_or_dir == "File")
                    unai_register(cur_event_filename);
                break;
            
        /* Subdir or file was created */
        case IN_CREATE:
            if(cur_event_file_or_dir == "Dir"){
                if(verbose)
                    printf ("CREATE: %s \"%s\"\n",
                            cur_event_file_or_dir, cur_event_filename);
            }
            break;
            
        /* Watched entry was deleted */
        case IN_DELETE_SELF:
            break;
            
        /* Watched entry was moved */
        case IN_MOVE_SELF:
            break;
            
        /* Backing FS was unmounted */
        case IN_UNMOUNT:
            break;
            
        /* Too many FS events were received without reading them
            *        some event notifications were potentially lost.  */
        case IN_Q_OVERFLOW:
            if(verbose)
                printf ("Warning: AN OVERFLOW EVENT OCCURRED: \n");
            break;
            
        /* Watch was removed explicitly by inotify_rm_watch or automatically
            *        because file was deleted, or file system was unmounted.  */
        case IN_IGNORED:
            watched_items--;
            printf ("IGNORED: WD #%d\n", cur_event_wd);
            printf("Watching = %d items\n",watched_items); 
            break;
            
        /* Some unknown message received */
        default:
            printf ("UNKNOWN EVENT \"%X\" OCCURRED for file \"%s\"\n",
                    event->inot_ev.mask, cur_event_filename);
            break;
    }
    /* If any flags were set other than IN_ISDIR, report the flags */
    if (flags & (~IN_ISDIR))
    {
        flags = event->inot_ev.mask;
        printf ("Flags=%lX\n", flags);
    }
}

void handle_events (queue_t q)
{
    queue_entry_t event;
    while (!queue_empty (q))
    {
        event = queue_dequeue (q);
        handle_event (event);
        free (event);
    }
}

int read_events (queue_t q, int fd)
{
    char buffer[16384];
    size_t buffer_i;
    struct inotify_event *pevent;
    queue_entry_t event;
    ssize_t r;
    size_t event_size, q_event_size;
    int count = 0;
    
    r = read (fd, buffer, 16384);
    if (r <= 0)
        return r;
    buffer_i = 0;
    while (buffer_i < r)
    {
        /* Parse events and queue them. */
        pevent = (struct inotify_event *) &buffer[buffer_i];
        event_size =  offsetof (struct inotify_event, name) + pevent->len;
        q_event_size = offsetof (struct queue_entry, inot_ev.name) + pevent->len;
        event = malloc (q_event_size);
        memmove (&(event->inot_ev), pevent, event_size);
        queue_enqueue (event, q);
        buffer_i += event_size;
        count++;
    }
    return count;
}

int event_check (int fd)
{
    fd_set rfds;
    FD_ZERO (&rfds);
    FD_SET (fd, &rfds);
    /* Wait until an event happens or we get interrupted 
     *    by a signal that we catch */
    return select (FD_SETSIZE, &rfds, NULL, NULL, NULL);
}

int process_inotify_events (queue_t q, int fd)
{
    while (keep_running && (watched_items > 0))
    {
        if (event_check (fd) > 0)
        {
            int r;
            r = read_events (q, fd);
            if (r < 0)
            {
                break;
            }
            else
            {
                handle_events (q);
            }
        }
    }
    return 0;
}

int watch_dir (int fd, const char *dirname, unsigned long mask)
{
    int wd;
    wd = inotify_add_watch (fd, dirname, mask);
    if (wd < 0)
    {
        printf ("Cannot add watch for \"%s\" with event mask %lX", dirname,
                mask);
        fflush (stdout);
        perror (" ");
    }
    else
    {
        watched_items++;
        printf ("Watching %s WD=%d\n", dirname, wd);
        printf ("Watching = %d items\n", watched_items); 
    }
    return wd;
}

int ignore_wd (int fd, int wd)
{
    int r;
    r = inotify_rm_watch (fd, wd);
    if (r < 0)
    {
        perror ("inotify_rm_watch(fd, wd) = ");
    }
    else 
    {
        watched_items--;
    }
    return r;
}

int keep_running;

/* This program will take as arguments one or more directory 
 * or file names, and monitor them, printing event notifications 
 * to the console. It will automatically terminate if all watched
 * items are deleted or unmounted. Use ctrl-C or kill to 
 * terminate otherwise.
 */

/* Signal handler that simply resets a flag to cause termination */
void signal_handler (int signum)
{
    keep_running = 0;
}

int add_dir_to_watch(char *the_dir, int inotify_fd)
{
    int wd = watch_dir (inotify_fd, the_dir, IN_ALL_EVENTS);
    printf("Watching %s\n", the_dir);
    /* Upon launch, register all the files in the given directory */
    DIR *d;
    struct dirent *dir;
    d = opendir(the_dir);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_type == DT_REG)
            {
                gchar *ai;
                ai = g_strconcat (the_dir, dir->d_name, NULL);
                ai_register(ai);
            }
        }
        closedir(d);
    }
    return wd;
}
        
int main (int argc, char **argv)
{
    GError *error = NULL;
    GOptionContext *context;
    
    context = g_option_context_new ("");
    g_option_context_add_main_entries (context, entries, NULL);
    // g_option_context_add_group (context, gtk_get_option_group (TRUE));
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_print ("option parsing failed: %s\n", error->message);
        exit (1);
    }
    
    keep_running = 1;
    
    /* Set a ctrl-c signal handler */
    if (signal (SIGINT, signal_handler) == SIG_IGN)
    {
        /* Reset to SIG_IGN (ignore) if that was the prior state */
        signal (SIGINT, SIG_IGN);
    }
    
    /* First we open the inotify dev entry */
    inotify_fd = open_inotify_fd ();
    if (inotify_fd > 0)
    {
        
        /* We will need a place to enqueue inotify events,
         *        this is needed because if you do not read events
         *        fast enough, you will miss them. This queue is 
         *        probably too small if you are monitoring something
         *        like a directory with a lot of files and the directory 
         *        is deleted.
         */
        queue_t q;
        q = queue_create (128);
        
        /* Watch all events (IN_ALL_EVENTS) for the directories and 
         *        files passed in as arguments.
         *        Read the article for why you might want to alter this for 
         *        more efficient inotify use in your app.      
         */
        int index;
        wd = 0;
        printf("\n");
        unsigned long flags;
        
        gchar *dir;
        
        /* Watch hardcoded directories if they exist */
        dir=g_build_filename(g_get_home_dir(), "/Downloads", NULL);
        if (g_file_test (dir, G_FILE_TEST_IS_DIR))
             wd = add_dir_to_watch(dir, inotify_fd);

        dir=g_build_filename(g_get_home_dir(), "/bin", NULL);
        if (g_file_test (dir, G_FILE_TEST_IS_DIR))
             wd = add_dir_to_watch(dir, inotify_fd);        
        
        dir=g_build_filename(g_get_home_dir(), "/Applications", NULL);
        if (g_file_test (dir, G_FILE_TEST_IS_DIR))
             wd = add_dir_to_watch(dir, inotify_fd);
        
        dir=g_build_filename("/isodevice/Applications", NULL);
        if (g_file_test (dir, G_FILE_TEST_IS_DIR))
             wd = add_dir_to_watch(dir, inotify_fd);

        dir=g_build_filename("/usr/local/bin", NULL);
        if (g_file_test (dir, G_FILE_TEST_IS_DIR))
             wd = add_dir_to_watch(dir, inotify_fd);
           
        dir=g_build_filename("/opt", NULL);
        if (g_file_test (dir, G_FILE_TEST_IS_DIR))
             wd = add_dir_to_watch(dir, inotify_fd);
                   
        /* Watch additional directories that have been specified on the command line */
        if(remaining_args){
            for (index = 0; (index < g_strv_length(remaining_args)) && (wd >= 0); index++) 
            {
                wd = add_dir_to_watch(remaining_args[index], inotify_fd);
            }
        }
        if (wd > 0) 
        {
            process_inotify_events (q, inotify_fd);
        }
        printf ("\nTerminating\n");
        close_inotify_fd (inotify_fd);
        queue_destroy (q);
    }
    return 0;
}

