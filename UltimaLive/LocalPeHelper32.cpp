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

#include "LocalPeHelper32.hpp"
using namespace std;

LocalPeHelper32::LocalPeHelper32(std::string moduleName)
  : m_initialized(false),
  m_pBaseAddress(NULL),
  m_moduleName(moduleName),
  m_pDosHeader(NULL),
  m_pNtHeader(NULL),
  m_pExportDirectory(NULL),
  m_pImportDescriptor(NULL)
{
  //do nothing
}

std::string toUpper(const std::string & s)
{
    std::string ret(s.size(), char());
    for(unsigned int i = 0; i < s.size(); ++i)
        ret[i] = (s[i] <= 'z' && s[i] >= 'a') ? s[i]-('a'-'A') : s[i];
    return ret;
}

bool LocalPeHelper32::Init()
{
  std::list<MODULEENTRY32> localModules = GetLocalModuleList();

  for(std::list<MODULEENTRY32>::iterator i = localModules.begin(); i != localModules.end(); ++i)
  {
    if (toUpper(i->szModule) == toUpper(m_moduleName))
    {
      m_pBaseAddress = i->modBaseAddr;
      m_initialized = true;
    }
  }

  if (m_initialized == true)
  {
    m_pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(m_pBaseAddress);
    m_pNtHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(m_pBaseAddress + m_pDosHeader->e_lfanew);
    m_pExportDirectory = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(m_pBaseAddress + 
	    m_pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    m_pImportDescriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(m_pBaseAddress + 
      m_pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
  }

  return m_initialized;
}

void LocalPeHelper32::HexPrint(unsigned char* buffer, int size)
{
  char asciiLine[17] = { };
  asciiLine[16] = NULL;

  for ( int i = 0; i < 16; i++)
  {
    asciiLine[i] = '.';
  }

  int i = 0;
  for (i = 0; i < size; i++)
  {
    if (i == 0 || ((i % 16) == 0)) 
    {
      printf(" %0.8x  |", (buffer + i));
    }
    else if ((i % 8) == 0) 
    {
      printf("  ");
    }
    
    printf(" %0.2x", buffer[i]);

    if (buffer[i] > 32 && buffer[i] < 127)
    {
      asciiLine[i % 16] = (char)buffer[i];
    }
    else
    {
      asciiLine[i % 16] = '.';
    }

    if (i > 0 && ((i + 1) % 16) == 0)
    {
      printf(" | %s |\n", asciiLine);
      for ( int i = 0; i < 16; i++)
      {
        asciiLine[i] = '.';
      }
    }
  }

  if (i % 16 != 0)
  {
    for (int j = i % 16; j < 16; j++)
    {
      printf("   ");
      asciiLine[j] = ' ';
      if ((j % 8) == 0) 
      {
        printf("  ");
      }
    }
    printf(" | %s |\n", asciiLine);

  }

  printf("\n");
}

void LocalPeHelper32::HexPrint(char* buffer, int size)
{
  char asciiLine[16] = { };
  for ( int i = 0; i < 16; i++)
  {
    asciiLine[i] = '.';
  }

  for (int i = 0; i < size; i++)
  {
    if (i == 0 || ((i % 16) == 0)) 
    {
      printf(" %0.8x  |", (buffer + i));
    }
    else if ((i % 8) == 0) 
    {
      printf("  ");
    }
    
    printf(" %0.2x", (unsigned char)buffer[i]);

    if ((unsigned char)buffer[i] > 32 && (unsigned char)buffer[i] < 127)
    {
      asciiLine[i % 16] = buffer[i];
    }
    else
    {
      asciiLine[i % 16] = '.';
    }

    if (i > 0 && ((i + 1) % 16) == 0)
    {
      printf(" | %s |\n", asciiLine);
      for ( int i = 0; i < 16; i++)
      {
        asciiLine[i] = '.';
      }
    }
  }
  printf("\n");
}

std::list<MODULEENTRY32> LocalPeHelper32::GetLocalModuleList()
{
  std::list<MODULEENTRY32> resultList;
    
  //get a snapshot of the process
  HANDLE hModuleSnapshot;
  MODULEENTRY32 moduleEntry;
  hModuleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
  if (hModuleSnapshot != INVALID_HANDLE_VALUE)
  {
    moduleEntry.dwSize = sizeof(MODULEENTRY32);
    if(Module32First( hModuleSnapshot, &moduleEntry ) ) 
    { 
      do 
      { 
        resultList.push_back(moduleEntry);
      } while( Module32Next( hModuleSnapshot, &moduleEntry ) );
    }
  }
  return resultList;
}

DWORD LocalPeHelper32::getImportedFunctionAddress(std::string dllName, std::string functionName)
{
  DWORD result = NULL;

  bool found = false;
  IMAGE_IMPORT_DESCRIPTOR* pDescriptorItr = matchImportDescriptor(dllName);

  if (pDescriptorItr != NULL)
  {
    DWORD* pImportItrAddr = reinterpret_cast<DWORD*>(pDescriptorItr->FirstThunk + (DWORD)m_pBaseAddress);
    for (DWORD* pLookupItrAddr = reinterpret_cast<DWORD*>(pDescriptorItr->OriginalFirstThunk + (DWORD)m_pBaseAddress); 
      *pLookupItrAddr != NULL; pLookupItrAddr++)
    {
        if ((*pLookupItrAddr & 0x80000000) == 0x00000000) //lookup by ordinal
        {
          DWORD hintAddress = (DWORD)(*pLookupItrAddr & ~0x80000000) + (DWORD)m_pBaseAddress;
          string currentFunctionName = std::string(reinterpret_cast<char*>(hintAddress + 2));

          if (toUpper(currentFunctionName) == toUpper(functionName))
          {
            result = *pImportItrAddr;
            break;
          }          
        }
        pImportItrAddr++;
    }
  }

  return result;
}

DWORD LocalPeHelper32::getExportedFunctionAddress(std::string dllName, std::string functionName)
{
  DWORD addressValue = NULL;

  WORD* nameOrdinals = reinterpret_cast<WORD*>(m_pExportDirectory->AddressOfNameOrdinals + (DWORD)m_pBaseAddress);
  char** nameAddresses = reinterpret_cast<char**>(m_pExportDirectory->AddressOfNames + (DWORD)m_pBaseAddress);

  if (nameOrdinals != NULL)
  {
    for (unsigned int i = 0; i < m_pExportDirectory->NumberOfNames; i++)
    {
      WORD ordinal = nameOrdinals[i];
      char* pName = reinterpret_cast<char*>(nameAddresses[ordinal] + (DWORD)m_pBaseAddress);
      string functionName(pName);
      if (toUpper(pName) == toUpper(functionName))
      {
        DWORD* addresses = reinterpret_cast<DWORD*>(m_pExportDirectory->AddressOfFunctions + (DWORD)m_pBaseAddress);
        addressValue = addresses[ordinal];
      }
    }
  }

  return addressValue;
}


bool LocalPeHelper32::PatchImportedFunctionAddress(std::string dllName, std::string functionName, DWORD newAddress)
{
  bool result = false;
  bool found = false;
  IMAGE_IMPORT_DESCRIPTOR* pDescriptorItr = matchImportDescriptor(dllName);

  if (pDescriptorItr != NULL)
  {
    DWORD* pImportItrAddr = reinterpret_cast<DWORD*>(pDescriptorItr->FirstThunk + (DWORD)m_pBaseAddress);
    for (DWORD* pLookupItrAddr = reinterpret_cast<DWORD*>(pDescriptorItr->OriginalFirstThunk + (DWORD)m_pBaseAddress); 
      *pLookupItrAddr != NULL; pLookupItrAddr++)
    {
        if ((*pLookupItrAddr & 0x80000000) == 0x00000000) //lookup by ordinal
        {
          DWORD hintAddress = (DWORD)(*pLookupItrAddr & ~0x80000000) + (DWORD)m_pBaseAddress;
          string currentFunctionName = std::string(reinterpret_cast<char*>(hintAddress + 2));

          if (toUpper(currentFunctionName) == toUpper(functionName))
          {
            if (SetMemoryWritable((DWORD)pImportItrAddr) == 0)
            {
              *pImportItrAddr = newAddress;
              result = true;
              break;
            }
          }          
        }
        pImportItrAddr++;
    }
  }

  return result;
}

DWORD LocalPeHelper32::SetMemoryWritable(DWORD startingAddress)
{
  HANDLE hProcess = GetCurrentProcess();

  DWORD errorCode = 0;

  //get the page baseaddress of the page where our address resides
  MEMORY_BASIC_INFORMATION mem_info;
  DWORD old_protections = 0;

  // VirtualQueryEx
  // The return value is the actual number of bytes returned in the information buffer.
  // If the function fails, the return value is zero. 
	DWORD retval = VirtualQueryEx(hProcess, (LPCVOID)startingAddress, &mem_info, sizeof(MEMORY_BASIC_INFORMATION));

	if (retval == 0)
	{
    errorCode = GetLastError();
    printf("VirtualQueryEx failed, error code: %i\n", errorCode);
    printf("VirtualQueryEx failed, retvalue: %i\n", retval);
    
	}
  else if(VirtualProtectEx(hProcess, mem_info.BaseAddress, mem_info.RegionSize, PAGE_READWRITE, &old_protections) == 0)
	{
    errorCode = GetLastError();
    printf("VirtualProtectEx failed: %i\n", errorCode);
    printf("param 1: %x\n", hProcess);
    printf("param 2: %x\n", mem_info.BaseAddress);
    printf("param 3: %x\n", mem_info.RegionSize);
    printf("param 4: %x\n", PAGE_READWRITE);
    printf("param 5: %x\n", old_protections);

	}

	return errorCode;
}

IMAGE_IMPORT_DESCRIPTOR* LocalPeHelper32::matchImportDescriptor(std::string dllName)
{
  IMAGE_IMPORT_DESCRIPTOR* pResult = NULL;

  for (IMAGE_IMPORT_DESCRIPTOR* pDescriptorItr = m_pImportDescriptor; pDescriptorItr != NULL; pDescriptorItr++)
  {
    string s(reinterpret_cast<char*>(pDescriptorItr->Name + m_pBaseAddress));
    if (toUpper(s) == toUpper(dllName))
    {
      pResult = pDescriptorItr;
      break;
    }
  }

  return pResult;
}

list<string> LocalPeHelper32::getExportedFunctionNames()
{
  std::list<std::string> functionNames;

  WORD* nameOrdinals = reinterpret_cast<WORD*>(m_pExportDirectory->AddressOfNameOrdinals + (DWORD)m_pBaseAddress);
  char** nameAddresses = reinterpret_cast<char**>(m_pExportDirectory->AddressOfNames + (DWORD)m_pBaseAddress);

  if (nameOrdinals != NULL)
  {
    for (uint32_t i = 0; i < m_pExportDirectory->NumberOfNames; i++)
    {
      WORD ordinal = nameOrdinals[i];
      char* pName = reinterpret_cast<char*>(nameAddresses[ordinal] + (DWORD)m_pBaseAddress);
      string functionName(pName);
      functionNames.push_back(functionName);
    }
  }

  return functionNames;
}

list<string> LocalPeHelper32::getImportedFunctionNames(std::string dllName)
{
  std::list<std::string> functionNames;
  bool found = false;

  IMAGE_IMPORT_DESCRIPTOR* pDescriptorItr = matchImportDescriptor(dllName);

  if (pDescriptorItr != NULL)
  {  
    for (DWORD* pLookupItrAddr = reinterpret_cast<DWORD*>(pDescriptorItr->OriginalFirstThunk + (DWORD)m_pBaseAddress); 
      *pLookupItrAddr != NULL; pLookupItrAddr++)
    {
        if ((*pLookupItrAddr & 0x80000000) != 0x00000000) //lookup by ordinal
        {
          char buffer [64];
          string functionName = string("ORDINAL_");
          functionName.append(itoa(*pLookupItrAddr & 0x0000FFFF, buffer, 10));
        }
        else
        {
          DWORD hintAddress = (DWORD)(*pLookupItrAddr & ~0x80000000) + (DWORD)m_pBaseAddress;
          string functionName = std::string(reinterpret_cast<char*>(hintAddress + 2));
          functionNames.push_back(functionName);
        }
    }
  }

  return functionNames;
}
