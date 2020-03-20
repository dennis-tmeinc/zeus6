// string and string list
// string functions

#ifndef __LIST_H__
#define __LIST_H__


// duel link list
template <class T>
class list {
    struct item {
        item * prev ;
        item * next ;
        T data ;
    } ;
    item * m_head ;
    item * m_tail ;
    item * m_iter ;			// iterator
    T * getdata() {
        if( m_iter ) {
            return &(m_iter->data) ;
        }
        else {
            return NULL;
        }
    }
    public:
        list() {
            m_head=NULL;
            m_tail=NULL;
            m_iter=NULL;	
        }
        ~list() {
            while(m_head) {
                m_iter=m_head;
                m_head=m_head->next ;
                delete m_iter ;
            }
        }
        T * head() {
            m_iter=m_head ;
            return getdata();
        }
        T * tail() {
            m_iter=m_tail ;
            return getdata();
        }
        T * next() {
            if( m_iter ) {
                m_iter=m_iter->next ;
            }
            return getdata();
        }
        T * prev() {
            if( m_iter ) {
                m_iter=m_iter->prev ;
            }
            return getdata();
        }
        int size() {
            int count=0;
            item * pit = m_head ;
            while( pit ) {
                count++;
                pit=pit->next ;
            }
            return count ;
        }
        T * find(T & t) {				// find a item equal to t, return NULL failed
            T * f ;
            f=head();
            while( f != NULL ) {
                if( t == *f ) {
                    return f;
                }
                f=next();
            }
            return NULL;
        }
        T * inserthead(T & t){
            item * nitem = new item ;
            nitem->data = t ;
            nitem->next = m_head ;
            nitem->prev = NULL ;
            if( m_head ) {
                m_head->prev = nitem ;
                m_head = nitem ;
            }
            else {
                m_head=m_tail=nitem;
                
            }
            return head();
        }
        T * inserttail(T & t){
            item * nitem = new item ;
            nitem->data = t;
            nitem->next = NULL;
            nitem->prev = m_tail ;
            if( m_tail ) {
                m_tail->next = nitem;
                m_tail=nitem ;
            }
            else {
                m_head=m_tail=nitem;
            }
            return tail();	
        }
        int removehead() {
            item * t ;
            if( m_head ) {
                t=m_head->next ;
                delete m_head ;
                m_head=t ;
                if( m_head ) {
                    m_head->prev=NULL ;
                }
                else {
                    m_tail=NULL ;
                }
                return 1;
            }
            return 0;
        }
        int removetail() {
            item * t ;
            if( m_tail ) {
                t=m_tail->prev ;
                delete m_tail;
                m_tail=t ;
                if( m_tail ) {
                    m_tail->next=NULL ;
                }
                else {
                    m_head=NULL;
                }
                return 1;
            }
            return 0;
        }
        int removecurrent() {		// remove current item pointer by m_iter
            if( m_iter==NULL ) {
                return 0 ;
            }
            else if( m_iter == m_head ) {
                m_iter=NULL ;
                return removehead();
            }
            else if( m_iter==m_tail ) {
                m_iter=NULL ;
                return removetail();
            }
            else {
                m_iter->prev->next = m_iter->next ;
                m_iter->next->prev = m_iter->prev ;
                delete m_iter ;
                m_iter=NULL;
                return 1;
            }
        }
        void clean() {          // clean all items
            while(m_head) {
                m_iter=m_head;
                m_head=m_head->next ;
                delete m_iter ;
            }
            m_tail=NULL;
            m_iter=NULL;
        }
};

#endif  // __LIST_H__
