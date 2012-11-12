/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 * Copyright (C) 2010 Code Aurora Forum, All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
/*! \file oscl_file_native.cpp
    \brief This file contains file io APIs
*/

#include "oscl_file_native.h"
#include "oscl_stdstring.h"
#include "oscl_utf8conv.h"
#include "oscl_int64_utils.h"

#ifdef ENABLE_SHAREDFD_PLAYBACK
#ifdef ANDROID
#undef LOG_TAG
#define LOG_TAG "OsclNativeFile"
#include <utils/Log.h>
#endif
#endif
#include "oscl_mem.h"
#include "oscl_file_types.h"
#include "oscl_file_handle.h"

// FIXME:
// Is 100 milliseconds a good choice for writing out a single
// compressed video frame?
static const int FILE_WRITER_SPEED_TOLERANCE_IN_MILLISECONDS = 100;

OsclNativeFile::OsclNativeFile()
{
    iOpenFileHandle = false;
    iMode = 0;

    iFile = 0;
#ifdef ENABLE_SHAREDFD_PLAYBACK
    iSharedFd = -1;
#endif

#if (OSCL_HAS_LARGE_FILE_SUPPORT)
#ifdef ANDROID
    iFileDescriptor = 0;
#endif
#endif

}

OsclNativeFile::~OsclNativeFile()
{
}

int32  OsclNativeFile::Open(const OsclFileHandle& aHandle, uint32 mode
                            , const OsclNativeFileParams& params
                            , Oscl_FileServer& fileserv)
{
    //open with an external file handle

    OSCL_UNUSED_ARG(fileserv);

    iMode = mode;
    iOpenFileHandle = true;

    {
        OSCL_UNUSED_ARG(params);
        //Just save the open file handle
        iFile = aHandle.Handle();
    }

#if (OSCL_HAS_LARGE_FILE_SUPPORT)
#ifdef ANDROID
    iFileDescriptor = fileno(aHandle.Handle());
#endif
#endif
    return 0;
}

static void OpenModeToString(uint32 mode, char mode_str[4])
{
    uint32 index = 0;

    if (mode & Oscl_File::MODE_READWRITE)
    {
        if (mode & Oscl_File::MODE_APPEND)
        {
            mode_str[index++] = 'a';
            mode_str[index++] = '+';
        }
        else
        {
            mode_str[index++] = 'w';
            mode_str[index++] = '+';
        }
    }
    else if (mode & Oscl_File::MODE_APPEND)
    {
        mode_str[index++] = 'a';
        mode_str[index++] = '+';
    }
    else if (mode & Oscl_File::MODE_READ)
    {
        mode_str[index++] = 'r';
    }
    else if (mode & Oscl_File::MODE_READ_PLUS)
    {
        mode_str[index++] = 'r';
        mode_str[index++] = '+';
    }

    if (mode & Oscl_File::MODE_TEXT)
    {
        mode_str[index++] = 't';
    }
    else
    {
        mode_str[index++] = 'b';
    }

    mode_str[index++] = '\0';
}

int32 OsclNativeFile::OpenFileOrSharedFd(
    const char *filename, const uint32 mode)
{

    char openmode[4];
    OpenModeToString(mode, openmode);

#ifdef ENABLE_SHAREDFD_PLAYBACK
    int fd;
    long long offset;
    long long len;
    if (sscanf(filename, "sharedfd://%d:%lld:%lld", &fd, &offset, &len) == 3)
    {
        iSharedFd = fd;
        iSharedFilePosition = 0;
        iSharedFileOffset = offset;
        long long size = lseek64(iSharedFd, 0, SEEK_END);
        lseek64(iSharedFd, 0, SEEK_SET);
        size -= offset;
        iSharedFileSize = size < len ? size : len;
    }
    else
#endif
    {

#if (OSCL_HAS_LARGE_FILE_SUPPORT)
#ifdef ANDROID
      int largeFileOpenMode = FindLargeFileOpenMode(mode);

      iFileDescriptor = open(filename, largeFileOpenMode, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

      //Populate iFile
      iFile = fdopen(iFileDescriptor, openmode);
      if (iFileDescriptor == -1)
        {
          return -1;
        }
      else
        return 0;
#endif
#endif
        if ((iFile = fopen(filename, openmode)) == NULL)
        {
            return -1;
        }
    }

    return 0;
}

int32 OsclNativeFile::Open(const oscl_wchar *filename, uint32 mode
                           , const OsclNativeFileParams& params
                           , Oscl_FileServer& fileserv)
{
    iMode = mode;
    iOpenFileHandle = false;

    {
        OSCL_UNUSED_ARG(fileserv);
        OSCL_UNUSED_ARG(params);

        if (!filename || *filename == '\0') return -1; // Null string not supported in fopen, error out

#ifdef _UNICODE
        oscl_wchar convopenmode[4];
        if (0 == oscl_UTF8ToUnicode(openmode, oscl_strlen(openmode), convopenmode, 4))
        {
            return -1;
        }

        if ((iFile = _wfopen(filename, convopenmode)) == NULL)
        {
            return -1;
        }
#else
        //Convert to UTF8
        char convfilename[OSCL_IO_FILENAME_MAXLEN];
        if (0 == oscl_UnicodeToUTF8(filename, oscl_strlen(filename), convfilename, OSCL_IO_FILENAME_MAXLEN))
        {
            return -1;
        }
        return OpenFileOrSharedFd(convfilename, mode);
#endif
    }

}

int32 OsclNativeFile::Open(const char *filename, uint32 mode
                           , const OsclNativeFileParams& params
                           , Oscl_FileServer& fileserv)
{
    iMode = mode;
    iOpenFileHandle = false;

    OSCL_UNUSED_ARG(fileserv);
    OSCL_UNUSED_ARG(params);

    if (!filename || *filename == '\0') return -1; // Null string not supported in fopen, error out

    return OpenFileOrSharedFd(filename, mode);

}

TOsclFileOffset OsclNativeFile::Size()
{
    //this is the default for platforms with no
    //native size query.
    //Just do seek to end, tell, then seek back.
    TOsclFileOffset curPos = Tell();
    if (curPos >= 0
            && Seek(0, Oscl_File::SEEKEND) == 0)
    {
        TOsclFileOffset endPos = Tell();
        if (Seek(curPos, Oscl_File::SEEKSET) == 0)
        {
            return endPos;
        }
        else
        {
            return (-1);
        }
    }
    return (-1);
}

int32 OsclNativeFile::Close()
{
    int32 closeret = 0;

    {
        if (iOpenFileHandle)
            closeret = Flush();
        else if (iFile != NULL)
        {
            closeret = fclose(iFile);
            iFile = NULL;
#if (OSCL_HAS_LARGE_FILE_SUPPORT)
#ifdef ANDROID
            iFileDescriptor = 0;
#endif
#endif
        }
#ifdef ENABLE_SHAREDFD_PLAYBACK
        else if (iSharedFd >= 0)
        {
            // we don't need to, and in fact MUST NOT, close mSharedFd here,
            // since it might still be shared by another OsclFileNative, and
            // will be closed by the playerdriver when we're done with it.
            closeret = 0;
        }
#endif
        else
        {
            return -1; //Linux Porting : Fix 1
        }
    }

    return closeret;
}


uint32 OsclNativeFile::Read(OsclAny *buffer, uint32 size, uint32 numelements)
{
#ifdef ENABLE_SHAREDFD_PLAYBACK
    if (iSharedFd >= 0)
    {
        // restore position, no locking is needed because all access to the
        // shared filedescriptor is done by the same thread.
        lseek64(iSharedFd, iSharedFilePosition + iSharedFileOffset, SEEK_SET);
        uint32 count = size * numelements;
        if (iSharedFilePosition + count > iSharedFileSize)
        {
            count = iSharedFileSize - iSharedFilePosition;
        }
        ssize_t numread = read(iSharedFd, buffer, count);
        // unlock
        long long curpos = lseek64(iSharedFd, 0, SEEK_CUR);
        if (curpos >= 0)
        {
            iSharedFilePosition = curpos - iSharedFileOffset;
        }
        if (numread < 0)
        {
            return numread;
        }
        return numread / size;
    }
#endif

    int32 result = 0;

#if (OSCL_HAS_LARGE_FILE_SUPPORT)
#ifdef ANDROID
    {
      int32 bytesToBeRead = size * numelements;
      result = read(iFileDescriptor, buffer, bytesToBeRead);
      if (result != -1)
        {
          return (uint32)(result / size);
        }
      return -1;
    }
#endif
#endif
    {
      result = fread(buffer, OSCL_STATIC_CAST(int32, size), OSCL_STATIC_CAST(int32, numelements), iFile);
      return result;
    }
    return 0;
}

bool OsclNativeFile::HasAsyncRead()
{
    return false;//not supported.
}

int32 OsclNativeFile::ReadAsync(OsclAny*buffer, uint32 size, uint32 numelements, OsclAOStatus& status)
{
    OSCL_UNUSED_ARG(buffer);
    OSCL_UNUSED_ARG(size);
    OSCL_UNUSED_ARG(numelements);
    OSCL_UNUSED_ARG(status);
    return -1;//not supported
}

void OsclNativeFile::ReadAsyncCancel()
{
}

uint32 OsclNativeFile::GetReadAsyncNumElements()
{
    return 0;//not supported
}



uint32 OsclNativeFile::Write(const OsclAny *buffer, uint32 size, uint32 numelements)
{
#ifdef ENABLE_SHAREDFD_PLAYBACK
    if (iSharedFd >= 0)
        return 0;
#endif
    if (iFile)
    {
#if (OSCL_HAS_LARGE_FILE_SUPPORT)
#ifdef ANDROID

      int32 num_bytes_written = write(iFileDescriptor, buffer, (size * numelements));
      if (num_bytes_written != -1)
      {
          return (uint32)(num_bytes_written / size);
      }
      else
          return -1;
#endif
#endif
#ifdef ANDROID
        struct timeval startTimeVal, endTimeVal;
        gettimeofday(&startTimeVal, NULL);
        uint32 items = fwrite(buffer, OSCL_STATIC_CAST(int32, size), OSCL_STATIC_CAST(int32, numelements), iFile);
        gettimeofday(&endTimeVal, NULL);
        long long timeInMicroSeconds = (endTimeVal.tv_sec - startTimeVal.tv_sec) * 1000000LL + (endTimeVal.tv_usec - startTimeVal.tv_usec);
        if (timeInMicroSeconds/1000 > FILE_WRITER_SPEED_TOLERANCE_IN_MILLISECONDS) {
          LOGW("writing %d bytes takes too long (%lld micro seconds)", items, timeInMicroSeconds);
        }
        return items;
#else
        uint32 items = fwrite(buffer, OSCL_STATIC_CAST(int32, size), OSCL_STATIC_CAST(int32, numelements), iFile);
        return items;
#endif
    }
    return 0;
}

int32 OsclNativeFile::Seek(TOsclFileOffset offset, Oscl_File::seek_type origin)
{

    {
#ifdef ENABLE_SHAREDFD_PLAYBACK
        if (iSharedFd >= 0)
        {
            uint32 newpos = iSharedFilePosition;
            if (origin == Oscl_File::SEEKCUR) newpos = newpos + offset;
            else if (origin == Oscl_File::SEEKSET) newpos = offset;
            else if (origin == Oscl_File::SEEKEND) newpos = iSharedFileSize + offset;
            if (newpos < 0)
                return EINVAL;
            if (newpos > iSharedFileSize) // is this valid?
                newpos = iSharedFileSize;
            iSharedFilePosition = newpos;
            return 0;
        }
#endif
        if (iFile)
        {
            int32 seekmode = SEEK_CUR;

            if (origin == Oscl_File::SEEKCUR)
                seekmode = SEEK_CUR;
            else if (origin == Oscl_File::SEEKSET)
                seekmode = SEEK_SET;
            else if (origin == Oscl_File::SEEKEND)
                seekmode = SEEK_END;
#if OSCL_HAS_LARGE_FILE_SUPPORT
#ifdef ANDROID
            TOsclFileOffset seekResult = lseek64(iFileDescriptor, offset, seekmode);
            if (seekResult == -1){
              LOGE("OsclNativeFile::Seek lseek64 failed");
              return -1;
            }
            else
              return 0;
#endif
            return fseeko(iFile, offset, seekmode);
#else
            return fseek(iFile, offset, seekmode);
#endif
        }
    }
    return -1;
}


TOsclFileOffset OsclNativeFile::Tell()
{
    TOsclFileOffset result = -1;
#ifdef ENABLE_SHAREDFD_PLAYBACK
    if (iSharedFd >= 0)
        return iSharedFilePosition;
#endif
    if (iFile)
    {
#if OSCL_HAS_LARGE_FILE_SUPPORT
#ifdef ANDROID
      result = lseek64(iFileDescriptor, 0, SEEK_CUR);
      if( result == -1 ){
        LOGE("OsclNativeFile::Tell lseek64 failed");
        return -1;
      }
      else return result;
#endif
      result = ftello(iFile);
#else
      result = ftell(iFile);
#endif
    }
    return result;
}



int32 OsclNativeFile::Flush()
{
#ifdef ENABLE_SHAREDFD_PLAYBACK
    if (iSharedFd >= 0)
        return iSharedFilePosition >= iSharedFileSize;
#endif
    if (iFile)
        return fflush(iFile);
    return EOF;
}



int32 OsclNativeFile::EndOfFile()
{
#ifdef ENABLE_SHAREDFD_PLAYBACK
    if (iSharedFd >= 0)
        return iSharedFilePosition >= iSharedFileSize;
#endif
#if OSCL_HAS_LARGE_FILE_SUPPORT
#ifdef ANDROID
    if (iFile)
      {
        return Tell() < Size() ? 0 : 1;
      }
#endif
#endif
    if (iFile)
        return feof(iFile);
    return 0;
}


int32 OsclNativeFile::GetError()
{
//FIXME ENABLE_SHAREDFD_PLAYBACK
    if (iFile)
        return ferror(iFile);
    return 0;
}

#if (OSCL_HAS_LARGE_FILE_SUPPORT)
#ifdef ANDROID
int32 OsclNativeFile::FindLargeFileOpenMode(uint32 mode)
{

  int32 largeFileOpenMode = 0;

  if (mode & Oscl_File::MODE_APPEND)
    {
      largeFileOpenMode = O_CREAT | O_APPEND;
    }
  else if (mode & Oscl_File::MODE_READ)
    {
      largeFileOpenMode = O_RDONLY;
    }
  else if (mode & Oscl_File::MODE_READ_PLUS)
    {
      largeFileOpenMode = O_RDWR;
    }
  else if (mode & Oscl_File::MODE_READWRITE)
    {
      largeFileOpenMode = (O_RDWR | O_CREAT);
      if (mode & Oscl_File::MODE_APPEND)
        {
          largeFileOpenMode |= O_APPEND;
        }
      else
        {
          largeFileOpenMode |= O_TRUNC;
        }
    }
  largeFileOpenMode |= O_LARGEFILE;
  return largeFileOpenMode;
}
#endif
#endif
