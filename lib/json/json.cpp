/*
	A very simple json parser and encoder
	by Dennis Chen@tme
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

json::~json()
{
	cleanup();
	if (name != NULL) {
		delete[] name;
	}
}

// cleanup child objects and valueString (keep object name) and set type to null
void json::cleanup()
{
	while (child != NULL) {
		json* n = child->next;
		delete child;
		child = n;
	}
	if (valueString != NULL) {
		delete[] valueString;
		valueString = NULL;
	}
	type = JSON_Null;
}

json& json::setName(const char* newname)
{
	if (name != NULL) {
		delete[] name;
		name = NULL;
	}
	if (newname != NULL) {
		name = new char[strlen(newname) + 1];
		strcpy(name, newname);
	}
	return *this;
}

json& json::setNumber(double dvalue)
{
	valueNumber = dvalue;
	return setType(JSON_Number);
}

double json::getNumber()
{
	if (type == JSON_Number) {
		return valueNumber;
	}
	else if (type == JSON_String) {
		if (sscanf(getString(), "%lg", &valueNumber) > 0) {
			return valueNumber;
		}
		else {
			return 0;
		}
	}
	else {
		return (double)(int)(type == JSON_True);
	}
}

json& json::setString(const char* value)
{
	type = JSON_String;
	if (valueString != NULL) {
		delete[] valueString;
		valueString = NULL;
	}
	if (value != NULL) {
		valueString = new char[strlen(value) + 1];
		strcpy(valueString, value);
	}
	else {
		valueString = new char[1];
		*valueString = 0;
	}
	return *this;
}

const char* json::getString()
{
	if (type == JSON_String && valueString != NULL) {
		return valueString;
	}
	else if (type == JSON_Number) {
		if (valueString)
			delete[] valueString;
		valueString = new char[30];
		// precision 13 should be enough for all my applications
		sprintf(valueString, "%.13lg", valueNumber);
		return valueString;
	}
	else if (type == JSON_Object) {
		return "(object)";
	}
	else if (type == JSON_Array) {
		return "(array)";
	}
	else if (type == JSON_True) {
		return "(true)";
	}
	else if (type == JSON_False) {
		return "(false)";
	}
	else {		// include JSON_Null
		return "(null)";
	}
}

// add item to the end of child list
json& json::addItem(json* item)
{
	if (child != NULL) {
		json* lastchild = child;
		while (lastchild->next)
			lastchild = lastchild->next;
		lastchild->next = item;
	}
	else {
		if( isNull() ) {
			// set to object or array for new created json, in case I used default constructor
			if( item->getName() == NULL ) {
				type = JSON_Array;
			}
			else {
				type = JSON_Object;
			}
		}
		child = item;
	}
	item->next = NULL;
	return *this;
}

int json::itemSize()
{
	int siz = 0;
	json* n = child;
	while (n) {
		siz++;
		n = n->next;
	}
	return siz;
}

// get child item by index
json* json::getItem(int index)
{
	if ( type != JSON_Array && index<0 ) {
		return NULL;
	}
	json* n = child;
	while ( index-- > 0 && n != NULL) {
		n = n->next;
	}
	return n;
}

// get child item by name
json* json::getItem(const char* iname)
{
	if (iname == NULL)
		return NULL;
	json* n = child;
	while (n != NULL) {
		if (n->name != NULL && strcmp(n->name, iname) == 0) {
			break;
		}
		n = n->next;
	}
	return n;
}

// get a leaf; ex: "resourceSets/0/resources/0/address/formattedAddress"
json* json::getLeaf(const char* leaf, char seperator)
{
	int l;
	char itemName[64];

	itemName[63] = 0;
	// get the first level child name
	for (l = 0; l < 63; l++) {
		if (leaf[l] == seperator) { // field name seperated by '/' (seperator)
			itemName[l] = 0;
			break;
		}
		else if (leaf[l] == 0) {
			itemName[l] = 0;
			l = 0;
			break;
		}
		itemName[l] = leaf[l];
	}

	json* child = NULL;
	if( isObject() ) {
		child = getItem(itemName);
	}
	else if ( isArray() ){
		child = getItem(atoi(itemName));
	}
	if (child && l>0 ) {	 // go deeper
		return child->getLeaf(leaf + l + 1);
	}
	return child;
}

const char * json::getLeafString(const char* leaf, char seperator)
{
	json * jleaf = getLeaf(leaf, seperator);
	if (jleaf) {
		return jleaf->getString();
	}
	else {
		return "";
	}
}

double json::getLeafNumber(const char* leaf, char seperator)
{
	json * jleaf = getLeaf(leaf, seperator);
	if (jleaf) {
		return jleaf->getNumber();
	}
	else {
		return 0.0;
	}
}

// skip space and comments ( with my own json extension, support sh and c comments)
static const char* skip_space(const char* v)
{
	while (*v) {
		if (((unsigned char)*v) <= ' ') { // space
			v++;
		}
		else if (*v == '#') //  # sign single line comment (my extensions)
		{
			v++;
			while (*v && *v != '\n')
				v++; // skip until new line
		}
		else if (*v == '/') //  check for c comments
		{
			if (*(v + 1) == '/') //  single line comment
			{
				v += 2;
				while (*v && *v != '\n')
					v++;				// skip until new line
			}
			else if (*(v + 1) == '*') // multi-line comment
			{
				v += 2;
				while (*v) {
					if (*v == '*' && *(v + 1) == '/') // end of multiline comment "*/"
					{
						v += 2;
						break;
					}
					v++;
				}
			}
			else
				break;
		}
		else {
			break;
		}
	}
	return v;
}

// parse string from text
void json::parse_string(const char* text, const char** parse_end)
{
	if (*text != '\"') {
		// Error, not a string
		if (parse_end != NULL) {
			*parse_end = text;
		}
		type = JSON_Null;
		return;
	}

	// estimate string size
	const char* t = text + 1;
	while (*t && *t != '\"') {
		if (*t++ == '\\')
			// Skip escaped quotes.
			if( *t ) t++; 
	}

	if (*t != '\"') {
		// Error, string not completed
		if (parse_end != NULL) {
			*parse_end = text;
		}
		type = JSON_Null;
		return;
	}

	type = JSON_String;
	int len = (t - text) + 2;
	if (valueString != NULL)
		delete valueString;
	valueString = new char[len + 2];
	char* v = valueString;
	unsigned int uc;
	int n;

	text++;
	while (*text && (v - valueString) < len) {
		if (*text == '\"') // end of string
		{
			text++;
			break;
		}
		else if (*text == '\\') // escape
		{
			if (*++text == 0)
				break;

			switch (*text) {
			case 'n':
				*v++ = '\n';
				text++;
				break;

			case 'r':
				*v++ = '\r';
				text++;
				break;

			case 't':
				*v++ = '\t';
				text++;
				break;

			case 'b':
				*v++ = '\b';
				text++;
				break;

			case 'f':
				*v++ = '\f';
				text++;
				break;

			case 'v':
				*v++ = '\v';
				text++;
				break;

			case 'u':
				text++;
				if (sscanf(text, "%4x%n", &uc, &n) > 0) {
					text += n;
					// convert to utf8
					if (uc <= 0x7f) {
						*v++ = (char)uc;
					}
					else if (uc <= 0x7ff) {
						*v++ = (char)(0xc0 | (uc >> 6));
						*v++ = (char)(0x80 | (uc & 0x3f));
					}
					else if (uc >= 0xD800 && uc <= 0xDBFF && *text == '\\' && *(text + 1) == 'u') {
						// UTF-16 surrogate pair (U+010000 to U+10FFFF)
						unsigned int lowsur; // low surrogates
						text += 2;
						if (sscanf(text, "%4x%n", &lowsur, &n) > 0) {
							text += n;
							uc = ((uc - 0xD800) << 10) + (lowsur - 0xDC00) + 0x10000;
							*v++ = (char)(0xf0 | ((uc >> 18) & 0x07));
							*v++ = (char)(0x80 | ((uc >> 12) & 0x3f));
							*v++ = (char)(0x80 | ((uc >> 6) & 0x3f));
							*v++ = (char)(0x80 | (uc & 0x3f));
						}
					}
					else {
						*v++ = (char)(0xe0 | (uc >> 12));
						*v++ = (char)(0x80 | ((uc >> 6) & 0x3f));
						*v++ = (char)(0x80 | (uc & 0x3f));
					}
				}
				break;

			default:
				// this includes \, /, "
				*v++ = *text++;
				break;
			}
		}
		else {
			*v++ = *text++;
		}
	}

	*v = 0;
	if (parse_end != NULL) {
		*parse_end = text;
	}
}

// parse object or array
void json::parse_object(const char* text, const char** parse_end)
{
	cleanup();
	if (*text == '{' || *text == '[') {
		type = *text == '{' ? JSON_Object : JSON_Array;
		text++;
		while (*text) {
			json* n = new json();
			n->parse(text, &text);
			text = skip_space(text);
			if (*text == ',') {
				text++;
				addItem(n);
				continue;
			}
			else if (*text == '}' || *text == ']') // object completed
			{
				if (n->isNull()) {
					delete n;
				}
				else {
					addItem(n);
				}
				text++;
				break;
			}
			else {
				delete n;
				// error!
				type = JSON_Null;
				break;
			}
		}
	}
	if (parse_end != NULL) {
		*parse_end = text;
	}
}

// parse json value only
void json::parse_value(const char* text, const char** parse_end)
{
	text = skip_space(text);
	if (strncmp(text, "null", 4) == 0) {
		text += 4;
		type = JSON_Null;
	}
	else if (strncmp(text, "false", 5) == 0) {
		text += 5;
		type = JSON_False;
	}
	else if (strncmp(text, "true", 4) == 0) {
		text += 4;
		type = JSON_True;
	}
	else if (*text == '{' || *text == '[') {
		parse_object(text, &text);
	}
	else if (*text == '\"') {
		// parse string
		parse_string(text, &text);
	}
	else {
		// try parse as an number
		int n;
		if (sscanf(text, "%lg%n", &valueNumber, &n) > 0) {
			type = JSON_Number;
			text += n;
		}
		else {
			// failed all json types
			type = JSON_Null;
		}
	}
	if (parse_end != NULL) {
		*parse_end = text;
	}
}

// parse json object (name:value pair)
void json::parse(const char* text, const char** parse_end)
{
	parse_value(text, &text);
	if (isString()) {
		text = skip_space(text);
		if (*text == ':') { // name:value pair?
			if (name != NULL)
				delete[] name;
			// use string value as name
			name = valueString;
			valueString = NULL;

			// parse the real value
			parse_value(text + 1, &text);
		}
	}
	if (parse_end != NULL) {
		*parse_end = text;
	}
}

// encode string,
// return number of chars used, return 0 if failed.
int json::encode_string(char* text, int si)
{
	char* t = text;
	*t++ = '\"'; // quote
	if (valueString != NULL) {
		char* v = valueString;
		while (*v) {
			if (si - (t - text) < 8) // no enough space, failed
				return 0;
			switch (*v) {
			case '\n':
				*t++ = '\\';
				*t++ = 'n';
				break;
			case '\r':
				*t++ = '\\';
				*t++ = 'r';
				break;
			case '\t':
				*t++ = '\\';
				*t++ = 't';
				break;
			case '\b':
				*t++ = '\\';
				*t++ = 'b';
				break;
			case '\f':
				*t++ = '\\';
				*t++ = 'f';
				break;
			case '\"':
				*t++ = '\\';
				*t++ = '\"';
				break;
			case '\\':
				*t++ = '\\';
				*t++ = '\\';
				break;
			case '/':
				*t++ = '\\';
				*t++ = '/';
				break;
			default:
				if (((unsigned char)*v) < 0x20) {
					t += sprintf(t, "\\u%04X", (int)*v);
				}
				else {
					*t++ = *v;
				}
				break;
			}
			v++;
		}
	}
	*t++ = '\"'; // end of string
	*t = 0;
	return t - text;
}

// encode object/array
// return number of chars used, return 0 if failed.
int json::encode_object(char* text, int len, int indent)
{
	int n;
	int idx = 0;
	char* t = text;

	// begin of object/array
	*t++ = type == JSON_Object ? '{' : '[';

	json* item = child;
	while (item != NULL) {
		if ((len - (t - text) - 5) < 0) // no enough space, failed
			return 0;

		// indent
		if( indent>0 ) {
			*t++ = '\n';
			for (n = 0; n < indent; n++) {
				*t++ = ' ';*t++ = ' ';
			}
		}

		if (type == JSON_Object) {
			if (item->name != NULL) {
				char* saveValue = item->valueString;				// save valueString
				item->valueString = item->name;						// replace value with name for encoder
				n = item->encode_string(t, (len - (t - text) - 5)); // encode name
				item->valueString = saveValue;						// restore value
			}
			else {
				n = sprintf(t, "\"%d\"", idx); // no name field, use idx number
			}
			if (n <= 0) // failed
				return 0;
			t += n;
			*t++ = ':';
		}

		n = item->encode(t, (len - (t - text) - 5), indent==0?0:(indent+1));
		if (n <= 0) // failed
			return 0;
		t += n;
		item = item->next;
		if (item) {
			*t++ = ',';
			idx++;
		}
		else {
			break;
		}
	}
	// end of object/array
	// closing indent
	if( indent>0 ) {
		*t++ = '\n';
		for (n = 1; n < indent; n++) {
			*t++ = ' ';*t++ = ' ';
		}
	}		
	*t++ = type == JSON_Object ? '}' : ']';
	*t = 0;
	return t - text;
}

// encode this json object
//    return total bytes encoded into text
int json::encode(char* text, int len, int indent)
{
	if (len < 20) {
		// at lease 20 bytes
		return 0;
	}
	switch (type) {
	case JSON_False:
		strcpy(text, "false");
		return 5;
	case JSON_True:
		strcpy(text, "true");
		return 4;
	case JSON_Number:
		// precision 13 should be enough for all my applications
		return sprintf(text, "%.13lg", valueNumber);
	case JSON_String:
		return encode_string(text, len);
	case JSON_Array:
	case JSON_Object:
		return encode_object(text, len, indent);
	default:
		// include null
		strcpy(text, "null");
		return 4;
	}
}

// clone from source json
json& json::clone(const json& src)
{
	json* n;
	cleanup();
	setName(src.name);
	type = src.type;
	switch (type) {
	case JSON_Number:
		setNumber(src.valueNumber);
		break;
	case JSON_String:
		setString(src.valueString);
		break;
	case JSON_Array:
	case JSON_Object:
		n = src.child;
		while (n != NULL) {
			json* i = new json();
			i->clone(*n);
			addItem(i);
			n = n->next;
		}
		break;
	default:
		break;
	}
	return *this;
}

json& json::loadFile(const char* filename)
{
	FILE* f = fopen(filename, "r");
	if (f) {
		fseek(f, 0, SEEK_END);
		int l = ftell(f);
		if (l > 0) {
			char* buf = new char[l + 1];
			fseek(f, 0, SEEK_SET);
			l = fread(buf, 1, l, f);
			if (l > 0) {
				buf[l] = 0;
				parse(buf);
			}
			delete[] buf;
		}
		fclose(f);
	}
	return *this;
}

#define MAX_JSON_FILE_SIZE (200000)

void json::saveFile(const char* filename)
{
	char* buf = new char[MAX_JSON_FILE_SIZE];
	int l = encode(buf, MAX_JSON_FILE_SIZE);
	if (l > 0) {
		FILE* f = fopen(filename, "w");
		if (f) {
			fwrite(buf, 1, l, f);
			fclose(f);
		}
	}
	delete[] buf;
}
