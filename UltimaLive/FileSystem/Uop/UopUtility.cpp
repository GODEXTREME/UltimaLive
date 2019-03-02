/* Copyright (C) 2013 Ian Karlinsey
 * 
 * UltimeLive is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * UltimaLive is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with UltimaLive.  If not, see <http://www.gnu.org/licenses/>. 
 */

#include "UopUtility.h"
#include "..\..\ProgressBarDialog.h"

void UopUtility::convertUopMapToMul(std::string uopSourceFilename, std::string uopDestFilename, ProgressBarDialog* pProgress)
{
  std::ifstream uopSourceFile;
  uopSourceFile.open(uopSourceFilename, std::ios::binary | std::ios::in);

  if (uopSourceFile.is_open())
  {
    std::ofstream mulDestFile;
    mulDestFile.open(uopDestFilename, std::ios::binary | std::ios::out);

    //read the offset to the file table
    uopSourceFile.seekg(12, std::ios::beg);
    uint32_t fileTableOffset = 0;
    uopSourceFile.read(reinterpret_cast<char*>(&fileTableOffset), sizeof(uint32_t));

    uint32_t totalFiles = 0;
    uopSourceFile.seekg(24, std::ios::beg);
    uopSourceFile.read(reinterpret_cast<char*>(&totalFiles), sizeof(uint32_t));

    std::string hashfilename = Utils::getFilenameFromPath(uopSourceFilename);
    hashfilename = Utils::getBaseFilenameWithoutExtension(hashfilename);
    std::transform(hashfilename.begin(), hashfilename.end(), hashfilename.begin(), ::tolower);

    std::map<uint32_t, uint64_t>* pHashes = UopUtility::getMapHashes(totalFiles, hashfilename);

    //seek the first file entry
    char fileEntryBuffer[34];
    uint32_t filePosition = fileTableOffset + 12;
    uopSourceFile.seekg(filePosition, std::ios::beg);
    uopSourceFile.read(fileEntryBuffer, 34);
    std::map<uint64_t, FileEntry*> entries;

    uint32_t totalFileSizeInBytes = 0;

    //read through all the file entries and count the bytes
    while (*reinterpret_cast<uint64_t*>(fileEntryBuffer) != 0)
    {
       FileEntry* pEntry = new FileEntry(); 
       pEntry->unmarshal(reinterpret_cast<uint8_t*>(fileEntryBuffer));
    
       if (entries.find(pEntry->PathChecksum) == entries.end())
       {
         entries[pEntry->PathChecksum] = pEntry;
         totalFileSizeInBytes += pEntry->UncompressedDataSize;
       }

       filePosition += 34;
       uopSourceFile.seekg(filePosition, std::ios::beg);
       uopSourceFile.read(fileEntryBuffer, 34);
    }
    
#ifdef DEBUG
    printf("There are %i file entries in the list\n", entries.size());
#endif
    uint32_t bytesCopied = 0;
    uint32_t prevPercent = 0;

    for(uint32_t i = 0; i < totalFiles; ++i)
    {
      uint64_t hash = (*pHashes)[i];
      FileEntry* pEntry = entries[hash];

      char* pEntryData = new char[pEntry->UncompressedDataSize];
      uopSourceFile.seekg(pEntry->UopFileOffset + pEntry->MetaDataSize, std::ios::beg);
      uopSourceFile.read(pEntryData, pEntry->UncompressedDataSize);
      mulDestFile.write(pEntryData, pEntry->UncompressedDataSize);

      bytesCopied += pEntry->UncompressedDataSize;
      if (pProgress != NULL)
      {
        uint32_t percent = (uint32_t)(((float)bytesCopied / (float)totalFileSizeInBytes) * 100.0f);
        if ((uint32_t)percent > prevPercent)
        {
          prevPercent = percent;
          pProgress->setProgress((uint32_t)percent);
        }
      }

      delete pEntryData;
    }

    delete pHashes;
    mulDestFile.flush();
    mulDestFile.close();
    uopSourceFile.close();
  }
}

/*
  This method assumes that there is only one file table.
*/
uint32_t UopUtility::getUopMapSizeInBytes(std::string filename)
{
  uint32_t totalBytes = 0;
  std::ifstream mapFile;
  mapFile.open(filename, std::ios::binary | std::ios::in);

  if (mapFile.is_open())
  {
    //read the offset to the file table
    mapFile.seekg(12, std::ios::beg);
    uint32_t fileTableOffset = 0;
    mapFile.read(reinterpret_cast<char*>(&fileTableOffset), sizeof(uint32_t));

    //seek the first file entry
    char fileEntryBuffer[34];
    uint32_t filePosition = fileTableOffset + 12;
    mapFile.seekg(filePosition, std::ios::beg);
    mapFile.read(fileEntryBuffer, 34);

    //read through all the file entries and count the bytes
    while (*reinterpret_cast<uint64_t*>(fileEntryBuffer) != 0)
    {
       FileEntry entry; 
       entry.unmarshal(reinterpret_cast<uint8_t*>(fileEntryBuffer));
       totalBytes += entry.UncompressedDataSize;

       if (entry.UncompressedDataSize != entry.CompressedDataSize)
       {
         printf("Size mismatches\n");
       }

       filePosition += 34;
       mapFile.seekg(filePosition, std::ios::beg);
       mapFile.read(fileEntryBuffer, 34);
    }

    mapFile.close();
  }

  return totalBytes;
}

std::string ZeroPadNumber(int num, int fieldWidth)
{
	std::stringstream ss;
	
	// the number is converted to string with the help of stringstream
	ss << num; 
	std::string ret;
	ss >> ret;
	
	// Append zero chars
	int str_length = ret.length();
	for (int i = 0; i < fieldWidth - str_length; i++)
		ret = "0" + ret;
	return ret;
}

std::map<uint32_t, uint64_t>* UopUtility::getMapHashes(int count, std::string pattern)
{
  std::map<uint32_t, uint64_t>* pHashes = new std::map<uint32_t, uint64_t>();

  for (int i = 0; i < count; i++)
  {
    std::string filename("build/");
    filename.append(pattern);
    filename.append("/");
    filename.append(ZeroPadNumber(i, 8));
    filename.append(".dat");

    uint64_t hash = HashFileName(filename);
    if (pHashes->find(i) == pHashes->end())
    {
      (*pHashes)[i] = hash;
    }
  }

  return pHashes;
}

uint64_t UopUtility::HashFileName(std::string s)
{
  uint32_t esi = (uint32_t)s.length() + 0xDEADBEEF;
  uint32_t eax = 0;
  uint32_t ecx = 0;
  uint32_t edx = 0;
  uint32_t ebx = esi;
  uint32_t edi = esi;

  uint32_t i = 0;

  for (i = 0; i + 12 < s.length(); i += 12)
  {
    edi = (uint32_t)((s[i + 7] << 24) | (s[i + 6] << 16) | (s[i + 5] << 8) | s[i + 4]) + edi;
    esi = (uint32_t)((s[i + 11] << 24) | (s[i + 10] << 16) | (s[i + 9] << 8) | s[i + 8]) + esi;
    edx = (uint32_t)((s[i + 3] << 24) | (s[i + 2] << 16) | (s[i + 1] << 8) | s[i]) - esi;

    edx = (edx + ebx) ^ (esi >> 28) ^ (esi << 4);
    esi += edi;
    edi = (edi - edx) ^ (edx >> 26) ^ (edx << 6);
    edx += esi;
    esi = (esi - edi) ^ (edi >> 24) ^ (edi << 8);
    edi += edx;
    ebx = (edx - esi) ^ (esi >> 16) ^ (esi << 16);
    esi += edi;
    edi = (edi - ebx) ^ (ebx >> 13) ^ (ebx << 19);
    ebx += esi;
    esi = (esi - edi) ^ (edi >> 28) ^ (edi << 4);
    edi += ebx;
  }

  if (static_cast<int32_t>(s.length() - i) > 0)
  {
    uint32_t len = s.length() - i;

    if (len > 11)
    {
      esi += (uint32_t)s[i + 11] << 24;
    }

    if (len > 10)
    {
      esi += (uint32_t)s[i + 10] << 16;
    }

    if (len > 9)
    {
      esi += (uint32_t)s[i + 9] << 8;
    }

    if (len > 8)
    {
      esi += (uint32_t)s[i + 8];
    }

    if (len > 7)
    {
      edi += (uint32_t)s[i + 7] << 24;
    }

    if (len > 6)
    {
      edi += (uint32_t)s[i + 6] << 16;
    }

    if (len > 5)
    {
      edi += (uint32_t)s[i + 5] << 8;
    }

    if (len > 4)
    {
      edi += (uint32_t)s[i + 4];
    }

    if (len > 3)
    {
      ebx += (uint32_t)s[i + 3] << 24;
    }

    if (len > 2)
    {
      ebx += (uint32_t)s[i + 2] << 16;
    }

    if (len > 1)
    {
      ebx += (uint32_t)s[i + 1] << 8;
    }

    if (len > 0)
    {
      ebx += (uint32_t)s[i];
    }

    esi = (esi ^ edi) - ((edi >> 18) ^ (edi << 14));
    ecx = (esi ^ ebx) - ((esi >> 21) ^ (esi << 11));
    edi = (edi ^ ecx) - ((ecx >> 7) ^ (ecx << 25));
    esi = (esi ^ edi) - ((edi >> 16) ^ (edi << 16));
    edx = (esi ^ ecx) - ((esi >> 28) ^ (esi << 4));
    edi = (edi ^ edx) - ((edx >> 18) ^ (edx << 14));
    eax = (esi ^ edi) - ((edi >> 8) ^ (edi << 24));

    return ((uint64_t)edi << 32) | eax;
  }

  return ((uint64_t)esi << 32) | eax;
}

