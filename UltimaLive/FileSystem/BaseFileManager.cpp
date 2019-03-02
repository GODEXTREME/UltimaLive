/* Copyright(c) 2016 UltimaLive
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "BaseFileManager.h"

#include <shlwapi.h>
#pragma comment(lib,"shlwapi.lib")
#include "shlobj.h"
#include "..\Maps\MapDefinition.h"

unsigned char* BaseFileManager::readStaticsBlock(uint32_t, uint32_t blockNum, uint32_t& rNumberOfBytesOut)
{
  uint8_t* pBlockIdx = m_pStaidxPool;
  pBlockIdx += (blockNum * 12);

  uint32_t lookup = *reinterpret_cast<uint32_t*>(pBlockIdx); 
  rNumberOfBytesOut = *reinterpret_cast<uint32_t*>(pBlockIdx + 4); //length
  uint8_t* pRawStaticData = NULL;

  if (lookup >= 0 && lookup < STATICS_MEMORY_SIZE && rNumberOfBytesOut > 0 && rNumberOfBytesOut != 0xFFFFFFFF)
  {
    uint8_t* pStatics = reinterpret_cast<uint8_t*>(m_pStaticsPool);
    pStatics += lookup;

    pRawStaticData = new uint8_t[rNumberOfBytesOut];

    for(uint32_t i = 0; i < rNumberOfBytesOut; ++i)
    {
      pRawStaticData[i] = pStatics[i];
    }
  }

  return pRawStaticData;
}

bool BaseFileManager::writeStaticsBlock(uint8_t, uint32_t blockNum, uint8_t* pBlockData, uint32_t updatedStaticsLength)
{
#ifdef DEBUG
  printf("Writing statics: %i\n", blockNum);
#endif

  char buffer[sizeof(uint32_t)];
  uint8_t* pBlockIdx = m_pStaidxPool;
  pBlockIdx += (blockNum * 12);

  //Zero length statics block is a corner case
  if (updatedStaticsLength <= 0)
  {
#ifdef DEBUG
    printf("writing block with zero statics\n");
#endif

    //update index length in memory
    *reinterpret_cast<uint32_t*>(pBlockIdx + 4) = 0;
    
    //update index lookup in memory
    *reinterpret_cast<uint32_t*>(pBlockIdx) = 0xFFFFFFFF;

    //update index length on disk
    *reinterpret_cast<uint32_t*>(buffer) = 0;
    m_pStaidxFileStream->seekp((blockNum * 12) + 4, std::ios::beg);
    m_pStaidxFileStream->write(buffer, sizeof(uint32_t));

    //update index lookup on disk
    *reinterpret_cast<uint32_t*>(buffer) = 0xFFFFFFFF;
    m_pStaidxFileStream->seekp(blockNum * 12, std::ios::beg);
    m_pStaidxFileStream->write(buffer, sizeof(uint32_t));
    m_pStaidxFileStream->flush();

    return true;
  }

  uint32_t existingLookup = *reinterpret_cast<uint32_t*>(pBlockIdx); 

  #ifdef DEBUG
    printf("Existing lookup: 0x%x\n", existingLookup);
  #endif

  uint32_t existingStaticsLength = *reinterpret_cast<uint32_t*>(pBlockIdx + 4);

  //update index length in memory
  *reinterpret_cast<uint32_t*>(pBlockIdx + 4) = updatedStaticsLength;

  //update index length on disk
  *reinterpret_cast<uint32_t*>(buffer) = updatedStaticsLength;
  m_pStaidxFileStream->seekp((blockNum * 12) + 4, std::ios::beg);
  m_pStaidxFileStream->write(buffer, sizeof(uint32_t));

  //Do we have enough room to write the statics into the existing location?
  if (existingStaticsLength >= updatedStaticsLength && existingLookup != 0xFFFFFFFF && existingLookup < STATICS_MEMORY_SIZE)
  {
#ifdef DEBUG
    printf("writing statics to existing file location at 0x%x, length:%i\n", existingLookup, updatedStaticsLength);
#endif

    //update memory
    uint8_t* pStatics = m_pStaticsPool;
    pStatics += existingLookup;
    memcpy(pStatics, pBlockData, updatedStaticsLength);

    //update disk
    m_pStaticsFileStream->seekp(existingLookup, std::ios::beg);
    m_pStaticsFileStream->write((const char*)pBlockData, updatedStaticsLength);
  }
  else
  {
    uint32_t newLookup = m_pStaticsPoolEnd - m_pStaticsPool;
#ifdef DEBUG
    printf("writing statics to end of file 0x%x, length:%i\n", newLookup, updatedStaticsLength);
#endif

    //update index lookup in memory
    *reinterpret_cast<uint32_t*>(pBlockIdx) = newLookup;

    //update index lookup on disk
    *reinterpret_cast<uint32_t*>(buffer) = newLookup;
    m_pStaidxFileStream->seekp(blockNum * 12, std::ios::beg);
    m_pStaidxFileStream->write(buffer, sizeof(uint32_t));

    //update statics in memory
    memcpy(m_pStaticsPoolEnd, pBlockData, updatedStaticsLength);

    //update statics on disk
    m_pStaticsFileStream->seekp(newLookup, std::ios::beg);
    m_pStaticsFileStream->write((const char*)pBlockData, updatedStaticsLength);

    m_pStaticsPoolEnd += updatedStaticsLength;
  }

  m_pStaidxFileStream->flush();
  m_pStaticsFileStream->flush();
  return true;
}

void BaseFileManager::Initialize()
{
  CreateDirectoryA(getUltimaLiveSavePath().c_str(), NULL);
}

void BaseFileManager::onLogout()
{
#ifdef DEBUG
  printf("Closing map, staidx, statics file streams\n");
#endif
  if (m_pMapFileStream != NULL)
  {
    m_pMapFileStream->flush();
    m_pMapFileStream->close();
  }

  if (m_pStaidxFileStream != NULL)
  {
    m_pStaidxFileStream->flush();
    m_pStaidxFileStream->close();
  }

  if (m_pStaticsFileStream != NULL)
  {
    m_pStaticsFileStream->flush();
    m_pStaticsFileStream->close();
  }
}

bool BaseFileManager::createNewPersistentMap(std::string pathWithoutFilename, uint8_t mapNumber, uint32_t numHorizontalBlocks, uint32_t numVerticalBlocks)
{
  //create file names
  std::string mapPath(pathWithoutFilename);
  mapPath.append("\\");
  std::string staidxPath(mapPath);
  std::string staticsPath(mapPath);
  char filename[32];
  sprintf_s(filename, "map%i.mul", mapNumber);
  mapPath.append(filename);
  sprintf_s(filename, "staidx%i.mul", mapNumber);
  staidxPath.append(filename);
  sprintf_s(filename, "statics%i.mul", mapNumber);
  staticsPath.append(filename);

  //create map new file
  std::fstream mapFile;
  mapFile.open(mapPath, std::ios::out | std::ios::app | std::ios::binary | std::ios::in);

#ifdef DEBUG
  printf("Creating map file\n");
  printf("Writing %u blocks by %u blocks\n", numHorizontalBlocks, numVerticalBlocks);
#endif

  uint32_t numberOfBytesInStrip = 196 * numVerticalBlocks;
  char* pVerticalBlockStrip = new char[numberOfBytesInStrip];

  const char block[196] = {
                          0x00, 0x00, 0x00, 0x00, //header
                          0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00,
                          0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00,
                          0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00,
                          0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00,
                          0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00,
                          0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00,
                          0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00,
                          0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00, 0x44, 0x02, 0x00 };
  for (uint32_t y = 0; y < numVerticalBlocks; y++)
  {
    memcpy(&pVerticalBlockStrip[196 * y], block, 196);
  }  

  for (uint32_t x = 0; x < numHorizontalBlocks; x++)
  {
    mapFile.write(pVerticalBlockStrip, numberOfBytesInStrip);
  }
  delete pVerticalBlockStrip;
  mapFile.close();

  std::fstream staidxFile;
  staidxFile.open(staidxPath, std::ios::out | std::ios::app | std::ios::binary | std::ios::in);

#ifdef DEBUG
  printf("Creating index file\n");
  printf("W %u blocks by %u blocks\n", numHorizontalBlocks, numVerticalBlocks);
#endif

  uint32_t numberOfBytesInIndexStrip = 12 * numVerticalBlocks;
  char* pVerticalBlockIndexStrip = new char[numberOfBytesInIndexStrip];
  unsigned char blankIndex[12] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

  for (uint32_t y = 0; y < numVerticalBlocks; y++)
  {
    memcpy(&pVerticalBlockIndexStrip[12 * y], blankIndex, 12);
  }  

  for (uint32_t x = 0; x < numHorizontalBlocks; x++)
  {
    staidxFile.write(pVerticalBlockIndexStrip, numberOfBytesInIndexStrip);
  }
  delete pVerticalBlockIndexStrip;
  staidxFile.close();

  std::fstream staticsFile;
  staticsFile.open(staticsPath, std::ios::out | std::ios::app | std::ios::binary | std::ios::in);
  staticsFile.close();

  return true;
}

void BaseFileManager::copyFile(std::string sourceFilePath, std::string destFilePath, ProgressBarDialog* pProgress)
{
  std::ifstream sourceFile(sourceFilePath, std::ifstream::binary);

  if (!sourceFile.good())
  {
    sourceFile.close();
    return;
  }

  sourceFile.seekg(0, sourceFile.end);
  std::streamoff totalFileSizeInBytes = sourceFile.tellg();
  sourceFile.close();

  uint32_t bytesCopied = 0;
  uint32_t prevPercent = 0;

  // allocate memory for buffer
  char* buffer = new char[4096];
  size_t bytesToWrite;

  FILE* pSource = fopen(sourceFilePath.c_str(), "rb");
  FILE* pDest = fopen(destFilePath.c_str(), "wb");

    // clean and more secure
    // feof(FILE* stream) returns non-zero if the end of file indicator for stream is set
  do 
  {
    bytesToWrite = fread(buffer, 1, 4096, pSource);
    if (bytesToWrite != 0)
    {
      fwrite(buffer, 1, bytesToWrite, pDest);
      bytesCopied += bytesToWrite;
      
      if (pProgress != NULL)
      {
        uint32_t percent = (uint32_t)(((float)bytesCopied / (float)totalFileSizeInBytes) * 100.0f);
        if ((uint32_t)percent > prevPercent)
        {
          prevPercent = percent;
          pProgress->setProgress((uint32_t)percent);
        }
      }
    }
  } while (bytesToWrite != 0);

  fclose(pSource);
  fclose(pDest);
}

void BaseFileManager::InitializeShardMaps(std::string shardIdentifier, std::map<uint32_t, MapDefinition> mapDefinitions)
{
  m_pProgressDlg = new ProgressBarDialog();
  m_pProgressDlg->show();


#ifdef DEBUG
  printf("(((((((((((((Initializing Shard Maps & Statics)))))))))))))))))\n");
#endif

  m_shardIdentifier = shardIdentifier;
  std::string shardFullPath(getUltimaLiveSavePath());
  shardFullPath.append("\\");
  shardFullPath.append(shardIdentifier);
  CreateDirectoryA(shardFullPath.c_str(), NULL);

  for (std::map<uint32_t, MapDefinition>::iterator itr = mapDefinitions.begin(); itr != mapDefinitions.end(); itr++)
  {
    std::string filePath(shardFullPath);
    filePath.append("\\");
    char filename[32];
    sprintf_s(filename, "map%i.mul", itr->first);
    filePath.append(filename);

#ifdef DEBUG
    printf("Checking for %s\n", filePath.c_str());
#endif
    std::fstream mapFile(filePath);

    if (mapFile.good())
    {
#ifdef DEBUG
      printf("File exists and is ok\n");
#endif
    }
    else
    {
      //copy existing maps if they match the dimensions specified in the map definitions
      uint32_t fileSizeNeeded = (itr->second.mapWidthInTiles >> 3) * (itr->second.mapHeightInTiles >> 3) * 196;

#ifdef DEBUG
      printf("File Needs to be %u bytes\n", fileSizeNeeded);
#endif

      std::string clientFolder = Utils::GetCurrentPathWithoutFilename();;
      std::string existingFilePath(clientFolder);
      existingFilePath.append("\\");
      existingFilePath.append(filename);

      std::streamoff currentFileSize = 0;

      //get file size
      std::ifstream existingMapFile(existingFilePath, std::ifstream::binary);
      if (existingMapFile.good())
      {
        existingMapFile.seekg(0, existingMapFile.end);
        currentFileSize = existingMapFile.tellg();
      }
      existingMapFile.close();

#ifdef DEBUG
      printf("%s\n", existingFilePath.c_str());
      printf("Existing file size: %u\n", currentFileSize);
#endif

      if (fileSizeNeeded == currentFileSize)
      {
#ifdef DEBUG
        printf("Copying File: %s to %s ... ", existingFilePath.c_str(), filePath.c_str());
#endif
        //map file
        std::string mapMessage("Copying ");
        mapMessage.append(filename);
        mapMessage.append(" from game client folder");
        m_pProgressDlg->setMessage(mapMessage);
        m_pProgressDlg->setProgress(0);
        copyFile(existingFilePath, filePath, m_pProgressDlg);

#ifdef DEBUG
        printf("done!\n");
#endif
        //statics file
        std::string dstStaticsFilePath(shardFullPath);
        dstStaticsFilePath.append("\\");
        char staticsFilename[32];
        sprintf_s(staticsFilename, "statics%i.mul", itr->first);
        dstStaticsFilePath.append(staticsFilename);
        std::string staticsFilePath (clientFolder);
        staticsFilePath.append("\\");
        staticsFilePath.append(staticsFilename);

#ifdef DEBUG
        printf("Copying File: %s to %s ... ", staticsFilePath.c_str(), dstStaticsFilePath.c_str());
#endif
        std::string staticsMessage("Copying ");
        staticsMessage.append(staticsFilename);
        staticsMessage.append(" from game client folder");
        m_pProgressDlg->setMessage(staticsMessage);
        m_pProgressDlg->setProgress(0);
        copyFile(staticsFilePath, dstStaticsFilePath, m_pProgressDlg);

#ifdef DEBUG
        printf("done!\n");
#endif
        //statics idx
        std::string dstStaidxFilePath(shardFullPath);
        dstStaidxFilePath.append("\\");
        char staidxFilename[32];
        sprintf_s(staidxFilename, "staidx%i.mul", itr->first);
        dstStaidxFilePath.append(staidxFilename);
        std::string staidxFilePath (clientFolder);
        staidxFilePath.append("\\");
        staidxFilePath.append(staidxFilename);

#ifdef DEBUG
        printf("Copying File: %s to %s ... ", staidxFilePath.c_str(), dstStaidxFilePath.c_str());
#endif
        std::string staidxMessage("Copying ");
        staidxMessage.append(filename);
        staidxMessage.append(" from game client folder");
        m_pProgressDlg->setMessage(staidxMessage);
        m_pProgressDlg->setProgress(0);
        copyFile(staidxFilePath, dstStaidxFilePath, m_pProgressDlg);

#ifdef DEBUG
        printf("done!\n");
#endif

      }
      else
      {
        createNewPersistentMap(shardFullPath, static_cast<uint8_t>(itr->first), itr->second.mapWidthInTiles >> 3, itr->second.mapWrapHeightInTiles >> 3);
      }
    }

    mapFile.close();
  }

  m_pProgressDlg->hide();
  delete m_pProgressDlg;
  m_pProgressDlg = NULL;
}

std::string BaseFileManager::getUltimaLiveSavePath()
{
  char szPath[MAX_PATH];
  // Get path for each computer, non-user specific and non-roaming data.
  if ( SUCCEEDED( SHGetFolderPath( NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath ) ) )
  {
      // Append product-specific path
	  std::string completePath(szPath);
	  completePath.append("\\");
	  return completePath;
  }
  return std::string("");
}

BaseFileManager::BaseFileManager()
  : m_files(),
  m_pMapPool(NULL),
  m_pStaticsPool(NULL),
  m_pStaticsPoolEnd(NULL),
  m_pStaidxPool(NULL),
  m_pStaidxPoolEnd(NULL),
  m_shardIdentifier(""),
  m_pMapFileStream(new std::ofstream()),
  m_pStaidxFileStream(new std::ofstream()),
  m_pStaticsFileStream(new std::ofstream()),
  m_pProgressDlg()
{
  //do nothing
}

BOOL WINAPI BaseFileManager::OnCloseHandle(_In_ HANDLE hObject)
{
  for (std::map<std::string, ClientFileHandleSet*>::const_iterator iterator = m_files.begin(); iterator != m_files.end(); iterator++)
  {
    if (iterator->second->m_createFileHandle == hObject)
    {
        m_files.erase(iterator);
        break;
    }
  }

  return true;
}

HANDLE WINAPI BaseFileManager::OnCreateFileA(
  __in      LPCSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile
  )
{
  std::string originalFilename(lpFileName);
  std::string filename = Utils::getFilenameFromPath(originalFilename);

  std::map<std::string, ClientFileHandleSet*>::iterator pMatchingFileSetItr = m_files.find(filename);

  HANDLE handleToBeReturned = CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

  if (pMatchingFileSetItr == m_files.end())
  {
    m_files[filename] = new ClientFileHandleSet(originalFilename);
    m_files[filename]->m_createFileHandle = handleToBeReturned;
  }

  return handleToBeReturned;
}

HANDLE WINAPI BaseFileManager::OnCreateFileMappingA(
  __in      HANDLE hFile,
  __in_opt  LPSECURITY_ATTRIBUTES lpAttributes,
  __in      DWORD flProtect,
  __in      DWORD dwMaximumSizeHigh,
  __in      DWORD dwMaximumSizeLow,
  __in_opt  LPCSTR lpName
  )
{
  ClientFileHandleSet* pMatchingFileset = NULL;

  for (std::map<std::string, ClientFileHandleSet*>::const_iterator iterator = m_files.begin(); iterator != m_files.end(); iterator++)
  {
    if (hFile == iterator->second->m_createFileHandle)
    {
      pMatchingFileset = iterator->second;
      break;
    }
  }
  
  HANDLE handleToReturn = CreateFileMappingA(hFile, lpAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);

  if (pMatchingFileset != NULL)
  {
    pMatchingFileset->m_createFileMappingHandle = handleToReturn;
  }

	return handleToReturn;
}

LPVOID WINAPI BaseFileManager::OnMapViewOfFile(
  __in  HANDLE hFileMappingObject,
  __in  DWORD dwDesiredAccess,
  __in  DWORD dwFileOffsetHigh,
  __in  DWORD dwFileOffsetLow,
  __in  SIZE_T dwNumberOfBytesToMap
  )
{
  ClientFileHandleSet* pMatchingFileset = NULL;

  for (std::map<std::string, ClientFileHandleSet*>::const_iterator iterator = m_files.begin(); iterator != m_files.end(); iterator++)
  {
    if (hFileMappingObject == iterator->second->m_createFileMappingHandle)
    {
      pMatchingFileset = iterator->second;
      break;
    }
  }

  HANDLE handleToReturn = handleToReturn = MapViewOfFile(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap); 

  if (pMatchingFileset != NULL)
  {
    pMatchingFileset->m_mapFileViewHandle = handleToReturn;
  }

	return handleToReturn;
}
