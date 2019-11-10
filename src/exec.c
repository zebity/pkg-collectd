/**
 * collectd - src/exec.c
 * Copyright (C) 2007  Florian octo Forster
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Florian octo Forster <octo at verplant.org>
 **/

#include "collectd.h"
#include "common.h"
#include "plugin.h"

#include <sys/types.h>
#include <pwd.h>
#include <signal.h>

#include <pthread.h>

/*
 * Private data types
 */
struct program_list_s;
typedef struct program_list_s program_list_t;
struct program_list_s
{
  char           *user;
  char           *exec;
  int             pid;
  program_list_t *next;
};

/*
 * Private variables
 */
static const char *config_keys[] =
{
  "Exec"
};
static int config_keys_num = STATIC_ARRAY_SIZE (config_keys);

static program_list_t *pl_head = NULL;

/*
 * Functions
 */
static int exec_config (const char *key, const char *value)
{
  if (strcasecmp ("Exec", key) == 0)
  {
    program_list_t *pl;
    pl = (program_list_t *) malloc (sizeof (program_list_t));
    if (pl == NULL)
      return (1);
    memset (pl, '\0', sizeof (program_list_t));

    pl->user = strdup (value);
    if (pl->user == NULL)
    {
      sfree (pl);
      return (1);
    }

    pl->exec = strchr (pl->user, ' ');
    if (pl->exec == NULL)
    {
      sfree (pl->user);
      sfree (pl);
      return (1);
    }
    while (*pl->exec == ' ')
    {
      *pl->exec = '\0';
      pl->exec++;
    }

    if (*pl->exec == '\0')
    {
      sfree (pl->user);
      sfree (pl);
      return (1);
    }

    pl->next = pl_head;
    pl_head = pl;
  }
  else
  {
    return (-1);
  }

  return (0);
} /* int exec_config */

static void exec_child (program_list_t *pl)
{
  int status;
  int uid;
  char *arg0;

  struct passwd *sp_ptr;
  struct passwd sp;
  char pwnambuf[2048];
  char errbuf[1024];

  sp_ptr = NULL;
  status = getpwnam_r (pl->user, &sp, pwnambuf, sizeof (pwnambuf), &sp_ptr);
  if (status != 0)
  {
    ERROR ("exec plugin: getpwnam_r failed: %s",
	sstrerror (errno, errbuf, sizeof (errbuf)));
    exit (-1);
  }
  if (sp_ptr == NULL)
  {
    ERROR ("exec plugin: No such user: `%s'", pl->user);
    exit (-1);
  }

  uid = sp.pw_uid;
  if (uid == 0)
  {
    ERROR ("exec plugin: Cowardly refusing to exec program as root.");
    exit (-1);
  }

  status = setuid (uid);
  if (status != 0)
  {
    ERROR ("exec plugin: setuid failed: %s",
	sstrerror (errno, errbuf, sizeof (errbuf)));
    exit (-1);
  }

  arg0 = strrchr (pl->exec, '/');
  if (arg0 != NULL)
    arg0++;
  if ((arg0 == NULL) || (*arg0 == '\0'))
    arg0 = pl->exec;

  status = execlp (pl->exec, arg0, (char *) 0);

  ERROR ("exec plugin: exec failed: %s",
      sstrerror (errno, errbuf, sizeof (errbuf)));
  exit (-1);
} /* void exec_child */

static int fork_child (program_list_t *pl)
{
  int fd_pipe[2];
  int status;

  if (pl->pid != 0)
    return (-1);

  status = pipe (fd_pipe);
  if (status != 0)
  {
    char errbuf[1024];
    ERROR ("exec plugin: pipe failed: %s",
	sstrerror (errno, errbuf, sizeof (errbuf)));
    return (-1);
  }

  pl->pid = fork ();
  if (pl->pid < 0)
  {
    char errbuf[1024];
    ERROR ("exec plugin: fork failed: %s",
	sstrerror (errno, errbuf, sizeof (errbuf)));
    return (-1);
  }
  else if (pl->pid == 0)
  {
    close (fd_pipe[0]);

    /* Connect the pipe to STDOUT and STDERR */
    if (fd_pipe[1] != STDOUT_FILENO)
      dup2 (fd_pipe[1], STDOUT_FILENO);
    if (fd_pipe[1] != STDERR_FILENO)
      dup2 (fd_pipe[1], STDERR_FILENO);
    if ((fd_pipe[1] != STDOUT_FILENO) && (fd_pipe[1] != STDERR_FILENO))
      close (fd_pipe[1]);

    exec_child (pl);
    /* does not return */
  }

  close (fd_pipe[1]);
  return (fd_pipe[0]);
} /* int fork_child */

static int parse_line (char *buffer)
{
  char *fields[4];
  int fields_num;

  char *hostname;
  char *plugin;
  char *plugin_instance;
  char *type;
  char *type_instance;

  const data_set_t *ds;
  value_list_t vl = VALUE_LIST_INIT;

  int status;

  fields_num = strsplit (buffer, fields, 4);
  if (fields_num != 2)
  {
    WARNING ("exec plugin: Number of fields is not 2.");
    return (-1);
  }

  status = parse_identifier (fields[0], &hostname,
      &plugin, &plugin_instance,
      &type, &type_instance);
  if (status != 0)
  {
    WARNING ("exec plugin: Cannot parse `%s'", fields[0]);
    return (-1);
  }

  if ((strlen (hostname) >= sizeof (vl.host))
      || (strlen (plugin) >= sizeof (vl.plugin))
      || ((plugin_instance != NULL)
	&& (strlen (plugin_instance) >= sizeof (vl.plugin_instance)))
      || ((type_instance != NULL)
	&& (strlen (type_instance) >= sizeof (vl.type_instance))))
  {
    WARNING ("exec plugin: An identifier is too long.");
    return (-1);
  }

  strcpy (vl.host, hostname);
  strcpy (vl.plugin, plugin);
  if (plugin_instance != NULL)
    strcpy (vl.plugin_instance, plugin_instance);
  if (type_instance != NULL)
    strcpy (vl.type_instance, type_instance);

  ds = plugin_get_ds (type);
  if (ds == NULL)
  {
    WARNING ("exec plugin: No such type: `%s'", type);
    return (-1);
  }

  vl.values_len = ds->ds_num;
  vl.values = (value_t *) malloc (sizeof (value_t) * vl.values_len);
  if (vl.values == NULL)
    return (-1);

  /* Sets vl.values and vl.time */
  status = parse_values (fields[1], &vl, ds);
  if (status != 0)
  {
    WARNING ("exec plugin: Cannot parse `%s'", fields[1]);
    sfree (vl.values);
    return (-1);
  }

  plugin_dispatch_values (type, &vl);

  sfree (vl.values);
  
  return (0);
} /* int parse_line */

static void *exec_read_one (void *arg)
{
  program_list_t *pl = (program_list_t *) arg;
  int fd;
  FILE *fh;
  char buffer[1024];

  fd = fork_child (pl);
  if (fd < 0)
    pthread_exit ((void *) 1);

  assert (pl->pid != 0);

  fh = fdopen (fd, "r");
  if (fh == NULL)
  {
    char errbuf[1024];
    ERROR ("exec plugin: fdopen (%i) failed: %s", fd,
	sstrerror (errno, errbuf, sizeof (errbuf)));
    kill (pl->pid, SIGTERM);
    close (fd);
    pthread_exit ((void *) 1);
  }

  while (fgets (buffer, sizeof (buffer), fh) != NULL)
  {
    int len;

    len = strlen (buffer);

    /* Remove newline from end. */
    while ((len > 0) && ((buffer[len - 1] == '\n')
	  || (buffer[len - 1] == '\r')))
      buffer[--len] = '\0';

    DEBUG ("exec plugin: exec_read_one: buffer = %s", buffer);

    parse_line (buffer);
  } /* while (fgets) */

  fclose (fh);
  pl->pid = 0;

  pthread_exit ((void *) 0);
  return (NULL);
} /* void *exec_read_one */

static int exec_read (void)
{
  program_list_t *pl;

  for (pl = pl_head; pl != NULL; pl = pl->next)
  {
    pthread_t t;
    pthread_attr_t attr;

    if (pl->pid != 0)
      continue;

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
    pthread_create (&t, &attr, exec_read_one, (void *) pl);
  } /* for (pl) */

  return (0);
} /* int exec_read */

static int exec_shutdown (void)
{
  program_list_t *pl;
  program_list_t *next;

  pl = pl_head;
  while (pl != NULL)
  {
    next = pl->next;

    if (pl->pid > 0)
    {
      kill (pl->pid, SIGTERM);
      INFO ("exec plugin: Sent SIGTERM to %hu", (unsigned short int) pl->pid);
    }

    sfree (pl->user);
    sfree (pl);

    pl = next;
  } /* while (pl) */
  pl_head = NULL;

  return (0);
} /* int exec_shutdown */

void module_register (void)
{
  plugin_register_config ("exec", exec_config, config_keys, config_keys_num);
  plugin_register_read ("exec", exec_read);
  plugin_register_shutdown ("exec", exec_shutdown);
} /* void module_register */

/*
 * vim:shiftwidth=2:softtabstop=2:tabstop=8
 */
