#pragma once
#include <vt_dsp/endian.h>

struct riffheader
{
	unsigned long id;
	unsigned long datasize;
};

enum mfseek
{
	mf_FromStart=0,
	mf_FromCurrent,
	mf_FromEnd,
};

__forceinline unsigned short swap_endianW(unsigned short x)
{
	return ((x&0xFF) << 8) | ((x&0xFF00) >> 8);
}

__forceinline unsigned int swap_endianDW(unsigned int x)
{
	return ((x&0xFF) << 24) | ((x&0xFF00) << 8) | ((x&0xFF0000) >> 8) | ((x&0xFF000000) >> 24);
}

// class intended to parse memory mapped RIFF-files 
class memfile
{
public:
	memfile(void *data, int datasize)
	{
		assert(data);
		assert(datasize);
		this->data = (char*)data;
		this->size = datasize;
		loc = 0;
		StartStack.push_front(0);
		EndStack.push_front(datasize);
	}

	virtual char Peek()
	{
		assert(data);
		if((loc<size)&&(loc>=0)) return data[loc];
		return 0;
	}
	
	bool Read(void *buffer, size_t s)
	{
		if((s + loc)>size) return false;
		memcpy(buffer,data + loc, s);
		loc += s;
		return true;
	}

	bool SkipPSTRING()
	{
		unsigned char length = (*(unsigned char*)(data + loc)) + 1;
		if(length&1) length++; // IFF 16-bit alignment
		if((length + loc)>size) return false;
		loc += length;
		return true;
	}

	unsigned long ReadDWORD()
	{		
		if((4 + loc)>size) return 0;
		unsigned long val = *(unsigned long*)(data + loc);		
		loc += 4;
		return val;
	}

	unsigned long ReadDWORDBE()
	{		
		if((4 + loc)>size) return 0;
		unsigned long val = *(unsigned long*)(data + loc);		
		loc += 4;
		return vt_read_int32BE(val);
	}
	
	void WriteDWORD(unsigned long dw)
	{		
		if((4 + loc)>size) return;
		*(unsigned long*)(data + loc) = dw;
		loc += 4;		
	}	

	void WriteDWORDBE(unsigned long dw)
	{		
		if((4 + loc)>size) return;
		*(unsigned long*)(data + loc) = vt_write_int32BE(dw);
		loc += 4;		
	}	
	
	unsigned short ReadWORD()
	{		
		if((2 + loc)>size) return 0;
		unsigned short val = *(unsigned short*)(data + loc);		
		loc += 2;
		return val;
	}

	void* ReadPtr(size_t s)		// moves loc like read, but it returns a pointer to the memory instead of copying it
	{
		if((s + loc)>size) return 0;
		void *ptr = data + loc;
		loc += s;
		return ptr;
	}

	void* GetPtr()
	{
		void *ptr = data + loc;
		return ptr;
	}

	size_t TellI() 
	{
		return loc;
	}

	size_t SeekI(size_t pos, int mode = mf_FromStart)
	{	
		switch(mode)
		{
		case mf_FromStart:
			loc = pos;
			break;

		case mf_FromCurrent:
			loc += pos;
			break;

		case mf_FromEnd:
			loc = size + pos;
			break;
		}
		return loc;
	}	

	bool riff_descend_RIFF_or_LIST(int tag,size_t *datasize, unsigned int entry=0)
	{	
		char *cv = (char*)&tag;
		int rtag = (cv[0]<<24) + (cv[1]<<16) + (cv[2]<<8) + cv[3];		
		assert((loc & 1) == 0);	// assure block unsigned short alignment (2-bytes)
		riffheader rh;	
		while(!Eob())
		{
			if (!Read(&rh,sizeof(riffheader))) return false;
			if((rh.id == 'FFIR')||(rh.id == 'TSIL'))
			{
				int subtag;				
				if (!Read(&subtag,4)) return false;
				if(subtag == rtag)
				{
					if(entry)
					{
						entry--;
						SeekI(-4,mf_FromCurrent);
					}
					else
					{
						if(datasize) *datasize = rh.datasize-4;	// don't include the subid						
						return true;					
					}
				}				
				else
				{
					SeekI(-4,mf_FromCurrent);
				}
			}
			
			if (!SeekI(rh.datasize + (rh.datasize&1),mf_FromCurrent)) return false;			
			// +1 if odd to align on unsigned short			
		}
		return false;
	}

	bool iff_descend_FORM(int tag,int *datasize)
	{			
		char *cv = (char*)&tag;
		int rtag = (cv[0]<<24) + (cv[1]<<16) + (cv[2]<<8) + cv[3];		

		assert((loc & 1) == 0);	// assure block unsigned short alignment (2-bytes)
		riffheader rh;	
		while(!Eof())
		{
			if (!Read(&rh,sizeof(riffheader))) return false;
			rh.datasize = swap_endianDW(rh.datasize);
			if(rh.id == 'MROF')
			{
				unsigned long subtag;				
				if (!Read(&subtag,4)) return false;
				if(subtag == rtag)
				{
					if(datasize) *datasize = rh.datasize;	// don't include the subid
					return true;					
				}
				else
				{
					SeekI(-4,mf_FromCurrent);
				}
			}
			
			if (!SeekI(rh.datasize + (rh.datasize&1),mf_FromCurrent)) return false;			
			// +1 if odd to align on unsigned short			
		}
		return false;
	}


	bool riff_descend(int tag,size_t *datasize)
	{	
		char *cv = (char*)&tag;
		unsigned int rtag = (cv[0]<<24) + (cv[1]<<16) + (cv[2]<<8) + cv[3];		
		assert((loc & 1) == 0);	// assure block unsigned short alignment (2-bytes)
		riffheader rh;	
		while(!Eob())
		{
			if (!Read(&rh,sizeof(riffheader))) return false;			
			if(rh.id == rtag)
			{
				if(datasize) *datasize = rh.datasize;
				return true;
			}
			
			if (!SeekI(rh.datasize + (rh.datasize&1),mf_FromCurrent)) return false;			
			// +1 if odd to align on unsigned short
			
		}
		return false;
	}
	
	bool riff_descend(int *tag,size_t *datasize)
	{			
		assert((loc & 1) == 0);	// assure block unsigned short alignment (2-bytes)
		riffheader rh;	
				
		if (!Read(&rh,sizeof(riffheader))) return false;		
		
		if(tag) *tag = vt_read_int32BE(rh.id);
		if(datasize) *datasize = rh.datasize;
		return true;									
	}

	// NEW Start

	bool RIFFPeekChunk(int *Tag=0, size_t *DataSize=0, bool *HasSubchunks=0, int *LISTTag=0)		// Get the Tag & size without affecting the locator
	{
		assert((loc & 1) == 0);	// assure block unsigned short alignment (2-bytes)
		if((loc + 8) > EndStack.front()) return false;
		int ChunkTag = vt_read_int32BE(*(int*)(data + loc));
		if (Tag) *Tag = ChunkTag;
		size_t dataSize = vt_read_int32LE(*(int*)(data + loc + 4));
		if (DataSize) *DataSize = dataSize;
		bool hasSubchunks = (ChunkTag == 'RIFF') || (ChunkTag == 'LIST');
		if (HasSubchunks) *HasSubchunks = hasSubchunks;	
		if(LISTTag)
		{			
			if(hasSubchunks) *LISTTag = vt_read_int32BE(*(int*)(data + loc + 8));
			else *LISTTag = 0;
		}	

		if((loc + 8 + dataSize) > EndStack.front()) 
		{
			invalid();
			return false;
		}

		return true;
	}	

	void invalid()
	{		
#if !MAC
		char txt[1024];
		sprintf(txt,"Invalid RIFF-structure.\n\nOffset: 0x%X",loc);
		MessageBox(::GetActiveWindow(),txt,"File I/O Error",MB_OK | MB_ICONERROR);
#endif
	}

	bool RIFFIsContainer()		// Returns true if the following Chunk is RIFF or LIST
	{
		assert((loc & 1) == 0);	// assure block unsigned short alignment (2-bytes)
		if((loc + 8) > EndStack.front()) return false;
		int ChunkTag = vt_read_int32BE(*(int*)(data + loc));
		return (ChunkTag == 'RIFF') || (ChunkTag == 'LIST');
	}

	void* RIFFReadChunk(int *Tag=0, size_t *DataSize=0)		// Read chunk & forward locator
	{
		assert((loc & 1) == 0);	// assure block unsigned short alignment (2-bytes)
		if((loc + 8) > EndStack.front()) return 0;		
		size_t chunksize = vt_read_int32LE(*(int*)(data + loc + 4));
		if (Tag) *Tag = vt_read_int32BE(*(int*)(data + loc));
		if (DataSize) *DataSize = size;
		void *dataptr = data + loc + 8;
		if((loc + 8 + chunksize) > EndStack.front()) 
		{
			invalid();
			return nullptr;
		}		
		loc += 8 + chunksize;
		if(loc & 1) loc++;		// Add 1 if odd
		return dataptr;
	}

	bool RIFFSkipChunk(int *Tag=0, size_t *DataSize=0)
	{
		assert((loc & 1) == 0);	// assure block unsigned short alignment (2-bytes)
		if((loc + 8) > EndStack.front()) return false;		
		size_t ChunkSize = vt_read_int32LE(*(int*)(data + loc + 4));
		if (Tag) *Tag = vt_read_int32BE(*(int*)(data + loc));
		if (DataSize) *DataSize = ChunkSize;		
		if((loc + 8 + ChunkSize) > EndStack.front())
		{
			invalid();
			return false;
		}
		loc += 8 + ChunkSize;
		if(loc & 1) loc++;		// Add 1 if odd
		return true;
	}

	bool RIFFAscend(bool Rewind = false)
	{		
		assert(!(StartStack.empty() || EndStack.empty()));

		if (!Rewind) loc = EndStack.front();
		
		StartStack.pop_front();
		EndStack.pop_front();

		assert(!(StartStack.empty() || EndStack.empty()));
		
		if (Rewind) loc = StartStack.front();

		return true;
	}

	bool RIFFDescend(int *LISTtag=0,size_t *datasize=0)	// Descend into the chunk at the current location
	{
		assert((loc & 1) == 0);	// assure block unsigned short alignment (2-bytes)
		if((loc + 12) > EndStack.front()) return false;		
		int Tag = vt_read_int32BE(*(int*)(data + loc));
		if((Tag != 'LIST')&&(Tag != 'RIFF')) return false;
		size_t chunksize = vt_read_int32LE(*(int*)(data + loc + 4));
		int LISTTag = vt_read_int32BE(*(int*)(data + loc + 8));
		loc += 12;		
		
		StartStack.push_front(loc);
		EndStack.push_front(loc + chunksize - 4);

		if(datasize) *datasize = chunksize-4;
		if(LISTtag) *LISTtag = LISTTag;
		return true;
	}

	bool RIFFDescendSearch(int tag,size_t *datasize=0, unsigned int entry=0)	// Descend into a chunk by searching for it (sequentially if desired)
	{			
		assert((loc & 1) == 0);	// assure block unsigned short alignment (2-bytes)
		riffheader rh;	
		while(!Eob())
		{
			if (!Read(&rh,sizeof(riffheader))) return false;
			rh.id = vt_read_int32BE(rh.id);
			rh.datasize = vt_read_int32LE(rh.datasize);
			if((rh.id == 'RIFF')||(rh.id == 'LIST'))
			{
				unsigned int subtag;				
				if (!Read(&subtag,4)) return false;
				subtag = vt_read_int32BE(subtag);
				if(subtag == tag)
				{
					if(entry)
					{
						entry--;
						SeekI(-4,mf_FromCurrent);
					}
					else
					{
						if(datasize) *datasize = rh.datasize-4;	// don't include the subid
						StartStack.push_front(loc);
						EndStack.push_front(loc + rh.datasize - 4);
						return true;					
					}
				}				
				else
				{
					SeekI(-4,mf_FromCurrent);
				}
			}
			
			if (!SeekI(rh.datasize + (rh.datasize&1),mf_FromCurrent)) return false;			
			// +1 if odd to align on unsigned short			
		}
		return false;
	}

	unsigned int RIFFGetFileType()	// Read the ID of the RIFF chunk
	{
		return vt_read_int32BE(*(unsigned int*)(data+8));
	};

	bool RIFFCreateChunk(unsigned long tag, void *indata, size_t datasize)
	{
		if((8 + loc + datasize)>size) 
		{
			invalid();
			return false;
		}
		*(unsigned long*)(data + loc) = vt_write_int32BE(tag);		loc +=4;
		*(unsigned long*)(data + loc) = vt_write_int32LE(datasize);	loc +=4;

		memcpy(data + loc, indata, datasize);
		loc += datasize;

		return true;
	}		

	bool RIFFCreateLISTHeader(unsigned long tag, size_t subsize)
	{
		if((12 + loc + subsize)>size) 
		{
			invalid();
			return false;
		}
		*(unsigned long*)(data + loc) = vt_write_int32BE('LIST');		
		*(unsigned long*)(data + loc + 4) = vt_write_int32LE(subsize+4);
		*(unsigned long*)(data + loc + 8) = vt_write_int32BE(tag);		
		loc +=12;
		return true;
	}
	
	static size_t RIFFTextChunkSize(char *txt)
	{
		if(!txt) return 0;
		if(!txt[0]) return 0;
		size_t datasize = strlen(txt)+1;
		if(datasize&1) datasize++;
		return datasize + 8;
	}	
	
	bool RIFFCreateTextChunk(unsigned long tag, char *txt)
	{
		// skip chunk if string is null/empty
		if(!txt) return true;		
		if(!txt[0]) return true;

		size_t datasize = strlen(txt)+1;	// Make room for the terminating \0
		if(datasize&1) datasize++;			// Force 2-byte alignment
		
		if((8 + loc + datasize)>size) 
		{
			invalid();
			return false;
		}
		*(unsigned long*)(data + loc) = vt_write_int32BE(tag);		
		*(unsigned long*)(data + loc + 4) = vt_write_int32LE(datasize);	
		loc +=8;

		memset(data + loc, 0, datasize);
		strncpy(data + loc, txt, datasize);
		loc += datasize;

		return true;
	}	

	// New End

	// File I/O

	bool LoadFile(wchar_t *FileName)
	{
		assert(0);
		return false;
	}
	bool SaveFile(wchar_t *FileName)
	{
		assert(0);
		return false;
	}

	bool iff_descend(int tag,int *datasize)
	{	
		char *cv = (char*)&tag;
		int rtag = (cv[0]<<24) + (cv[1]<<16) + (cv[2]<<8) + cv[3];		
		assert((loc & 1) == 0);	// assure block unsigned short alignment (2-bytes)
		riffheader rh;	
		while(!Eof())
		{
			if (!Read(&rh,sizeof(riffheader))) return false;			
			rh.datasize = swap_endianDW(rh.datasize);
			if(rh.id == rtag)
			{
				if(datasize) *datasize = rh.datasize;
				return true;
			}
			
			if (!SeekI(rh.datasize + (rh.datasize&1),mf_FromCurrent)) return false;			
			// +1 if odd to align on unsigned short
			
		}
		return false;
	}


	bool Eof()
	{
		return (loc>size);
	}	
	bool Eob()
	{
		return (loc>size) || (loc>EndStack.front());
	}
private:
	size_t loc,size;
	std::list<size_t> StartStack;
	std::list<size_t> EndStack;
	char *data;
};
