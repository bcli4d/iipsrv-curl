// Member functions for CurlImage.h

/*  IIP Server: Remote file IO handler

*/


#include "CurlImage.h"
//#include <sstream>


using namespace std;

size_t throw_away(void *ptr, size_t size, size_t nmemb, void *data)
{
  (void)ptr;
  (void)data;
  /* we are not interested in the headers itself,
     so we only return the size we would have saved ... */
  return (size_t)(size * nmemb);
}

int CurlImage::curlStatProc(curl_hdl * hdl, struct stat *buf)
{
  CURLcode res;
  long filetime = -1;
  double filesize = 0.0;
  const char *filename = strrchr(ftpurl, '/') + 1;

  curl_easy_setopt(curl, CURLOPT_URL, ftpurl);
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
    if((CURLE_OK == res) && (filetime >= 0)) {
      time_t file_time = (time_t)filetime;
      printf("filetime %s: %s", filename, ctime(&file_time));
    }
    res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
			    &filesize);
    if((CURLE_OK == res) && (filesize>0.0))
      printf("filesize %s: %0.0f bytes\n", filename, filesize);

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
    fprintf(stderr, "curl told us %d\n", res);
    return -1
  }
  return 0;
}

void CurlImage::print_bytes(const void *object, size_t size)
{
  // This is for C++; in C just drop the static_cast<>() and assign.
  const unsigned char * const bytes = static_cast<const unsigned char *>(object);
  //  const unsigned char * const bytes = object;
  size_t i;

  printf("[ ");
  for(i = 0; i < size; i++)
    {
      printf("%02x ", bytes[i]);
    }
  printf("]\n");
}

/// curl callback function to copy data from curl buffer to our buffer
size_t CurlImage::copy_data(void *buffer, size_t size, size_t nmemb, void *userp)
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

/// Read from a remote file
tmsize_t CurlImage::curlReadProc(thandle_t hdl, void * buf, tmsize_t size)
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
  curl_easy_setopt(hdl->curl_handle, CURLOPT_URL, hdl->filename);

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
    return(-1);
  }
  else {
    printf("%lu bytes retrieved\n", (long)chunk.size);
    //    print_bytes(buf, chunk.size);
    hdl->offset = hdl->offset+chunk.size;
    return chunk.size;
  }
}
      
/// Write to a remote file
tmsize_t CurlImage::curlWriteProc(thandle_t hdl, void * buf, tmsize_t size)
{
  fprintf(stderr, "write() not supported for remote files\n");
  return -1;
}

/// Return size of a remote file
tmsize_t CurlImage::curlSizeProc(thandle_t hdl)
{
  fprintf(stderr, "size() not supported for remote filess\n");
  return -1;
}

/// Seek in a remote file
toff_t CurlImage::curlSeekProc(thandle_t hdl, toff_t offset, int whence)
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
int  CurlImage::curlCloseProc(thandle_t hdl)
{
  delete *(curl_handle *)hdl;
  return 0;
}

/// Open a TIFF file. File might be local or remote
TIFF * CurlImage::curlTIFFOpen(const char* filename, const char* mode)
{
  TIFF *tiff;
  if ( (tiff = TIFFOpen( filename.c_str(), "rm" ) ) == NULL ){
    struct curl_handle *hdl = new curl_handle;
    hdl->filename = filename;
    hdl->offset = 0;
    hdl->curl_handle = curl;
    hdl->isRemote = true;
    hdl->local_handle = NULL;
    tiff = TIFFClientOpen( filename.c_str(), "rm",
			   (thandle_t)hdl,
			   curlReadProc,
			   curlWriteProc,
			   curlSeekProc,
			   curlSizeProc,
			   curlCloseProc,
			   NULL, /* no mapproc */
			   NULL /* no unmapproc */);
  }
}

/// Open a possibly remote file.
FILE CurlImage::curlfopen(const char *pathname, const char *mode){
  curl_handle *hdl = new curl_handle;
  FILE *f;
  if ( ( im = fopen( pstr, "rb" )) != NULL){
    hdl->isRemote = false;
    hdl->local_handle = im;
  }
  else {
    /* 
     * If file is remote we don't actually open it. Presumably the file exists remotely or
     * the previous call to curlstat() would have failed and we would not have gotten here.
     */
    hdl->filename = filename;
    hdl->offset = 0;
    hdl->curl_handle = curl;
    hdl->isRemote = true;
  }
  return hdl;
}

/// Read from a possibly remote file.
size_t CurlImage::curlfread(void *ptr, size_t size, size_t nmemb, curl_handle *stream){
  if (!stream->isRemote) {
    return fread(ptr, size, nmemb, stream->local_handle);
  }
  else {
    return curlReadProc( thandle_t stream, ptr, nmemb*size );
  }
      
}

/// Close a possible remote file.
int CurlImage::curlfclose(curl_handle *stream){
  int res = 0;
  if (!stream->isRemote) {
    res =  fclose(stream->local_handle);
  }
  /* No close in the case of a remote file */
  delete *stream;
}

/// Get file status of a possibly remote file.
int CurlImage ::curlstat(const char *pathname, struct stat *buf){
  if (stat(pathname, buf) == -1) {
}

