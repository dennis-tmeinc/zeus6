#include <errno.h>
#include "mpheader.h"
#include "dvr.h"
#include "dir.h"

static int file_bufsize;								// file buffer size
static int file_encrypt ;

static volatile int file_busy = 0 ;						// busy counter, 

unsigned char g_filekey[260] ;
static unsigned char file_encrypt_RC4_table[1024] ;		// RC4 encryption table

dvrfile::dvrfile()
{
    m_handle = NULL;
    m_khandle = NULL;
}

dvrfile::~dvrfile()
{
    close();
}

int dvrfile::open(const char *filename, char *mode, int initialsize, int encrypt)
{
    close();

    m_filename=filename ;
    f264time(filename, &m_filetime);
    m_openmode=0;
	m_curframe=0 ;
	m_fileencrypt=file_encrypt ;    		// default enc
	m_lastframetime = 0 ;
    if( *mode == 'w' ) {
		
		struct stat fstat ;
		if( stat(filename, &fstat)==0 ) {		// file existed!
			m_handle = fopen(filename, "r+");	// open as read+write
			seek(0, SEEK_END) ;
			m_filesize = tell();			
		}
		else {
			m_handle = fopen(filename, mode);
			m_filesize = 0 ;
		}
		
		if (m_handle == NULL) {
			dvr_log("open file:%s failed",filename);
			return 0;
		}

        setvbuf(m_handle, (char *)NULL, _IOFBF, file_bufsize);

		// prepare header
		int channel = f264channel(filename) ;
		memcpy( &m_fileheader, Dvr264Header, sizeof(m_fileheader));
		if( channel>=0 && channel<cap_channels ) {
			struct File_format_info *pFileHead=(struct File_format_info*)&m_fileheader;
			pFileHead->video_height=cap_channel[channel]->getHeight();
			pFileHead->video_width=cap_channel[channel]->getWidth();
			pFileHead->video_framerate=cap_channel[channel]->getFrameRate();
		}
		
		if( m_fileencrypt ) {
			RC4_block_crypt( (unsigned char*)&m_fileheader, sizeof(m_fileheader), 0, file_encrypt_RC4_table, 1024);
		}
					
        m_openmode = 1;

		if( m_filesize<1000 ) {    
			// consider this is a new file
			m_filesize = 0 ;
			
			if (initialsize > 0) {
				truncate(initialsize);
			}

			// to write file header
	        seek(0);
			write( &m_fileheader, sizeof(m_fileheader));				// write header
		}
		else {
			// append data
			seek(0, SEEK_END) ;
		}
        
        //open the k file
		string keyfilename(m_filename) ;
		char * pk = (char *)keyfilename ;
		int l=strlen(pk);
		if( l>24 && strcmp( pk+l-4, ".266")==0 ) {
			strcpy( pk+l-4,".k");
			m_khandle=fopen( pk, "a" );
			setvbuf(m_khandle, NULL, _IONBF, 0) ;			
		}
    }
    else if( *mode == 'r' ) {
		
	    m_handle = fopen(filename, mode);
		if (m_handle == NULL) {
			dvr_log("open file:%s failed",filename);
			return 0;
		}
        seek(0, SEEK_END) ;
        m_filesize = tell(); 
        if( m_filesize<1000 ) {
			close();
			return 0;
		}

		seek(0);
        if( read(&m_fileheader, sizeof(m_fileheader) )!=sizeof(m_fileheader) ) {
			close();
			return 0;
		}
		
        if( m_fileheader.flag == 0x544d4546 ) {
            m_fileencrypt=0 ;	// no encrypted
        }
        else {
			m_fileencrypt=1 ;
        }
         
		// read key frame index
        readkey(m_keyarray);
        if( m_keyarray.size()<=1 || m_filesize < m_keyarray[ m_keyarray.size()-1 ].koffset ) {
			close();
		}
    }
	return isopen();
}

void dvrfile::close()
{
	if( m_khandle ) {
		fclose(m_khandle);
		m_khandle=NULL;
	}

	if( m_handle ) {
		
		flush();
		
        if( m_openmode ) {  // open for write
            int wsize = tell();
            seek(0, SEEK_END);
            int fsize = tell();
            if (fsize > wsize) {
                truncate(wsize);
            }
        }

		if( fclose(m_handle)!=0 ) {
			dvr_log("Close file failed : %s %d",m_filename.getstring(),errno);
		}
		
        m_handle = NULL;
    }

	m_keyarray.empty();
}

int dvrfile::read(void *buffer, size_t buffersize)
{
    if( m_handle && buffer && buffersize>0 ) {
		flush();		
        return fread(buffer, 1, buffersize, m_handle);
    }
    else {
        return 0 ;
    }
}

int dvrfile::write(void *buffer, size_t buffersize)
{
    if( m_handle && buffer && buffersize>0 ) {
		return fwrite(buffer, 1, buffersize, m_handle);	
	}
	return 0;
}
        
void dvrfile::flush()
{
}

int dvrfile::tell() 
{
    if( m_handle ) {
        return ftell(m_handle);
    }
    else {
        return 0;
    }
}

int dvrfile::seek(int pos, int from) 
{
    if( m_handle ) {
		flush();
        return fseek(m_handle, pos, from);
    }
    else {
        return 0 ;
    }
}

int dvrfile::truncate( int tsize )
{
    int res = 0 ;
    if( m_handle ) {
		flush();
		fflush(m_handle);
		res=ftruncate(fileno(m_handle), tsize);
	}
    return res ;
}

int dvrfile::readheader(void * buffer, size_t headersize) 
{
    if( headersize<sizeof(m_fileheader)  ) {
        return 0 ;
    }

	if( m_handle ) {
		memcpy( buffer, &m_fileheader, sizeof(m_fileheader) );
	}
	else {
		memcpy( buffer, Dvr264Header, sizeof(m_fileheader));
		if( file_encrypt ) {
			RC4_block_crypt( (unsigned char*)buffer, sizeof(m_fileheader), 0, file_encrypt_RC4_table, 1024);
		}		
	}
    
	return sizeof(m_fileheader) ;
}

int dvrfile::writeheader(void * buffer, size_t headersize) 
{
    int framepos ;
    
    if( m_handle == NULL || headersize<sizeof(struct hd_file) ) {
        return 0 ;
    }
    
    // save old file pointer
    framepos = tell();
    seek(0);
    write( buffer, sizeof(struct hd_file));
    seek( framepos );
    return sizeof( struct hd_file );
}


// write a new frame, return 1 success, 0: failed
int dvrfile::writeframe( rec_fifo * frame )
{
	return writeframe( (unsigned char * )frame->buf, (int)frame->bufsize, frame->frametype, &(frame->time) );
}

// write a new frame, return 1 success, 0: failed
int dvrfile::writeframe(unsigned char * buf, int size, int frametype, dvrtime * frametime)
{
	if( frametype == FRAMETYPE_KEYVIDEO && m_khandle ) {
		m_curframe++ ;
		m_lastframetime = time_dvrtime_diffms(frametime, &m_filetime) ;		// if size==0, then this is then ending frame
		if( size>0 ) {
			fprintf(m_khandle,"%d,%d\n", 
				m_lastframetime ,
				tell() );
		}
	}
	
	if( buf && size>0 ) {
		if( m_fileencrypt ) {
			int peslen ;
			int peshlen ;
			while( size > 0 ) {
				if( buf[0]==0 && buf[1]==0 && buf[2]==1 ) {
					// AV frame
					if (buf[3] == 0xc0 || buf[3] == 0xe0 ) {
						// audio / video PES frame
						peslen = (((unsigned int)buf[4])<<8) |
								((unsigned int)buf[5]) ;
						peshlen = (((unsigned int)buf[8]) & 0xff) + 9 ;
						if( peslen > 0 ) {
							peslen += 6 ;
						}
						else {
							peslen = (((unsigned int)buf[15])<<24) |
								(((unsigned int)buf[16])<<16) |
								(((unsigned int)buf[17])<<8) |
								((unsigned int)buf[18]) ;
							peslen += peshlen ;
						}
						if( peslen > size ) {
							peslen = size ;
						}
						
						if( buf[3] == 0xe0 ) {			//video frame
							
							// look for tag 00 00 00 01 25/21/65/61
							while( (peslen-peshlen)>10 ) {
								if( buf[peshlen] == 0 &&
									buf[peshlen+1] == 0 &&
									buf[peshlen+2] == 1 &&
									(buf[peshlen+3] == 0x21 || buf[peshlen+3] == 0x25 || buf[peshlen+3] == 0x61 || buf[peshlen+3] == 0x65) ) 
								{
									peshlen += 4 ;
									break ;
								}
								peshlen++ ;
							}
						
						}
					
						// save header
						if( peshlen > 0 ) {
							write( buf, peshlen );
							buf+=peshlen ;
							size-=peshlen ;		
							peslen-=peshlen ;
						}
						
						// save encrypted parts
						if( peslen>0 ) {
							peshlen = peslen ;						
							if( peshlen > 1024 ) {
								peshlen = 1024 ;
							}
							
							// do encryption
							unsigned char xbuf[peshlen] ;						
							memcpy( xbuf, buf, peshlen );
							RC4_block_crypt( xbuf, peshlen, 0, file_encrypt_RC4_table, 1024);
							write( xbuf, peshlen );
							
							buf+=peshlen ;
							size-=peshlen ;	
							peslen-=peshlen ;
						}
					}
					else if( buf[3] == 0xba ) {
						// skip PS header, usually 20 bytes
						peslen = 14 + (buf[13] & 0x7) ;
					}
					else {
						// other PES, save it all
						peslen = ((((unsigned int)buf[4])<<8) |
						 ((unsigned int)buf[5])) + 6 ;
					}
				}
				else if( ( buf[0]=='T' && buf[1]=='X' && buf[2]=='T' ) ||		// TXT
							( buf[0]=='G' && buf[1]=='P' && buf[2]=='S' ) )		// GPS  
				{
					// text/gps len is little endian
					peslen = ((((unsigned int)buf[7])<<8) |
						 ((unsigned int)buf[6])) + 8 ;
				}
				else {
					// write everything else as is
					peslen = size ;
				}

				if( peslen>0 ) {
					if( peslen>size )
						peslen = size ;
											
					write( buf, peslen );
					buf+=peslen ;
					size-=peslen ;		
				}	
									
			}
			return 1 ;
		}
		else {
			if( write( buf, size )==(int)size ) {
				return 1;
			}
		}
	}
	return 0 ;	
}


// read current frame , if success advance to next frame after read
int dvrfile::readframe( frame_info * fi )
{
	int framesize = 0 ;
	if( isopen() && m_curframe<m_keyarray.size() && m_curframe>=0 ) {
		if( m_curframe < m_keyarray.size()-1 ) {
			framesize = m_keyarray[m_curframe+1].koffset - m_keyarray[m_curframe].koffset ;
		}
		else {
			framesize = m_filesize - m_keyarray[m_curframe].koffset ;
		}
		if( framesize<0 ) framesize = 0 ;
		if( fi == NULL ) {
			return framesize ;
		}
		fi->frametype = FRAMETYPE_KEYVIDEO ;
		fi->frametime = m_filetime ;
		time_dvrtime_addms(&(fi->frametime), m_keyarray[m_curframe].ktime );
		if( fi->framebuf == NULL || fi->framesize<=0 ) {
			fi->framesize = framesize ;
		}
		else {
			seek( m_keyarray[m_curframe].koffset );
			framesize = read( fi->framebuf, fi->framesize );
			m_curframe++ ;
		}
	}
	return framesize ;
}

// return 1: seek success, 0: out of range
int dvrfile::seek( dvrtime * seekto )
{
    int seekms ; 		// seek in milliseconds frome time of file
    if( !isopen() ) {
        return 0;
    }
    if( *seekto < m_filetime ) {
        m_curframe = 0;
        return 0;
    }

    seekms=time_dvrtime_diffms( seekto, &m_filetime );
	for( m_curframe=0; m_curframe<m_keyarray.size(); m_curframe++ ) {
		if( seekms <= m_keyarray[m_curframe].ktime ) {
			return 1;
		}
	}
	
    return 0;
}

int dvrfile::readkey( array <struct dvr_key_t> &keyarray )
{
	FILE * keyfile = NULL;
	
	string keyfilename(m_filename) ;
	char * pk = (char *)keyfilename ;
	int l=strlen(pk);
	if( l>24 && strcmp( pk+l-4, ".266")==0 ) {
		strcpy( pk+l-4,".k");
		keyfile=fopen( pk, "r" );
	}

	keyarray.clean();
	
	if( keyfile ) {
		struct dvr_key_t key;
		char lbuf[128] ;
		while( fgets( lbuf, 128, keyfile) != NULL ) {
			if( sscanf( lbuf, "%d,%d", &(key.ktime), &(key.koffset))==2){
				keyarray.add(key);
			}
		}
		fclose(keyfile);
	}
	
	return keyarray.size();
}

// update key index file
void dvrfile::writekey( array <struct dvr_key_t> &keyarray )
{
	// renew key file
	string keyfilename(m_filename) ;
	char * pk = (char *)keyfilename ;
	int l=strlen(pk);
	if( l>24 && strcmp( pk+l-4, ".266")==0 ) {
		strcpy( pk+l-4,".k");
		FILE * keyfile=fopen( pk, "w" );
		if( keyfile ) {
			for( int i=0; i<keyarray.size(); i++ ) {
				fprintf(keyfile, "%d,%d\n", 
					keyarray[i].ktime,
					keyarray[i].koffset );
			}
			fclose( keyfile );
		}
	}
}

// repair a .264 file, return 1 for success, 0 for failed
int dvrfile::repair()
{
    string nf ;
    char * newfilename ;
    int filelen ;
    int filesize ;
    int i, k ;
    
    if( m_keyarray.size()<3 ) {
		return 0;
	}

	for( k=2; k<m_keyarray.size(); k++ ) {
		if(m_keyarray[k].koffset >= m_filesize ) {
			break;
		}
	}

	k-- ;
	filesize = m_keyarray[k].koffset ;
	filelen = m_keyarray[k].ktime / 1000 ;

	// renew key file
	m_keyarray.setsize(k);
	writekey( m_keyarray );
	
	// truncate file
	close();
	::truncate( (char *)m_filename, filesize );
	
	// nename to new file length
    newfilename = nf.setbufsize(512) ;
	strcpy( newfilename, (char *)m_filename );
	char * base = strrchr( newfilename, '/' );
	int lock = (strstr( base+21, "_L_" )!=NULL);
	sprintf(base + 21,
			"%d_%c_%s.266", 
			filelen, 
			lock?'L':'N', 
			g_hostname);
			
    dvrfile::rename( (char *)m_filename, newfilename );	
    return 1 ;
}

int dvrfile::repairpartiallock()
{
    dvrfile lockfile ;
    struct dvrtime breaktime ;
    int breakindex ;
    char lockfilename[300] ;
    char lockfilename1[300] ;
    char * lockfilenamebase ;
    int i;
    int locklength, filelen ;
    
    filelen = f264length( (char *)m_filename );
    locklength = f264locklength( (char *)m_filename );
    
    if( locklength >= filelen || locklength<2 ) {        // not a partial locked file
        return 0 ;
    }
    
    if( m_keyarray.size() < 3 ) {            // no key index ?
        return 0 ;
    }
    
    for( breakindex=m_keyarray.size()-2; breakindex>=0; breakindex-- ) {
        if( m_keyarray[breakindex].ktime < (filelen-locklength)*1000 ) {
            breakindex++;	  
            break;
        }
    }
   // dvr_log("inside repair lock");
    breaktime = m_filetime ;
    time_dvrtime_addms( &breaktime, m_keyarray[breakindex].ktime );
    
    strcpy( lockfilename, m_filename.getstring() );
    lockfilenamebase = basefilename( lockfilename );
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_0_L_%s.266", 
                breaktime.year,
                breaktime.month,
                breaktime.day,
                breaktime.hour,
                breaktime.minute,
                breaktime.second,
                g_hostname);
    lockfile.open(lockfilename, "wb");
    if( !lockfile.isopen() ) {
        return 0 ;
    }
   // dvr_log("file:%s is opened",lockfilename);
    char lockfilehead[40] ;
    
    readheader( (void *)lockfilehead, 40 );
    lockfile.writeheader( lockfilehead, 40 );
    lockfile.m_fileencrypt=0 ;
    
    for( i=breakindex ; i<m_keyarray.size(); i++ ) {
        // if rec_busy, delay for 0.1 s
	/*
        for( busywait=0; busywait<10; busywait++) {
            if( rec_busy==0 ) break ;
            usleep(1000);
        }*/
        
        unsigned char * framebuf ;
        int framesize ;
        int framepos ;
        struct dvrtime frametime ;
        
        framepos = m_keyarray[i].koffset ;
        if( i<m_keyarray.size()-1 ) {
            framesize = m_keyarray[i+1].koffset - m_keyarray[i].koffset ;
        }
        else {
            framesize = m_filesize - m_keyarray[i].koffset ;
        }
        if(framesize<0)
	  break;
	if(framesize>10*1024*1024)
	  break;
      //  dvr_log("i=%d keysize:%d framesize=%d",i,m_keyarray.size(),framesize);
      //dvr_log("%d:%d",m_keyarray[i+1].koffset,m_keyarray[i].koffset);
        frametime = m_filetime ;
        time_dvrtime_addms( &frametime, m_keyarray[i].ktime );
        
        framebuf = (unsigned char *) malloc ( framesize ) ;
        seek( framepos );
        read( framebuf, framesize );
        lockfile.writeframe( framebuf, framesize, FRAMETYPE_KEYVIDEO, &frametime );
        free( framebuf );
        
    }
    
    lockfile.close();
 //   dvr_log("lock file is closed");
    // rename new locked file
    strcpy( lockfilename1, lockfilename );
    lockfilenamebase = basefilename( lockfilename1 );
    
    locklength = m_keyarray[breakindex].ktime / 1000 ;
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_%d_L_%s.266", 
            breaktime.year,
            breaktime.month,
            breaktime.day,
            breaktime.hour,
            breaktime.minute,
            breaktime.second,
            filelen-locklength,
            g_hostname);
    
    dvrfile::rename( lockfilename, lockfilename1 );
    
    // close original file
    seek( m_keyarray[breakindex].koffset ) ;

    m_keyarray.setsize(breakindex);
    m_openmode = 1 ;                    // set open mode to write, so close() would save key file
    close();
    
    dvr_log( "Event locked file breakdown success. (%s)", lockfilename1 );

    // rename old locked filename
    strcpy( lockfilename1, m_filename.getstring() );
    lockfilenamebase = basefilename( lockfilename1 );
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_%d_N_%s.266", 
            m_filetime.year,
            m_filetime.month,
            m_filetime.day,
            m_filetime.hour,
            m_filetime.minute,
            m_filetime.second,
            locklength,
            g_hostname);
   
    usleep(1000);	    
    dvrfile::rename( m_filename.getstring(), lockfilename1 );
    disk_rescan();
    return 1 ;
}

char * dvrfile::makedvrfilename( struct dvrtime * filetime, int channel, int lock, int filelen )
{
	int l ;
	string filename ;

	// do not create file before 2019 ( firmware build year )
	if( filetime->year < build_year )
		return NULL;
	
	// retrieve base dir depends on file type ?
	char * basedisk = disk_getbasedisk( lock ) ;
	if( basedisk == NULL ) {
		// disk not ready, do not create file
		return NULL;
	}
	
	char * p = filename.setbufsize(512);
	
	// make base rec dir
	l = sprintf(p, "%s/_%s_", basedisk, g_hostname );
	mkdir(p, 0777);
	
	// make date directory
	l += sprintf( p+l, "/%04d%02d%02d", 
			filetime->year,
			filetime->month,
			filetime->day );
	mkdir(p, 0777);
	
	// make channel directory
	l += sprintf( p+l, "/CH%02d", channel);
	mkdir(p, 0777);
	
	// make new file name
	l += sprintf( p+l,
			"/CH%02d_%04d%02d%02d%02d%02d%02d_%d_%c_%s.266", 
			channel,
			filetime->year,
			filetime->month,
			filetime->day,
			filetime->hour,
			filetime->minute,
			filetime->second,
			filelen, 
			lock?'L':'N', 
			g_hostname);
			
	char * dvrfilename = new char [l+8] ;
	strcpy( dvrfilename, p );
			
	return dvrfilename ;
}

int dvrfile::breakLfile( char * filename, struct dvrtime * locktime )
{
	dvrfile nfile ;
	dvrfile lfile ;
	char * base ;
	struct dvrtime filetime ;
	struct dvrtime fileendtime ;
	int filelen , locklen, filechannel ;
	int breaktime ;
	int breakindex ;
	char * lockname ;
	int i;
	
	char * lockdisk = disk_getbasedisk(1);
	if( lockdisk == NULL ) 
		return 0;
	
	base = basefilename( filename );
	if( base == NULL || strstr( base, "_N_") == NULL ) {
		return 0 ;		// not a valid N file
	}
	
	f264time( base, &filetime );
	filelen = f264length( base );
	filechannel = f264channel( base ) ;
	fileendtime = filetime ;
	
	time_dvrtime_add( &fileendtime, filelen-10 );
	if( *locktime > fileendtime ) {
		return 0 ;		// locktime out of this file
	}
       
    breaktime = time_dvrtime_diff( locktime, &filetime ) ;
    if( breaktime<=0 ) {								// to lock whole file
		i = strlen(lockdisk);
		if( strncmp( lockdisk, filename, i )==0 ) {		// same disk, rename it
			lockname = makedvrfilename( &filetime, filechannel, 1, filelen )  ;
			if( lockname != NULL ) {
				dvrfile::rename( filename, lockname );
				delete lockname ;
			}
			return 1 ;
		}
	}

	nfile.open( filename , "r+" );
	if( !nfile.isopen() ) {
		dvr_log( "Can't open file for repair. (%s)", filename );
		return 0 ;		// can open
	}

    if( nfile.m_keyarray.size()<3 ) {            // small file? skip it
        return 0 ;
    }
       
    breaktime*=1000 ;							// make it milliseconds
    breakindex = -1 ;
	for( i=0;i<nfile.m_keyarray.size()-3;i++) {
		if( nfile.m_keyarray[i].ktime >= breaktime ) {
			breakindex = i ;
			break;
		}
	}
	if( breakindex < 0 ) return 0 ;
	
	unsigned char * framebuf ;
	struct dvrtime frametime ;
	
	// new lock file len
	locklen = filelen - nfile.m_keyarray[breakindex].ktime / 1000 ;
	if( locklen<5 ) return 0 ;
	
	// new filelen
	filelen -= locklen ;
		
	frametime = filetime ;
	time_dvrtime_addms( &frametime, nfile.m_keyarray[breakindex].ktime ) ;
	
	lockname = makedvrfilename( &frametime, filechannel, 1, locklen ) ;
	if( lockname == NULL ) 
		return 0 ;

	lfile.open( lockname, "w" );
	delete lockname ;
	
	if( !lfile.isopen() ) {
		dvr_log( "Can't open file for repair. (%s)", (char *)(*lockname) );
		return 0 ;		// can open
	}	
	
	// allow write raw data to lfile
    lfile.m_fileencrypt=0 ;
    
	// duplicate file header
	framebuf = new unsigned char [40] ;
	nfile.seek(0);
	nfile.read( framebuf, 40 );
	lfile.seek(0);
	lfile.write( framebuf, 40 );
    delete framebuf ;

    for( i=breakindex ; i<nfile.m_keyarray.size(); i++ ) {
        int framesize ;
		int framepos ;
		
        framepos = nfile.m_keyarray[i].koffset ;
        if( i<nfile.m_keyarray.size()-1 ) {
            framesize = nfile.m_keyarray[i+1].koffset - framepos ;
        }
        else {
			// lastframe
            framesize = nfile.m_filesize - framepos;
        }
                
        if(framesize<=0)
			break;
			
		// wait a second if busy
		for( int busy=0; busy<5; busy++ ) {
			if( file_busy > 1 || g_cpu_usage > 90 || g_memdirty > 1000 ) {
				usleep(100000);
			}
			else {
				break;
			}
		}
			
		framebuf = new unsigned char [framesize] ;
		
		nfile.seek(framepos) ;
		nfile.read(framebuf, framesize);
		
		frametime = filetime ;
		time_dvrtime_addms( &frametime, nfile.m_keyarray[i].ktime ) ;
		
		lfile.writeframe( framebuf, framesize, FRAMETYPE_KEYVIDEO, &frametime );
		
		delete framebuf ;
		
	}
	lfile.close();

	// now truncate oldfile
	nfile.truncate( nfile.m_keyarray[breakindex].koffset );
	nfile.m_keyarray.setsize(breakindex);
	nfile.writekey( nfile.m_keyarray );
    nfile.close();
    
    // rename broken file
    string newfilename ;
    strcpy( newfilename.setbufsize(512), filename );
    base = basefilename( newfilename );
    sprintf(base+20, "%d_N_%s.266", 
            filelen,
            g_hostname);
    dvrfile::rename( filename, (char *)newfilename );
    disk_rescan();

    dvr_log( "Locked file breakdown. (%s)", (char *)newfilename );
 
    return NULL ;
}    
   
// rename .264 file as well .idx file
int dvrfile::rename(const char * oldfilename, const char * newfilename)
{
    int res ;
    char oldkfile[512], newkfile[512] ;
    int  lo, ln ;
    res = ::rename( oldfilename, newfilename );
    strcpy( oldkfile, oldfilename );
    strcpy( newkfile, newfilename );
    lo = strlen( oldkfile );
    ln = strlen( newkfile );
    if( strcmp( &oldkfile[lo-4], ".266" )== 0 &&
       strcmp( &newkfile[ln-4], ".266" )== 0 ) {
           strcpy( &oldkfile[lo-4], ".k" );
           strcpy( &newkfile[ln-4], ".k" );
           ::rename( oldkfile, newkfile );
    }
    return res;
}

// remove .264 file as well .idx file
int dvrfile::remove(const char * filename)
{
    int res ;
    char kfile[256] ;
    int l ;
    res = ::remove( filename );
    strcpy( kfile, filename );
    l = strlen( kfile );
    if( strcmp( &kfile[l-4], ".266" ) == 0 ) {
        strcpy( &kfile[l-4], ".k" ) ;
        ::remove( kfile );
    }
    return res ;
}

DWORD dvrfile::getfileheader()
{
  	return m_fileheader.flag ;
}

void file_init()
{
    int iv ;
    string v;
    config dvrconfig(dvrconfigfile);
    unsigned char file_filekey[260] ;
    
    file_encrypt=dvrconfig.getvalueint("system", "fileencrypt");
    v = dvrconfig.getvalue("system", "filepassword");
    if( v.length()>=342 ) {
       // c642bin(v.getstring(), file_filekey, 256);
       // RC4_crypt_table( file_encrypt_RC4_table, 1024, file_filekey);
        c642bin(v.getstring(), g_filekey, 256);
        RC4_crypt_table( file_encrypt_RC4_table, 1024, g_filekey);		
    }
    else {
        file_encrypt=0;
    }
    
    v=dvrconfig.getvalue("system", "filebuffersize");
    char unit='b' ;
    file_bufsize=(512*1024) ;
    int n=sscanf(v.getstring(), "%d%c", &file_bufsize, &unit);
    if( n==2 && (unit=='k' || unit=='K') ) {
        file_bufsize*=1024 ;
    }
    else if( n==2 && (unit=='M' || unit=='m' ) ) {
        file_bufsize*=1024*1024 ;
    }
    if( file_bufsize<128*1024)file_bufsize=(128*1024) ;
    if( file_bufsize>(4*1024*1024) ) file_bufsize=(4*1024*1024) ;
    
    //file_nodecrypt=dvrconfig.getvalueint("system", "file_nodecrypt");
}

void file_uninit()
{
	// wait for file write completed
	for( int busy=0; busy<10000; busy++) {
		if( file_busy>0 ) {
			usleep(10000);
		}
		else {
			break;
		}
	}
    sync();
}
