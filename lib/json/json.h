/*
	A very simple json parser and encoder
	by Dennis Chen@tme
*/

#ifndef __JSON__H__
#define __JSON__H__

#include <stddef.h>

/* JSON Types: */
#define JSON_Null (0)
#define JSON_True (1)
#define JSON_False (2)
#define JSON_Number (3)
#define JSON_String (4)
#define JSON_Array (5)
#define JSON_Object (6)

class json {
protected:
	int type; // json value type
	double valueNumber;
	char* valueString;
	char* name;

	json* next;  // next item in the same object or array
	json* child; // first child items of this object or array

private:
	void parse_string(const char* text, const char** parse_end);
	void parse_object(const char* text, const char** parse_end);
	void parse_value(const char* text, const char** parse_end);

	int encode_string(char* text, int len);
	int encode_object(char* text, int len, int indent);

	void cleanup(); // cleanup value and set type to null

public:
	// construct a typed json object
	json(int typ = JSON_Null)
		: type(typ)
		, valueNumber(0)
		, valueString(NULL)
		, name(NULL)
		, next(NULL)
		, child(NULL)
	{
	}

	~json();

	json& clone(const json& src); // clone from source json object
	json& operator=(const json& src) { return clone(src); }
	json& setType(int typ)
	{
		type = typ;
		return *this;
	}
	int getType() { return type; };
	json& setName(const char* newname);
	const char* getName() { return name; }
	json& setNull() { return setType(JSON_Null); }
	json& setTrue() { return setType(JSON_True); }
	json& setFalse() { return setType(JSON_False); }
	json& setNumber(double dvalue);
	json& setNumber(int ivalue) { return setNumber((double)ivalue); }
	double getNumber();
	int getInt() { return (int)getNumber(); }
	json& setString(const char* value);
	const char* getString();

	json& addItem(json* item);							// add a child item
	json& addNumberItem(const char* nam, double dvalue) // add a number item to object
	{
		return addItem(&(new json())->setNumber(dvalue).setName(nam));
	}
	json& addNumberItem(double dvalue) // add a number item to array
	{
		return addItem(&(new json())->setNumber(dvalue));
	}
	json& addStringItem(const char* nam,
		const char* string) // add a string item to object
	{
		return addItem(&(new json())->setString(string).setName(nam));
	}
	json& addStringItem(const char* string) // add a string item to array
	{
		return addItem(&(new json())->setString(string));
	}
	int itemSize();					 // get number of children items
	json* getItem(int index);		 // get item with index (for array)
	json* getItem(const char* name); // get item with name (for objects)

	int isNull() { return type == JSON_Null; }
	int isTrue() { return type == JSON_True; }
	int isFalse() { return !isTrue(); }
	int isBool() { return type == JSON_False || type == JSON_True; }
	int isNumber() { return type == JSON_Number; }
	int isInt()
	{
		return type == JSON_Number && valueNumber == (double)(int)valueNumber;
	}
	int isString() { return type == JSON_String; }
	int isArray() { return type == JSON_Array; }
	int isObject() { return type == JSON_Object; }

	void parse(const char* text,
		const char** parse_end = NULL);				 // parse a json value in text
	int encode(char* text, int len, int indent = 0); // encode this json object

	json& loadFile(const char* filename);
	void saveFile(const char* filename);
};

#endif // __JSON__H__
