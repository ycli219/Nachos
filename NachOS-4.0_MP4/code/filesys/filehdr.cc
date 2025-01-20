// filehdr.cc
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector,
//
//      Unlike in a real system, we do not keep track of file permissions,
//	ownership, last modification date, etc., in the file header.
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "filehdr.h"
#include "debug.h"
#include "synchdisk.h"
#include "main.h"

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::FileHeader
//	There is no need to initialize a fileheader,
//	since all the information should be initialized by Allocate or FetchFrom.
//	The purpose of this function is to keep valgrind happy.
//----------------------------------------------------------------------
FileHeader::FileHeader()
{
	numBytes = -1;
	numSectors = -1;
	memset(dataSectors, -1, sizeof(dataSectors));
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::~FileHeader
//	Currently, there is not need to do anything in destructor function.
//	However, if you decide to add some "in-core" data in header
//	Always remember to deallocate their space or you will leak memory
//----------------------------------------------------------------------
FileHeader::~FileHeader()
{
	// nothing to do now
}

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

//有改
void FileHeader::MultiLevelAlloc(PersistentBitmap *freeMap, int fileSize, int maxFileSize) {
	int i = 0;
	while(fileSize > 0) {
		// FindAndSet() return the number of the first bit which is clear.
		// As a side effect, set the bit (mark it as in use).
		// In other words, find and allocate a bit.
		dataSectors[i] = freeMap->FindAndSet();

		FileHeader *subHdr = new FileHeader();

		if(fileSize > maxFileSize) {
			subHdr->Allocate(freeMap, maxFileSize);
		}
		else {
			subHdr->Allocate(freeMap, fileSize);
		}

		fileSize = fileSize - maxFileSize;

		// WriteBack()
		// Write the modified contents of the file header back to disk.
		subHdr->WriteBack(dataSectors[i]);
		i++;
	}
}

bool FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)
{
	numBytes = fileSize;
	numSectors = divRoundUp(fileSize, SectorSize);

	// NumClear() Return the number of clear bits in the bitmap.
	// In other words, how many bits are unallocated?
	if (freeMap->NumClear() < numSectors)
		return FALSE;
	
	if(fileSize > MaxFileSize4) {
		MultiLevelAlloc(freeMap, fileSize, MaxFileSize4);
	}
	else if(fileSize > MaxFileSize3) {
		MultiLevelAlloc(freeMap, fileSize, MaxFileSize3);
	}
	else if(fileSize > MaxFileSize2) {
		MultiLevelAlloc(freeMap, fileSize, MaxFileSize2);
	}
	else if(fileSize > MaxFileSize1) {
		MultiLevelAlloc(freeMap, fileSize, MaxFileSize1);
	}
	else {
		for (int i=0 ; i<numSectors ; i++) {
			dataSectors[i] = freeMap->FindAndSet();
			// since we checked that there was enough free space,
			// we expect this to succeed
			ASSERT(dataSectors[i] >= 0);

			char* init = new char[SectorSize]();
			// WriteSector()
			// Write the contents of a buffer into a disk sector.
			kernel->synchDisk->WriteSector(dataSectors[i], init);
			delete init;
		}
	}
	return TRUE;
}
//有改


//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

//有改
void FileHeader::Deallocate(PersistentBitmap *freeMap)
{
	if(numBytes > MaxFileSize1) {
		for(int i=0 ; i<numSectors ; i++) {
			FileHeader *subhdr = new FileHeader();
			subhdr->FetchFrom(dataSectors[i]);
			subhdr->Deallocate(freeMap);
		}
	} 
	else {
		for(int i=0 ; i<numSectors ; i++) {
			// Clear()
			// Clear the "nth" bit in a bitmap.
			freeMap->Clear((int) dataSectors[i]);
		}
	}
}
//有改

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk.
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void FileHeader::FetchFrom(int sector)
{
	kernel->synchDisk->ReadSector(sector, (char *)this);

	/*
		MP4 Hint:
		After you add some in-core informations, you will need to rebuild the header's structure
	*/
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk.
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void FileHeader::WriteBack(int sector)
{
	kernel->synchDisk->WriteSector(sector, (char *)this);

	/*
		MP4 Hint:
		After you add some in-core informations, you may not want to write all fields into disk.
		Use this instead:
		char buf[SectorSize];
		memcpy(buf + offset, &dataToBeWritten, sizeof(dataToBeWritten));
		...
	*/
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

//有改
void FileHeader::ByteToSectorTool(int offset, int maxFileSize) {
	int entry = divRoundDown(offset, maxFileSize);
	FileHeader *subhdr = new FileHeader;
	subhdr->FetchFrom(dataSectors[entry]);
	subhdr->ByteToSector(offset - maxFileSize*entry);
}

int FileHeader::ByteToSector(int offset)
{
	if(numBytes > MaxFileSize4) ByteToSectorTool(offset, MaxFileSize4);
	else if(numBytes > MaxFileSize3) ByteToSectorTool(offset, MaxFileSize3);
	else if(numBytes > MaxFileSize2) ByteToSectorTool(offset, MaxFileSize2);
	else if(numBytes > MaxFileSize1) ByteToSectorTool(offset, MaxFileSize1);
	else return (dataSectors[offset / SectorSize]);
}
//有改

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int FileHeader::FileLength()
{
	return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void FileHeader::PrintTool() {
	for(int i=0 ; i<numSectors ; i++) {
		OpenFile *opfile = new OpenFile(dataSectors[i]);
		FileHeader *subhdr = opfile->getHdr();
		subhdr->Print();
	}
}

void FileHeader::Print()
{
	int i, j, k;
	char *data = new char[SectorSize];

	printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
	for (i = 0; i < numSectors; i++)
		printf("%d ", dataSectors[i]);
	printf("\nFile contents:\n");


	if(numBytes > MaxFileSize1) {
		PrintTool();
	}
	else {

		for (i = k = 0; i < numSectors; i++)
		{
			kernel->synchDisk->ReadSector(dataSectors[i], data);
			for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++)
			{
				if ('\040' <= data[j] && data[j] <= '\176') // isprint(data[j])
					printf("%c", data[j]);
				else
					printf("\\%x", (unsigned char)data[j]);
			}
			printf("\n");
		}
		
	}delete[] data;
}
