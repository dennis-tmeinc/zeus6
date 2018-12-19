/*
	A very simple json parser and encoder
*/

#include <stdio.h>
#include <string.h>

#include "json.h"

// construct a typed json object
json::json(int typ)
{
	next = NULL;
	child = NULL;
	type = typ;
	valueString = NULL;
	name = NULL;
}

json::~json()
{
	cleanup();
	if (name != NULL)
	{
		delete name;
	}
}

// cleanup child objects and valueString (keep object name) and set type to null
void json::cleanup()
{
	while (child != NULL)
	{
		json *n = child->next;
		delete child;
		child = n;
	}
	if (valueString != NULL)
	{
		delete valueString;
		valueString = NULL;
	}
	type = JSON_Null;
}

json &json::setName(const char *newname)
{
	if (name != NULL)
	{
		delete name;
		name = NULL;
	}
	if (newname != NULL)
	{
		name = new char[strlen(newname) + 1];
		strcpy(name, newname);
	}
	return *this;
}

json &json::setNull()
{
	type = JSON_Null;
	return *this;
}

json &json::setBool(int value)
{
	type = (value) ? JSON_True : JSON_False;
	return *this;
}

json &json::setNumber(double dvalue)
{
	type = JSON_Number;
	valueNumber = dvalue;
	return *this;
}

json &json::setString(const char *value)
{
	type = JSON_String;
	if (valueString != NULL)
		delete valueString;
	if (value != NULL)
	{
		valueString = new char[strlen(value) + 1];
		strcpy(valueString, value);
	}
	else
	{
		valueString = new char[1];
		*valueString = 0;
	}
	return *this;
}

json &json::setArray()
{
	cleanup();
	type = JSON_Array;
	return *this;
}

json &json::setObject()
{
	cleanup();
	type = JSON_Object;
	return *this;
}

// add item to the end of child list
json &json::addItem(json *item)
{
	if (child)
	{
		json *lastchild = child;
		while (lastchild->next)
			lastchild = lastchild->next;
		lastchild->next = item;
	}
	else
	{
		child = item;
	}
	item->next = NULL;
	return *this;
}

// add an number item to object
json &json::addNumberItem(const char *nam, double dvalue)
{
	json *j = new json();
	j->setNumber(dvalue);
	j->setName(nam);
	return addItem(j);
}

// add a string item to object
json &json::addStringItem(const char *nam, const char *string)
{
	json *j = new json();
	j->setString(string);
	j->setName(nam);
	return addItem(j);
}

int json::itemSize()
{
	int siz = 0;
	json *n = child;
	while (n)
	{
		siz++;
		n = n->next;
	}
	return siz;
}

// get child item by index
json *json::getItem(int index)
{
	json *n = child;
	while (index-- > 0 && n != NULL)
	{
		n = n->next;
	}
	return n;
}

// get child item by name
json *json::getItem(const char *iname)
{
	if (iname == NULL)
		return NULL;
	json *n = child;
	while (n != NULL)
	{
		if (n->name != NULL && strcmp(n->name, iname) == 0)
		{
			return n;
		}
		n = n->next;
	}
	return NULL;
}

// skip space and comments
static const char *skip_space(const char *v)
{
	while (*v)
	{
		while (*v > 0 && *v <= ' ')
			v++;
		if (*v == '/') //  check for comments
		{
			if (*(v + 1) == '/') //  single line comment
			{
				v += 2;
				while (*v && *v != '\n')
					v++; // skip until new line
			}
			else if (*(v + 1) == '*') // multi-line comment
			{
				v += 2;
				while (*v)
				{
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
		else
		{
			break;
		}
	}
	return v;
}

// parse string from text
void json::parse_string(const char *text, const char **parse_end)
{
	cleanup();
	text = skip_space(text);

	if (*text != '\"')
	{
		// Error, not a string
		if (parse_end != NULL)
		{
			*parse_end = text;
		}
		return;
	}
	text++;

	// estimate string size
	const char *t = text;
	while (*t && *t != '\"')
	{
		if (*t++ == '\\')
			t++; /* Skip escaped quotes. */
	}
	int len = (t - text) + 1;
	type = JSON_String;
	valueString = new char[len + 1];
	char *v = valueString;
	while (*text && (v - valueString) < len)
	{
		if (*text == '\"') // end of string
		{
			text++;
			break;
		}
		else if (*text == '\\') // escape
		{
			if (*++text == 0)
				break;

			switch (*text)
			{
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

			case 'u':
				text++;
				{
					unsigned int uc = 0;
					int n = 0;
					if (sscanf(text, "%4x%n", &uc, &n) > 0)
					{
						text += n;
						// convert to utf8
						if (uc < 0x80)
						{
							*v++ = (char)uc;
						}
						else if (uc < 0x800)
						{
							*v++ = (char)(0xc0 | (uc >> 6));
							*v++ = (char)(0x80 | (uc & 0x3f));
						}
						else
						{
							*v++ = (char)(0xe0 | (uc >> 12));
							*v++ = (char)(0x80 | ((uc >> 6) & 0x3f));
							*v++ = (char)(0x80 | (uc & 0x3f));
						}
					}
				}
				break;

			default:
				*v++ = *text++;
				break;
			}
		}
		else
		{
			*v++ = *text++;
		}
	}

	*v = 0;
	if (parse_end != NULL)
	{
		*parse_end = text;
	}
}

void json::parse_array(const char *text, const char **parse_end)
{
	cleanup();

	text = skip_space(text);
	if (*text == '[')
	{
		type = JSON_Array;
		text = skip_space(text + 1);
		if (*text == ']') // empty array
		{
			if (parse_end != NULL)
			{
				*parse_end = text + 1;
			}
			return;
		}
		while (*text)
		{
			json *js = new json();
			js->parse(text, &text);
			addItem(js);

			text = skip_space(text);
			if (*text == ']')// array completed
			{ 
				text++;
				break;
			}
			else if (*text == ',')
			{
				text++;
				continue;
			}
			else
			{
				cleanup();
				break;
			}
		}
	}

	if (parse_end != NULL)
	{
		*parse_end = text;
	}
}

void json::parse_object(const char *text, const char **parse_end)
{
	cleanup();

	text = skip_space(text);
	if (*text == '{')
	{
		type = JSON_Object;
		text = skip_space(text + 1);
		if (*text == '}') // empty object
		{ 
			if (parse_end != NULL)
			{
				*parse_end = text + 1;
			}
			return;
		}
		while (1)
		{
			// get items name
			json *js = new json();
			js->parse(text, &text);
			text = skip_space(text);
			if (js->isString() && *text == ':')
			{
				// move value to name
				js->name = js->valueString;
				js->valueString = NULL;

				js->parse(text + 1, &text);
				text = skip_space(text);
			}
			addItem(js);
			if (*text == '}') // object completed
			{ 
				text++;
				break;
			}
			else if (*text == ',')
			{
				text++;
				continue;
			}
			else
			{
				// error
				cleanup();
				break;
			}
		}
	}
	if (parse_end != NULL)
	{
		*parse_end = text;
	}
}

json &json::parse(const char *text, const char **parse_end)
{
	text = skip_space(text);
	if (strncmp(text, "null", 4) == 0)
	{
		text += 4;
		setNull();
	}
	else if (strncmp(text, "false", 5) == 0)
	{
		text += 5;
		setFalse();
	}
	else if (strncmp(text, "true", 4) == 0)
	{
		text += 4;
		setTrue();
	}
	else if (*text == '{')
	{
		parse_object(text, &text);
	}
	else if (*text == '[')
	{
		parse_array(text, &text);
	}
	else if (*text == '\"')
	{
		// parse string
		parse_string(text, &text);
	}
	else
	{
		// try parse as an number
		double dv;
		int n = 0;
		if (sscanf(text, "%lg%n", &dv, &n) > 0)
		{
			setNumber(dv);
			text += n;
		}
		else
		{
			// failed all json types
			cleanup();
		}
	}

	if (parse_end != NULL)
	{
		*parse_end = text;
	}
	return *this;
}

// encode string,
// return number of chars used, return 0 if failed.
int json::encode_string(char *text, int len)
{
	char *t = text;
	*t++ = '\"'; // quote
	char *v = valueString;
	if (v != NULL)
	{
		while (*v)
		{
			if ((len - (t - text) - 5) < 0) // no enough space, failed
				return 0;
			switch (*v)
			{
			case '\n':
				*t++ = '\\';
				*t++ = 'n';
				v++;
				break;
			case '\r':
				*t++ = '\\';
				*t++ = 'r';
				v++;
				break;
			case '\t':
				*t++ = '\\';
				*t++ = 't';
				v++;
				break;
			case '\b':
				*t++ = '\\';
				*t++ = 'b';
				v++;
				break;
			case '\f':
				*t++ = '\\';
				*t++ = 'f';
				v++;
				break;
			case '\"':
				*t++ = '\\';
				*t++ = '\"';
				v++;
				break;
			case '\\':
				*t++ = '\\';
				*t++ = '\\';
				v++;
				break;
			default:
				*t++ = *v++;
				break;
			}
		}
	}
	*t++ = '\"'; // end of string
	*t = 0;
	return t - text;
}

// encode array
// return number of chars used, return 0 if failed.
int json::encode_array(char *text, int len)
{
	int n = 0;
	char *t = text;
	*t++ = '['; // start of array
	json *item = child;
	while (item != NULL)
	{
		if ((len - (t - text) - 5) < 0) // no enough space, failed
			return 0;

		n = item->encode(t, (len - (t - text) - 5));
		if (n <= 0) // failed
			return 0;
		t += n;
		item = item->next;
		if (item)
		{
			*t++ = ',';
			*t++ = ' '; // a space
		}
	}
	*t++ = ']'; // end of array
	*t = 0;
	return t - text;
}

// encode object
// return number of chars used, return 0 if failed.
int json::encode_object(char *text, int len)
{
	int n = 0;
	char *t = text;
	int idx = 0;

	*t++ = '{'; // begin of object
	json *item = child;
	while (item != NULL)
	{
		if ((len - (t - text) - 5) < 0) // no enough space, failed
			return 0;

		if (item->name != NULL)
		{
			char *saveValue = item->valueString; // save valueString
			item->valueString = item->name;		 // replace value with name for encoder
			n = item->encode_string(t, (len - (t - text) - 5));
			item->valueString = saveValue; // restore value
		}
		else
		{
			n = sprintf(t, "\"%d\"", idx); // no name field, use idx number
		}
		if (n <= 0) // failed
			return 0;
		t += n;
		*t++ = ':';

		n = item->encode(t, (len - (t - text) - 5));
		if (n <= 0) // failed
			return 0;
		t += n;
		item = item->next;
		if (item)
		{
			*t++ = ',';
			*t++ = ' '; // a space
			idx++;
		}
	}
	*t++ = '}'; // end of object
	*t = 0;
	return t - text;
}

// encode this json object
//    return
int json::encode(char *text, int len)
{
	if (len < 20)
	{ // at lease 20 bytes
		return 0;
	}
	switch (type)
	{
	case JSON_Null:
		strcpy(text, "null");
		return 4;
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
		return encode_array(text, len);
	case JSON_Object:
		return encode_object(text, len);
	}
	return 0;
}

// clone from source json
json &json::clone(const json &src)
{
	json *n;
	cleanup();
	setName(src.name);
	type = src.type;
	switch (type)
	{
	case JSON_Number:
		setNumber(src.valueNumber);
		break;
	case JSON_String:
		setString(src.valueString);
		break;
	case JSON_Array:
	case JSON_Object:
		n = src.child;
		while (n != NULL)
		{
			json *i = new json();
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

json &json::loadFile(const char *filename)
{
	FILE *f = fopen(filename, "r");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		int l = ftell(f);
		if (l > 0)
		{
			char *buf = new char[l + 2];
			fseek(f, 0, SEEK_SET);
			l = fread(buf, 1, l, f);
			if (l > 0)
			{
				buf[l] = 0;
				parse(buf);
			}
			delete[] buf;
		}
		fclose(f);
	}
	return *this;
}

void json::saveFile(const char *filename)
{
	char *buf = new char[60000];
	int l = encode(buf, 60000);
	if (l > 0)
	{
		FILE *f = fopen(filename, "w");
		if (f)
		{
			fwrite(buf, 1, l, f);
			fclose(f);
		}
	}
	delete[] buf;
}
