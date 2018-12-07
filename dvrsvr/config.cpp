
#include "dvr.h"

static char *skipspace(char *line)
{
    while ( *line > 0 && *line <= ' ' )
        line++;
    return line;
}

config::config(const char *configfilename)
{
    m_delimiter = '=' ;         // default delimiter
    m_configfile=configfilename;
    load();
}

char * config::getsection(int line)
{
    if( line>=0 && line<m_strlist.size() ) {
        char *s = skipspace(m_strlist[line]);
        if(*s == '[') {
            m_tempstr = skipspace(s + 1) ;
            s = strchr( (char *)m_tempstr, ']' );
            if (s) {
                *s = '\0';
            }
            m_tempstr.trimtail();
            return (char *)m_tempstr;
        }
    }
    return NULL ;
}

char * config::getkey(int line)
{
    if( line>=0 && line<m_strlist.size() ) {
        m_tempstr = "" ;
        char *s = skipspace(m_strlist[line]);
        if(*s == '[') {
            // a section line
            return NULL ;
        }
        else if (*s == '#' || *s == ';' || *s == '\0' || *s == m_delimiter ) {       //  comments or nul line
            return (char *)m_tempstr ;
        }
        else {
            m_tempstr = s ;
            s = strchr( (char *)m_tempstr, m_delimiter);
            if(s!=NULL){
                *s = '\0';
            }
            m_tempstr.trimtail();
        }
        return (char *)m_tempstr ;
    }
    return NULL ;
}

char * config::getvalue(int line)
{
    m_tempstr="";
    if( line>=0 && line<m_strlist.size() ) {
        char *s = skipspace(m_strlist[line]);
        if( *s != '['  &&  *s != '#' && *s != ';' && *s != '\0' && *s != m_delimiter ) {    // a valid key = value line
            s = strchr( s, m_delimiter);
            if(s!=NULL){
                m_tempstr = skipspace(s+1) ;
                // remove comments
                s = strchr( (char *)m_tempstr, '#' );
                if( s != NULL )
                    *s=0;
                s = strchr( (char *)m_tempstr, ';' );
                if( s != NULL )
                    *s=0;
                m_tempstr.trimtail();
            }
        }
    }
    return (char *)m_tempstr ;
}

// search for section
char * config::enumsection(int &index)
{
    if( index<0 ) {
        return NULL;
    }
    for ( int i=index ; i < m_strlist.size(); i++) {
        char * sect = getsection(i);
        if( sect != NULL ) {
            index = i+1 ;
            return sect ;
        }
    }
    index = -1;
    return NULL;                    // not found ;
}

// return key string
char * config::enumkey( int &index )
{
    if( index<0 ) {
        return NULL;
    }
    for(int i=index; i < m_strlist.size(); i++) {
        char * key = getkey(i);
        if( key==NULL ){
            break;
        }
        else if( *key != '\0' ) {
            index = i+1 ;
            return key ;
        }
    }
    index = -1;
    return NULL;
}

// search for section
int config::findsection(const char *section)
{
    if (section == NULL || *section == 0 ) {
        // from first line
        return 0;
    }
    char * key ;
    int index = 0 ;
    while( (key = enumsection( index ))!= NULL ) {
        if( strcmp( key, section )==0 ) {
            return index ;
        }
    }
    return -1 ;
}

// return line index of found key
int config::findkey(int index, const char *key )
{
    char * k ;
    if( index<0 || key==NULL ) {
        return -1 ;
    }
    string tkey(key);
    while( (k=enumkey(index))!=NULL ) {
        if( strcmp( k, (char *)tkey )==0 ) {                // found key
            return index-1 ;
        }
    }
    return -1 ;
}

// return total sections found, (exclude blank section)
int config::listsection( array <string> &sections )
{
    sections.setsize(0);
    char * se ;
    int i=0;
    while( (se=enumsection(i))!=NULL ) {
        string s(se) ;
        sections.add( s );
    }
    return sections.size();
}

int config::listkey( const char * section, array <string> &keys )
{
    keys.setsize(0);
    char * k ;
    int i = findsection(section);
    while( (k=enumkey(i)) != NULL ) {
        string s(k) ;
        keys.add(s);
    }
    return keys.size();
}

string & config::getvalue(const char *section, const char *key)
{
    getvalue(findkey(findsection(section), key ));
    return m_tempstr;
}

int config::getvalueint(const char *section, const char *key)
{
    int v = 0 ;
    getvalue(section, key );
    if( sscanf((char *)m_tempstr, "%d", &v)) {
        return v;
    }
    else {
        if( strcasecmp((char *)m_tempstr, "yes")==0 ||
            strcasecmp((char *)m_tempstr, "on")==0 )
        {
            return 1 ;
        }
    }
    return 0 ;
}

void config::setvalue(const char *section, const char *key, const char *value)
{
    int sectionindex;
    int keyindex;
    string kv ;     // key value string
    string tsection(section) ;
    string tkey(key) ;
    string tvalue(value) ;
    string newline ;
    sprintf( newline.setbufsize(strlen(tkey) + strlen(tvalue) + 8), "%s%c%s", (char *)tkey, m_delimiter, (char *)tvalue);

    sectionindex = findsection(tsection);
    if (sectionindex >= 0) {        // section found
        keyindex = findkey(sectionindex, tkey);
        if( keyindex >= 0 ) {       // found key
            char * ov = (char *)getvalue(keyindex);
            if( strcmp( ov, tvalue )!=0 ) {
                m_strlist[keyindex] = newline ;     // replace with new value
                m_dirty = 1 ;
            }
        }
        else {                      // new key
            m_strlist.insert( newline, sectionindex );
            m_dirty = 1;
        }
    }
    else {          // new section
        sprintf( kv.setbufsize(strlen(section) + 4), "[%s]", section);
        m_strlist.add(kv);                          // add new section
        m_strlist.add(newline);
        m_dirty = 1;
    }
}

void config::setvalueint(const char *section, const char *key, int value)
{
    char cvalue[40] ;
    sprintf(cvalue, "%d", value);
    setvalue(section, key, cvalue);
}

void config::removekey(const char *section, const char *key)
{
    int index = findkey(findsection(section), key);
    if( index>=0 ) {
        m_strlist.remove(index);
        m_dirty = 1 ;
    }
}

void config::save()
{
    int i;
    FILE *sfile ;
    if( m_dirty ) {
        sfile = fopen((char *)m_configfile, "w");
        if (sfile) {
            for (i = 0; i < m_strlist.size(); i++) {
                m_tempstr = m_strlist[i] ;
                m_tempstr.trimtail();
                fputs((char *)m_tempstr, sfile);
                fputs("\n", sfile);
            }
            fclose(sfile);
        }
        m_dirty = 0;
    }
}

void config::load()
{
    FILE *rfile;
    rfile = fopen(m_configfile, "r");
    if( rfile ) {
        string line ;
        while (fgets(line.setbufsize(8192), 8192, rfile)) {
            m_strlist.add(line);
        }
        fclose( rfile );
    }
    m_dirty = 0 ;
}
