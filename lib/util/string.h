// string and string list
// string functions

#ifndef __STRING_H__
#define __STRING_H__

#include <string.h>
#include <stdarg.h>

class string {
    protected:
        char *m_str ;
        int   m_s ;
        void set(const char *str){
			if( str==m_str ) return ;
			char * t_str = m_str ;
            if( str && *str != 0 ) {
                m_s = strlen(str)+1 ;
                m_str=new char [m_s];
                strcpy(m_str, str);
            }
            else {
				m_str = NULL ;
				m_s = 0 ;
			}
            if( t_str!=NULL ) {
                delete t_str ;
            }
        }
	
    public:
        string() {
			m_s = 0 ;
            m_str=NULL;
        } 

        string( const char * str ) {
			m_s = 0 ;
            m_str=NULL;
			set( str );
        }
        
        string(string & str) {
			m_s = 0 ;
            m_str=NULL;
			set( str.m_str );
        }

        ~string() {
            if( m_str ) {
                delete m_str ;
            }
        }
        
		char *get(){
			if (m_str == NULL) {
				m_s = 2;
				m_str=new char [m_s] ;
				m_str[0]='\0' ;
			}
			return m_str;
		}

		char *getstring(){
			return get();
		}
		
		operator char * (){
			return get() ;
		}

		string& operator=(const char* str)
		{
			set(str);
			return *this;
		}

		string& operator=(string& str)
		{
			set(str.m_str);
			return *this;
		}
        
        string & operator += ( const char * s2 ) {
            if( s2 != NULL )
			    strcat( expand( length()+strlen(s2)+1 ), s2 );
			return *this ;
        }

        int compare( const char * s ) {
            return strcmp(getstring(), s) ;
        }
                
        int operator > ( const char * s ) {
            return compare(s)>0 ;
        }

        int operator < ( const char * s ) {
            return compare(s)<0 ;
        }
        
        int operator == ( const char * s ) {
            return compare(s)==0 ;
        }

        int operator != ( const char * s ) {
            return compare(s)!=0 ;
        }
        
        int length(){
            if( m_str ) {
                return strlen(m_str);
            }
            else {
                return 0;
            }
        }
        
        int size() {
			return m_s ;
		}

        char * expand(int nsize){
            if( nsize<0 ) nsize=0 ;
            nsize+=4 ;
			if( nsize>m_s ) {
				char * nstr = new char [nsize] ;
				if( m_str ) {
					memcpy( nstr, m_str, m_s );
					delete m_str ;
					nstr[m_s]=0;
				}
				else {
					nstr[0] = 0 ;
				}
				m_str = nstr ;
				m_s = nsize ;
			}
			return get();
		}
		
		char * setbufsize( int nsize ) {
            if( nsize<0 ) nsize=0 ;
            nsize+=4 ;
			if( nsize>m_s ) {		// to resize buffer
				if( m_str ) {
					delete m_str ;
				}
				m_s = nsize ;
				m_str = new char [m_s] ;
				m_str[0]=0;
			}
			return get();
		}
		
		string & printf(const char * format, ...) {
			va_list va;
			va_start(va, format);
			vsnprintf(setbufsize(2048), 2048, format, va);
			va_end(va);
			return *this;
		}
		
		string & trimtail()
        {
			if( m_str ) {
				int l = strlen( m_str );
				if( l>m_s ) l=m_s ;
				while( l>0 && m_str[l-1]>0 && m_str[l-1]<=' ' ) {
					l-- ;
				}
				if( l<m_s )  {
					m_str[l] = 0 ;
				}
			}
			return *this ;
		}

		string & trimhead()
        {
			if( m_str ) {
				int l = 0 ;
				while( l<m_s && m_str[l]>0 && m_str[l]<=' ' ) {
					l++;
				}
				if( l>0 ) {
					set( &m_str[l] );
				}
			}
			return *this ;
		}
		
		string & trim()
        {
			trimtail();
			trimhead();
			return *this ;
		}
		
        int isempty() {
			return m_str==NULL || *m_str==0 ;
        }
        char & operator[] (int idx) {
            return m_str[idx] ;
        }
};

#endif  // __STRING_H__
