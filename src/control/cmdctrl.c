/*
 * Copyright (C) 2020  Joshua Minter
 *
 * This file is part of ReplaySorcery.
 *
 * ReplaySorcery is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ReplaySorcery is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ReplaySorcery.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "../util.h"
#include "control.h"
#include "rsbuild.h"
#include <stdio.h>
#ifdef RS_BUILD_UNIX_SOCKET_FOUND
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

typedef struct CommandControl {
   int fd;
} CommandControl;

#ifdef RS_BUILD_UNIX_SOCKET_FOUND
static void commandControlDestroy(RSControl *control) {
   CommandControl *command = control->extra;
   if (command != NULL) {
      if (command->fd) {
         close(command->fd);
      }
      remove(RS_COMMAND_CONTROL_PATH);
      av_freep(&control->extra);
   }
}

static int commandControlWantsSave(RSControl *control) {
   int ret = 0;
   CommandControl *command = control->extra;
   for (;;) {
      int fd = accept(command->fd, NULL, NULL);
      if (fd == -1) {
         if (errno != EAGAIN) {
            ret = AVERROR(errno);
            av_log(NULL, AV_LOG_ERROR, "Failed to accept connection: %s\n",
                   av_err2str(ret));
         }
         return ret;
      }
      ret = 1;
      close(fd);
   }
}
#endif

int rsCommandControlCreate(RSControl *control) {
#ifdef RS_BUILD_UNIX_SOCKET_FOUND
   int ret;
   CommandControl *command = av_mallocz(sizeof(CommandControl));
   control->extra = command;
   control->destroy = commandControlDestroy;
   control->wantsSave = commandControlWantsSave;
   if (command == NULL) {
      ret = AVERROR(ENOMEM);
      goto error;
   }
   if ((ret = rsDirectoryCreate(RS_COMMAND_CONTROL_PATH)) < 0) {
      goto error;
   }

   command->fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
   if (command->fd == -1) {
      ret = AVERROR(errno);
      av_log(NULL, AV_LOG_ERROR, "Failed to create socket: %s\n", av_err2str(ret));
      goto error;
   }

   struct sockaddr_un addr = {.sun_family = AF_UNIX, .sun_path = RS_COMMAND_CONTROL_PATH};
   if (bind(command->fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      ret = AVERROR(errno);
      av_log(NULL, AV_LOG_ERROR, "Failed to bind socket: %s\n", av_err2str(ret));
      goto error;
   }
   listen(command->fd, 1);

   return 0;
error:
   rsControlDestroy(control);
   return ret;

#else
   (void)control;
   av_log(NULL, AV_LOG_ERROR, "Unix socket was not found during compilation\n");
   return AVERROR(ENOSYS);
#endif
}
