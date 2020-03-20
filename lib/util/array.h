// string and string list
// string functions

#ifndef __ARRAY_H__
#define __ARRAY_H__

#define ARRAYSTEPSIZE (50)

// dynamic growable array
template <class T>
class array {
protected:
	T* m_array;
	int m_size;		 // valid number of item
	int m_arraysize; // availabel items space

	// swap items of index a and b
	void swap(int a, int b)
	{
		T s;
		s = m_array[a];
		m_array[a] = m_array[b];
		m_array[b] = s;
	}

	void quicksort(int lo, int hi)
	{
		int i = lo, j = hi - 1;
		while (i <= j) {
			while (i <= j && m_array[i] < m_array[hi]) {
				i++;
			}
			while (j > i && (!(m_array[j] < m_array[hi]))) {
				j--;
			}
			if (i < j) {
				swap(i, j);
				i++;
			}
			j--;
		}
		if (i < hi) {
			swap(i, hi);
		}
		if (lo < i - 1) {
			quicksort(lo, i - 1);
		}
		if (hi > i + 1) {
			quicksort(i + 1, hi);
		}
	}

public:
	array(int initialsize = ARRAYSTEPSIZE)
	{
		m_size = 0;
		if (initialsize > 0) {
			m_arraysize = initialsize;
			m_array = new T[m_arraysize];
		} else {
			m_arraysize = 0;
			m_array = NULL;
		}
	}
	~array()
	{
		if (m_arraysize > 0) {
			delete[] m_array;
		}
	}
	int size()
	{
		return m_size;
	}
	void expand(int newsize)
	{
		if (newsize > m_arraysize) {
			if (newsize < ARRAYSTEPSIZE) {
				newsize = ARRAYSTEPSIZE;
			} else if (newsize < m_arraysize + ARRAYSTEPSIZE) {
				newsize = m_arraysize + ARRAYSTEPSIZE;
			} else {
				newsize += ARRAYSTEPSIZE;
			}
			T* newarray = new T[newsize];
			if (m_size > 0 && m_array != NULL) {
				for (int i = 0; i < m_size; i++) {
					newarray[i] = m_array[i];
				}
			}
			if (m_array != NULL)
				delete[] m_array;
			m_arraysize = newsize;
			m_array = newarray;
		}
	}
	void setsize(int newsize)
	{
		expand(newsize);
		m_size = newsize;
	}
	T* at(int index)
	{
		if (index >= m_size) {
			expand(index + 1);
			m_size = index + 1;
		}
		return &m_array[index];
	}
	T& operator[](int index)
	{
		if (index >= m_size) {
			expand(index + 1);
			m_size = index + 1;
		}
		return m_array[index];
	}
	void add(T& t)
	{
		expand(m_size + 1);
		m_array[m_size++] = t;
	}
	void insert(T& t, int pos)
	{
		if (pos >= m_size) {
			expand(pos + 1);
			m_size = pos + 1;
			m_array[pos] = t;
		} else {
			expand(m_size + 1);
			for (int i = m_size; i > pos; i--) {
				m_array[i] = m_array[i - 1];
			}
			m_array[pos] = t;
			m_size++;
		}
	}
	void remove(int pos)
	{
		if (pos >= 0 && m_size > 0 && pos < m_size) {
			m_size--;
			for (int i = pos; i < m_size; i++) {
				m_array[i] = m_array[i + 1];
			}
		}
	}
	void remove(T* pos)
	{
		int i;
		for (i = 0; i < m_size; i++) {
			if ((&m_array[i]) == pos) {
				remove(i);
				break;
			}
		}
	}
	void clean()
	{
		if (m_arraysize > 0) {
			delete[] m_array;
			m_array = NULL;
		}
		m_arraysize = 0;
		m_size = 0;
	}
	void compact()
	{
		if (m_size < m_arraysize) {
			if (m_size == 0) {
				delete[] m_array;
				m_array = NULL;
				m_arraysize = 0;
			} else {
				T* newarray = new T[m_size];
				for (int i = 0; i < m_size; i++) {
					newarray[i] = m_array[i];
				}
				delete[] m_array;
				m_array = newarray;
				m_arraysize = m_size;
			}
		}
	}
	void sort()
	{
		expand(m_size + 1);
		if (m_size > 1) {
			quicksort(0, m_size - 1);
		}
	}
	array<T>& operator=(array<T>& a)
	{
		int i;
		setsize(a.size());
		for (i = 0; i < m_size; i++) {
			m_array[i] = a[i];
		}
		return *this;
	}
};

#endif // __ARRAY_H__
