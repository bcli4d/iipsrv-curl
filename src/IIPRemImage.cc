// IIPImage.cc 


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


#include "IIPRemImage.h"

#ifdef HAVE_GLOB_H
#include <glob.h>
#endif

#if _MSC_VER
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

#include <cstdio>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>

using namespace std;

void IIPRemImage::testImageType() throw(file_error)
{
  // Check whether it is a regular file
  struct stat sb;

  string path = fileSystemPrefix + imagePath;
  const char *pstr = path.c_str();

  if( (rem_stat(pstr,&sb)==0) && S_ISREG(sb.st_mode) ){

    unsigned char header[10];

    // Immediately open our file to reduce (but not eliminate) TOCTOU race condition risks
    // We should really use open() before fstat() but it's not supported on Windows and
    // fopen will in any case complain if file no longer readable
    /*
     * struct remoteHdl *im = rem_fopen( pstr, "rb" );
     *
     * if( im == NULL ){
     *  string message = "Unable to open file '" + path + "'";
     *  throw file_error( message );
     * }
     */

    rem_fopen( pstr, "rb" );

    // Determine our file format using magic file signatures -
    // read in 10 bytes and immediately close file
    //int len = rem_fread( header, 1, 10, im );
    int len = rem_fread( header, 1, 10 );
    //rem_fclose( im );
    rem_fclose( );

    // Make sure we were able to read enough bytes
    if( len < 10 ){
      string message = "Unable to read initial byte sequence from file '" + path + "'";
      throw file_error( message );
    }

    isFile = true;
    timestamp = sb.st_mtime;

    // Magic file signature for JPEG2000
    static const unsigned char j2k[10] = {0x00,0x00,0x00,0x0C,0x6A,0x50,0x20,0x20,0x0D,0x0A};

    // Magic file signatures for TIFF (See http://www.garykessler.net/library/file_sigs.html)
    static const unsigned char stdtiff[3] = {0x49,0x20,0x49};       // TIFF
    static const unsigned char lsbtiff[4] = {0x49,0x49,0x2A,0x00};  // Little Endian TIFF
    static const unsigned char msbtiff[4] = {0x49,0x49,0x2A,0x00};  // Big Endian TIFF
    static const unsigned char lbigtiff[4] = {0x4D,0x4D,0x00,0x2B}; // Little Endian BigTIFF
    static const unsigned char bbigtiff[4] = {0x49,0x49,0x2B,0x00}; // Big Endian BigTIFF

    // Compare our header sequence to our magic byte signatures
    if( memcmp( header, j2k, 10 ) == 0 ) format = JPEG2000;
    else if( memcmp( header, stdtiff, 3 ) == 0
	     || memcmp( header, lsbtiff, 4 ) == 0 || memcmp( header, msbtiff, 4 ) == 0
	     || memcmp( header, lbigtiff, 4 ) == 0 || memcmp( header, bbigtiff, 4 ) == 0 ){
      format = TIF;
    }
    else format = UNSUPPORTED;

  }
  else{

#ifdef HAVE_GLOB_H

    // Check for sequence
    glob_t gdat;
    string filename = path + fileNamePattern + "000_090.*";

    if( glob( filename.c_str(), 0, NULL, &gdat ) != 0 ){
      globfree( &gdat );
      string message = path + string( " is neither a file nor part of an image sequence" );
      throw file_error( message );
    }
    if( gdat.gl_pathc != 1 ){
      globfree( &gdat );
      string message = string( "There are multiple file extensions matching " )  + filename;
      throw file_error( message );
    }

    string tmp( gdat.gl_pathv[0] );
    globfree( &gdat );

    isFile = false;

    int dot = tmp.find_last_of( "." );
    int len = tmp.length();

    suffix = tmp.substr( dot + 1, len );
    if( suffix == "jp2" || suffix == "jpx" || suffix == "j2k" ) format = JPEG2000;
    else if( suffix == "tif" || suffix == "tiff" ) format = TIF;
    else format = UNSUPPORTED;

    updateTimestamp( tmp );

#else
    string message = path + string( " is not a regular file and no glob support enabled" );
    throw file_error( message );
#endif

  }

}

void IIPRemImage::updateTimestamp( const string& path ) throw(file_error)
{
  // Get a modification time for our image
  struct stat sb;

  if( rem_stat( path.c_str(), &sb ) == -1 ){
    string message = string( "Unable to open file " ) + path;
    throw file_error( message );
  }
  timestamp = sb.st_mtime;
}

/// Get file status of a possibly remote file.                                           
int IIPRemImage ::rem_stat(const char *pathname, struct stat *buf) throw(file_error){
  if (stat(pathname, buf) == -1) {
    if ( (StatProc(pathname, buf) == -1)) {
      return -1;
    }
  }
  return 0;
}

int IIPRemImage::StatProc(const char *pathname, struct stat *buf)
{
  CURLcode res;
  long filetime = -1;
  double filesize = 0.0;
  //const char *filename = strrchr(ftpurl, '/') + 1;                                     

  curl_easy_setopt(curl, CURLOPT_URL, pathname);
  /* No download if the file */
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
  /* Ask for filetime */
  curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);
  /* No header output: TODO 14.1 http-style HEAD output for ftp */
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, throw_away);
  curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
  /* Switch on full protocol/debug output */
  /* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); */

  res = curl_easy_perform(curl);

  if(CURLE_OK == res) {
    res = curl_easy_getinfo(curl, CURLINFO_FILETIME, &filetime);
    /*                                                                                   
    if((CURLE_OK == res) && (filetime >= 0)) {                                           
      time_t file_time = (time_t)filetime;                                               
      printf("filetime %s: %s", filename, ctime(&file_time));                            
    }                                                                                    
    */
    res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                            &filesize);
    /*                                                                                   
    if((CURLE_OK == res) && (filesize>0.0))                                              
      printf("filesize %s: %0.0f bytes\n", filename, filesize);                          
    */
    if ( ( file_time >= 0 ) && ( filesize > 0.0 ) ){
      /* Assume it'a a regular file */
      buf->st_mode = I_IFREG;

      /* Return filetime as st_mtime */
      buf->st_mtime.tv_sec = file_time;
      buf->st_mtime.tv_nsec = 0;
      return 0;
    }
    else {
      return -1;
    }
  }
  else {
    /* we failed */
    // fprintf(stderr, "curl told us %d\n", res);
    return -1
      }
  return 0;
  }

size_t IIPRemImage::throw_away(void *ptr, size_t size, size_t nmemb, void *data)
{
  (void)ptr;
  (void)data;
  /* we are not interested in the headers itself,                                        
     so we only return the size we would have saved ... */
  return (size_t)(size * nmemb);
}

/// Open a TIFF file. File might be local or remote
TIFF * IIPRemImage::rem_TIFFOpen(const char* filename, const char* mode)
{
  TIFF *tiff;
  if ( (tiff = TIFFOpen( filename.c_str(), "rm" ) ) == NULL ){
    struct curl_handle *hdl = new curl_handle;
    isRemote = true;
    local_handle = NULL;
    tiff = TIFFClientOpen( filename.c_str(), "rm",
			   (thandle_t)this,
			   ReadProc,
			   WriteProc,
			   SeekProc,
			   SizeProc,
			   CloseProc,
			   NULL, /* no mapproc */
			   NULL /* no unmapproc */);
  }
  return tiff;
}

/// Read from a remote file                                                             
tmsize_t IIPRemImage::ReadProc(thandle_t hdl, void * buf, tmsize_t size)
{
  CURLcode res;
  ssize_t rsize;
  curl_handle * hdl = (curl_handle*)hdl;
  char range[256];

  struct MemoryStruct chunk;

  chunk.memory = buf;  /* copy to this buffer */
  chunk.size = 0;    /* no data at this point */
  chunk.buf_size = size;

  /* Generate range string*/
  sprintf(range,"%d-%ld",hdl->offset, hdl->offset+size-1);

  /* specify URL to get */
  curl_easy_setopt(hdl->curl_handle, CURLOPT_URL, hdl->imagePath);

  /* Set the range of bytes to read */
  curl_easy_setopt(hdl->curl_handle, CURLOPT_RANGE, range);

  /* send all data to this function  */
  curl_easy_setopt(hdl->curl_handle, CURLOPT_WRITEFUNCTION, copy_data);

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(hdl->curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

  /* Perform the request */
  res = curl_easy_perform(hdl->curl_handle);

  /* check for errors */
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
	    curl_easy_strerror(res));
    return -1;
  }
  else {
    printf("%lu bytes retrieved\n", (long)chunk.size);
    //    print_bytes(buf, chunk.size);                                                 
    hdl->offset = hdl->offset+chunk.size;
    return chunk.size;
  }
}

/// Write to a remote file                                                              
tmsize_t IIPRemImage::WriteProc(thandle_t hdl, void * buf, tmsize_t size)
{
  fprintf(stderr, "write() not supported for remote files\n");
  return -1;
}

/// Return size of a remote file                                                        
tmsize_t IIPRemImage::SizeProc(thandle_t hdl)
{
  fprintf(stderr, "size() not supported for remote filess\n");
  return -1;
}

/// Seek in a remote file                                                               
toff_t IIPRemImage::SeekProc(thandle_t hdl, toff_t offset, int whence)
{
  curl_handle * hdl = (curl_handle*)hdl;
  tmsize_t size;
  toff_t res;

  switch (whence){
  case SEEK_SET:
    hdl->offset = offset;
    break;
  case SEEK_CUR:
    hdl->offset += offset;
    break;
  case SEEK_END:
    if ( (size = _size(hdl) ) < 0) {
      fprintf(stderr, "Seek mode SEEK_END not supported");
      return -1;
    }
  default:
    fprintf(stderr, "Seek mode %d undefined", whence);
    return -1;
  }
  return hdl->offset;
}

/// Close a remote file                                                                 
int  IIPRemImage::CloseProc(thandle_t hdl)
{
  delete *(curl_handle *)hdl;
  return 0;
}

/// Open a possibly remote file.                                                         
FILE IIPRemImage::rem_fopen(const char *pathname, const char *mode){
  FILE *f;
  if ( ( im = fopen( pstr, "rb" )) != NULL){
    hdl->isRemote = false;
    hdl->local_handle = im;
  }
  else {
    /*                                                                                   
     * If file is remote we don't actually open it. Presumably the file exists 
     * remotely	or the previous call to rem_stat() would have failed and 
     * we would not have gotten here.
     */
    hdl->imagePath = pathname;
    hdl->offset = 0;
    hdl->curl_handle = curl_handle;
    hdl->isRemote = true;
  }
  return 0;
}

/// Read from a possibly remote file.                                                    
size_t IIPRemImage::rem_fread(void *ptr, size_t size, size_t nmemb){
  if (isRemote) {
    return std:fread(ptr, size, nmemb, local_handle);
  }
  else {
    return curlReadProc( thandle_t stream, ptr, nmemb*size );
  }

}

/// Close a possible remote file.                                                        
int IIPRemImage::rem_fclose( ){
  int res = 0;
  if (isRemote) {
    res =  fclose(local_handle);
  }
  /* No close in the case of a remote file */
  delete *stream;
}

/// curl callback function to copy data from curl buffer to our buffer                  
size_t IIPImage::copy_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  if(realsize+mem->size > mem->buf_size) {
    printf("Potential buffer overflow in TIFF read\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), buffer, realsize);
  mem->size += realsize;

  return realsize;
}

