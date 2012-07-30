/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <glib/gi18n.h>
#include "gvfsjobstopmountable.h"
#include "gvfsdbusutils.h"
#include "gvfsdaemonutils.h"

G_DEFINE_TYPE (GVfsJobStopMountable, g_vfs_job_stop_mountable, G_VFS_TYPE_JOB_DBUS)

static void         run          (GVfsJob        *job);
static gboolean     try          (GVfsJob        *job);
static DBusMessage *create_reply (GVfsJob        *job,
				  DBusConnection *connection,
				  DBusMessage    *message);

static void
g_vfs_job_stop_mountable_finalize (GObject *object)
{
  GVfsJobStopMountable *job;

  job = G_VFS_JOB_STOP_MOUNTABLE (object);

  if (job->mount_source)
    g_object_unref (job->mount_source);

  g_free (job->filename);
  
  if (G_OBJECT_CLASS (g_vfs_job_stop_mountable_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_vfs_job_stop_mountable_parent_class)->finalize) (object);
}

static void
g_vfs_job_stop_mountable_class_init (GVfsJobStopMountableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GVfsJobClass *job_class = G_VFS_JOB_CLASS (klass);
  GVfsJobDBusClass *job_dbus_class = G_VFS_JOB_DBUS_CLASS (klass);
  
  gobject_class->finalize = g_vfs_job_stop_mountable_finalize;
  job_class->run = run;
  job_class->try = try;
  job_dbus_class->create_reply = create_reply;
}

static void
g_vfs_job_stop_mountable_init (GVfsJobStopMountable *job)
{
}

GVfsJob *
g_vfs_job_stop_mountable_new (DBusConnection *connection,
                              DBusMessage *message,
                              GVfsBackend *backend)
{
  GVfsJobStopMountable *job;
  DBusMessage *reply;
  DBusMessageIter iter;
  DBusError derror;
  char *path;
  const char *dbus_id, *obj_path;
  guint32 flags;
  
  dbus_error_init (&derror);
  dbus_message_iter_init (message, &iter);

  path = NULL;
  if (!_g_dbus_message_iter_get_args (&iter, &derror, 
				      G_DBUS_TYPE_CSTRING, &path,
				      DBUS_TYPE_UINT32, &flags,
                                      DBUS_TYPE_STRING, &dbus_id,
                                      DBUS_TYPE_OBJECT_PATH, &obj_path,
				      0))
    {
      g_free (path);
      reply = dbus_message_new_error (message,
				      derror.name,
                                      derror.message);
      dbus_error_free (&derror);

      dbus_connection_send (connection, reply, NULL);
      dbus_message_unref (reply);
      return NULL;
    }

  job = g_object_new (G_VFS_TYPE_JOB_STOP_MOUNTABLE,
		      "message", message,
		      "connection", connection,
		      NULL);

  job->filename = path;
  job->backend = backend;
  job->mount_source = g_mount_source_new (dbus_id, obj_path);
  job->flags = flags;
  
  return G_VFS_JOB (job);
}

static void
run (GVfsJob *job)
{
  GVfsJobStopMountable *op_job = G_VFS_JOB_STOP_MOUNTABLE (job);
  GVfsBackendClass *class = G_VFS_BACKEND_GET_CLASS (op_job->backend);

  if (class->stop_mountable == NULL)
    {
      g_vfs_job_failed (job, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                        _("Operation not supported by backend"));
      return;
    }

  class->stop_mountable (op_job->backend,
                         op_job,
                         op_job->filename,
                         op_job->flags,
                         op_job->mount_source);
}

static gboolean
try (GVfsJob *job)
{
  GVfsJobStopMountable *op_job = G_VFS_JOB_STOP_MOUNTABLE (job);
  GVfsBackendClass *class = G_VFS_BACKEND_GET_CLASS (op_job->backend);

  if (class->try_stop_mountable == NULL)
    return FALSE;

  return class->try_stop_mountable (op_job->backend,
                                    op_job,
                                    op_job->filename,
                                    op_job->flags,
                                    op_job->mount_source);
}

/* Might be called on an i/o thread */
static DBusMessage *
create_reply (GVfsJob *job,
	      DBusConnection *connection,
	      DBusMessage *message)
{
  DBusMessage *reply;

  reply = dbus_message_new_method_return (message);
  
  return reply;
}
