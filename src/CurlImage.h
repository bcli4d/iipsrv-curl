/*
    Curl Image Class

    Copyright (C) 2014-2015 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/


#ifndef _CURLIMAGE_H
#define _CURLIMAGE_H

#include <cstdio>
#include <tiffio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <curl/curl.h>

#include "CurlSession.h"

/// Simple utility class to decode and filter URLs

struct MemoryStruct {
  char *memory;
  size_t size;
  size_t buf_size;
};

class CurlImage{

 private:

/*
  ///libcurl session handle
  CURL curl;

  ///Remote file handle
  RemoteHdl hdl;
*/
  
  /// Filename of this image
  char *filename;

  /// Seek offset
  uint offset;

  /// Curl session handle
  CURL *curl_handle;

  /// True if file is remote
  boolean isRemote;

  /// Handle to be used when file is local
  FILE * local_handle;

  /// Utility function to print bytes read from remote file
  void print_bytes(const void *object, size_t size);
    
  /// curl callback function to copy data from curl buffer to our buffer
  size_t copy_data(void *buffer, size_t size, size_t nmemb, void *userp);
  
  /// Read from a remote file
  tmsize_t curlReadProc(thandle_t hdl, void * buf, tmsize_t size);

  /// Write to a remote file
  tmsize_t curlWriteProc(thandle_t hdl, void * buf, tmsize_t size);

  /// Return size of a remote file
  tmsize_t curlSizeProc(thandle_t hdl);

  /// Seek in a remote file
  toff_t curlSeekProc(thandle_t hdl, toff_t offset, int whence);

  /// Close a remote file
  int  curlCloseProc(thandle_t hdl);

 public:

  /// Constructor
  CurlImage(const std::string& s)
   : filename( s ),
    offset( 0 ),
    curl_handle( 0 ),
    isRemote(false),
    remote_handle(0) {};
    
  };

  /// Open a TIFF file. File might be local or remote
  TIFF * TIFFOpen(const char* filename, const char* mode);

  /// Open a possibly remote file.
  FILE open(const char *pathname, const char *mode);
  
  /// Read from a possibly remote file.
  size_t read(void *ptr, size_t size, size_t nmemb, curl_handle *stream);

  /// Close a possible remote file.
  int close(curl_handle *stream);

  /// Get file status of a possibly remote file.
  int stat(const char *pathname, struct stat *buf);
  
  /// Return any warning message
  std::string warning(){ return warning_message; };

};

#endif
