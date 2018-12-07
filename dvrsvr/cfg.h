
#ifndef __cfg_h__
#define __cfg_h__

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>

// struct used to enumerate config file
struct config_enum {
    int line ;                                          // initialize to 0 before call enumkey/enumsection
    string key ;
} ;

class config {
  protected:
    array <string> m_strlist ;
    string  m_configfile ;
    string  m_tempstr ;
    int     m_dirty ;
    char    m_delimiter ;

    char * getsection(int line);
    char * getkey(int line);
    char * getvalue(int line);

  public:
    config(const char *configfilename);

    int findsection(const char *section);
    int findkey(int index, const char *key );

    char * enumsection(int &index);
    char * enumkey( int &index );

    // return total number of sections found
    int     listsection( array <string> &sections );
    // return total number of keys found
    int     listkey( const char * section, array <string> &keys );

    string & getvalue(const char *section, const char *key);
    int     getvalueint(const char *section, const char *key);
    void    setvalue(const char *section, const char *key, const char *value);
    void    setvalueint(const char *section, const char *key, int value);
    void    removekey(const char *section, const char *key);
    void    setdelimiter( char delimiter ) {
        m_delimiter = delimiter ;
    }
    void    save();
    void    load();
};

#endif                                                  // __cfg_h__
