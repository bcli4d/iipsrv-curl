// Subclass of IIPImage to support remote images

/*  IIP fcgi server module

    Copyright (C) 2000-2016 Ruven Pillay.

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


#ifndef _REMIIPIMAGE_H
#define _REMIIPIMAGE_H


// Fix missing snprintf in Windows
#if _MSC_VER
#define snprintf _snprintf
#endif


#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdexcept>
#include <tiff.h>
#include "RawTile.h"


/// Define our own derived exception class for file errors
class file_error : public std::runtime_error {
 public:
  file_error(std::string s) : std::runtime_error(s) { }
};


class RemIIPImage : public IIPImage {

 private:

  /// Seek offset
  uint offset;

  /// libcurl session handle
  CURL *curl_handle;

  /// True if file is remote
  bool isRemote;

  /// Handle to be used when file is local
  FILE * local_handle;

  /// Check if a file exists and return its mod time
  int StatProc(const char *pathname, struct stat *buf);

  /// curl callback function to copy data from curl buffer to our buffer
  size_t copy_data(void *buffer, size_t size, size_t nmemb, void *userp);

  /// No-op needed by curlStatProc
  size_t throw_away(void *ptr, size_t size, size_t nmemb, void *data);

  /// Read from a remote file
  tmsize_t ReadProc(thandle_t hdl, void * buf, tmsize_t size);

  /// Write to a remote file
  tmsize_t WriteProc(thandle_t hdl, void * buf, tmsize_t size);

  /// Return size of a remote file
  tmsize_t SizeProc(thandle_t hdl);

  /// Seek in a remote file
  toff_t SeekProc(thandle_t hdl, toff_t offset, int whence);

  /// Close a remote file
  int CloseProc(thandle_t hdl);

 protected:

 public:

  /// Default Constructor
  RemIIPImage() throw (file_error)
   : IIPImage(),
    offset( 0 ),
    isRemote(false),
    local_handle( NULL ) {
    if ( (curl_handle = curl_easy_init()) == NULL){
      throw file_error("RemIIPImage::RemIIPImage(): curl_easyInit() failed");
  };

  /// Constructer taking the image path as parameter
  /** @param s image path
   */
  RemIIPImage( const std::string& s ) throw (file_error)
   : IIPImage( s ),
    offset( 0 ),
    isRemote(false),
    local_handle( NULL ) {
    if ( (curl_handle = curl_easy_init()) == NULL){
      throw file_error("RemIIPImage::RemIIPImage(): curl_easyInit() failed");
    }
  };

  /// Copy Constructor taking reference to another IIPImage object
  /** @param im IIPImage object
   */
  RemIIPImage( const RemIIPImage& image )
   : IIPImage( image ),
    offset( image.offset ),
    curl_handle( image.curl_handle ),
    isRemote( image.isremote ),
    local_handle( image.local_handle ) {};

  /// Virtual Destructor
  virtual ~IIPImage() { 
    delete *curl_handle
  };

  /// Open a possibly remote file.
  FILE rem_fopen(const char *pathname, const char *mode);

  /// Read from a possibly remote file.
  size_t rem_fread(void *ptr, size_t size, size_t nmemb, CurlImage *stream);

  /// Close a possible remote file.
  int rem_fclose(CurlImage *stream);

  /// Get file status of a possibly remote file.
  int rem_stat(const char *pathname, struct stat *buf) throw(std::string);

  /// No-op needed by curlStatProc
  size_t throw_away(void *ptr, size_t size, size_t nmemb, void *data);

  /// "Overload" of libtiff:TIFFOpen 
  TIFF * rem_TIFFOpen(const char* filename, const char* mode);

};

#endif
