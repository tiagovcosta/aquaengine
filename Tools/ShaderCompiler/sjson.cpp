#include "sjson.h"

#include <iostream>
#include <sstream>

struct File
{
	const char* str;
	uint32_t    current_line_num;
};

Value::Value(int line_num)
	: _data(nullptr), _type(ValueType::NULL_VALUE),
	_line_num(line_num)
{}

Value::Value(Object obj, int line_num) 
	: _data(new Object(std::move(obj))), _type(ValueType::OBJECT), _line_num(line_num)
{}

Value::Value(Array arr, int line_num)
	: _data(new Array(std::move(arr))), _type(ValueType::ARRAY), _line_num(line_num)
{}

Value::Value(std::string str, int line_num)
	: _data(new std::string(std::move(str))), _type(ValueType::STRING), _line_num(line_num)
{}

Value::Value(double number, int line_num) 
	: _data(new double(number)), _type(ValueType::NUMBER),
	_line_num(line_num)
{}

Value::Value(bool boolean, int line_num) 
	: _data(new bool(boolean)), _type(ValueType::BOOL),
	_line_num(line_num)
{}

Value::Value(const Value& other) 
	: _data(nullptr), _type(other._type), _line_num(other._line_num)
{
	switch(_type)
	{
	case ValueType::OBJECT:
		_data = new Object(*(Object*)other._data);
		break;
	case ValueType::ARRAY:
		_data = new Array(*(Array*)other._data);
		break;
	case ValueType::STRING:
		_data = new std::string(*(std::string*)other._data);
		break;
	case ValueType::NUMBER:
		_data = new double(*(double*)other._data);
		break;
	case ValueType::BOOL:
		_data = new bool(*(bool*)other._data);
		break;
	}
}

Value::Value(Value&& other) : Value(0)
{
	swap(*this, other);
}

Value& Value::operator=(Value other)
{
	swap(*this, other);

	return *this;
}

void swap(Value& first, Value& second) // nothrow
{
	std::swap(first._data, second._data);
	std::swap(first._type, second._type);
	std::swap(first._line_num, second._line_num);
}

Value::~Value()
{
	switch(_type)
	{
		case ValueType::OBJECT:
			delete ((Object*)_data);
			break;
		case ValueType::ARRAY:
			delete ((Array*)_data);
			break;
		case ValueType::STRING:
			delete ((std::string*)_data);
			break;
		case ValueType::NUMBER:
			delete ((double*)_data);
			break;
		case ValueType::BOOL:
			delete ((bool*)_data);
			break;
	}
}

ValueType Value::getType() const
{
	return _type;
}

uint32_t  Value::getLineNum() const
{
	return _line_num;
}

const Object& Value::asObject() const
{
	return *(const Object*)_data;
}

const Array& Value::asArray() const
{
	return *(const Array*)_data;
}

const std::string& Value::asString() const
{
	return *(const std::string*)_data;
}

double Value::asNumber() const
{
	return *(double*)_data;
}

bool Value::asBool() const
{
	return *(bool*)_data;
}

void skipWhitespaces(File& file)
{
	while(*file.str != '\0')
	{
		if(*file.str == '/')
		{
			if(*(file.str + 1) == '/')
			{
				//skip line comment
				file.str += 2;

				while(*file.str != '\n' && *file.str != '\0')
					file.str++;

			}
			else if(*(file.str + 1) == '*')
			{
				file.str += 2;

				//skip block comment
				while(*file.str != '\0')
				{
					if(*file.str == '*' && *(file.str + 1) == '/')
					{
						file.str += 2;
						break;
					}

					if(*file.str == '\n')
						file.current_line_num++;

					file.str++;
				}
			}
			else
				break;
		}
		else if(*file.str == ' ' || *file.str == '\t' || *file.str == '\n' || *file.str == ',')
		{
			if(*file.str == '\n')
				file.current_line_num++;

			file.str++;
		}
		else
			break;
	}
}

inline bool isNum(char c)
{
	return c >= '0' && c <= '9';
}

inline bool isAlpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline bool isAlphaNum(char c)
{
	return isAlpha(c) || isNum(c);
}

//Tries to consume <value> from file
//If fails, restores file to original state
bool consume(File& file, const char* value)
{
	File temp = file;

	while(*file.str == *value && *value != '\0')
	{
		if(*file.str == '\n')
			file.current_line_num++;

		file.str++;
		value++;
	}

	if(*value == '\0')
		return true;

	//Didnt match so restore state
	file = temp;

	return false;
}

bool parseCode(File& file, std::string& out_code)
{
	const char* start = file.str;

	while(!consume(file, "\"\"\""))
	{
		if(*file.str == '\n')
			file.current_line_num++;

		if(*file.str == '\0')
		{
			std::cout << "Line " << file.current_line_num << ": Unexpected end-of-file!\n";
			return false;
		}

		file.str++;
	}

	out_code = std::string(start, file.str - start - 3);

	return true;
}

bool parseString(File& file, std::string& out_string)
{
	//consume "
	file.str++;

	std::stringstream ss;

	while(*file.str != '"')
	{
		if(*file.str == '\n')
			file.current_line_num++;

		if(*file.str == '\0')
		{
			std::cout << "Line " << file.current_line_num << ": Unexpected end-of-file!\n";
			return false;
		}
		else if(*file.str != '\\')
		{
			ss << *file.str;
		}
		else
		{
			file.str++;

			if(*file.str == '"' || *file.str == '\\' || *file.str == '/')
				ss << *file.str;
			else if(*file.str == 'b')
				ss << '\b';
			else if(*file.str == 'f')
				ss << '\f';
			else if(*file.str == 'n')
				ss << '\n';
			else if(*file.str == 'r')
				ss << '\r';
			else if(*file.str == 't')
				ss << '\t';
			else
			{
				std::cout << "Line " << file.current_line_num << ": Unexpected escaped character!\n";
				return false;
			}
		}

		file.str++;
	}

	out_string = ss.str();

	//consume '"'
	file.str++;

	return true;
}

bool parseIdentifier(File& file, std::string& out_string)
{
	if(*file.str == '"')
		return parseString(file, out_string);

	if(!isAlpha(*file.str))
	{
		std::cout << "Line " << file.current_line_num << ": Unexpected char '" << *file.str << "'. Identifier must start with an alpha char!\n";
		std::cout << (int)*file.str << "\n";

		return false;
	}

	const char* start = file.str;
	file.str++;

	while(isAlphaNum(*file.str) || *file.str == '_')
		file.str++;

	out_string = std::string(start, file.str - start);

	return true;
}

bool parseValue(File& file, Value& out_value);

bool parseObject(File& file, Object& out_obj)
{
	file.str++; // consume '{'

	skipWhitespaces(file);

	while(*file.str != '}')
	{
		std::string key;

		uint32_t key_line_num = file.current_line_num;

		if(!parseIdentifier(file, key))
		{
			std::cout << "Line " << key_line_num << ": Error parsing identifier!\n";
			return false;
		}
		else if(key.empty())
		{
			std::cout << "Line " << key_line_num << ": Identifier can't be an empty string!\n";
			return false;
		}

		if(out_obj[key] != nullptr)
		{
			std::cout << "Line " << key_line_num << ": Key '" << key << "' already exists!\n";
			return false;
		}

		skipWhitespaces(file);

		if(*file.str != '=')
		{
			std::cout << "Line " << file.current_line_num << ": Expected '=' not found!\n";
			return false;
		}

		file.str++; //consume '='

		skipWhitespaces(file);

		Value value;

		if(!parseValue(file, value))
			return false;

		out_obj.insert(std::move(key), std::move(value), key_line_num);

		skipWhitespaces(file);
	}

	//consume '}'
	file.str++;

	return true;
}

bool parseArray(File& file, Array& out_array)
{
	file.str++; // consume '['

	skipWhitespaces(file);

	while(*file.str != ']')
	{
		Value value;

		if(!parseValue(file, value))
			return false;

		out_array.push_back(value);

		skipWhitespaces(file);
	}

	//consume ']'
	file.str++;

	return true;
}

bool parseNumber(File& file, double& out_double)
{
	char* end;

	char* saved_locale = setlocale(LC_NUMERIC, "C");

	out_double = strtod(file.str, &end);

	setlocale(LC_NUMERIC, saved_locale);

	file.str = end;

	return true;
}

bool parseValue(File& file, Value& out_value)
{
	skipWhitespaces(file);

	char c = *file.str;

	uint32_t line_num = file.current_line_num;

	if(c == '{')
	{
		Object obj;

		if(!parseObject(file, obj))
			return false;

		out_value = Value(std::move(obj), line_num);
	}
	else if(c == '[')
	{
		Array arr;

		if(!parseArray(file, arr))
			return false;

		out_value = Value(std::move(arr), line_num);
	}
	else if(c == '"')
	{
		std::string str;

		if(consume(file, "\"\"\""))
		{
			if(!parseCode(file, str))
				return false;

		}
		else if(!parseString(file, str))
			return false;

		out_value = Value(std::move(str), line_num);
	}
	else if(c == '-' || (c >= '0' && c <= '9'))
	{
		double num;

		if(!parseNumber(file, num))
			return false;

		out_value = Value(num, line_num);
	}
	else if(isAlpha(*file.str))
	{
		if(*file.str == 't' && consume(file, "true"))
		{
			out_value = Value(true, line_num);
			return true;
		}
		else if(*file.str == 'f' && consume(file, "false"))
		{
			out_value = Value(false, line_num);
			return true;
		}
		else if(*file.str == 'n' && consume(file, "null"))
		{
			out_value = Value(line_num);
			return true;
		}

		std::string str;

		if(!parseIdentifier(file, str))
			return false;

		out_value = Value(std::move(str), line_num);
	}
	else if(c == '\0')
	{
		std::cout << "Line " << file.current_line_num << ": Unexpected end-of-file!\n";
		return false;
	}
	else
	{
		std::cout << "Line " << file.current_line_num << ": Unexpected char '" << c << "'!\n";
		return false;
	}

	return true;
}

bool parseSJSON(const std::string& filename, const char* data, Object& out_obj)
{
	File file;
	file.str = data;
	file.current_line_num = 1;

	skipWhitespaces(file);

	while(*file.str != '\0')
	{
		std::string key;

		uint32_t key_line_num = file.current_line_num;

		if(!parseIdentifier(file, key))
		{
			std::cout << "Line " << key_line_num << ": Error parsing identifier!\n";
			return false;
		}

		if(out_obj[key] != nullptr)
		{
			std::cout << "Line " << key_line_num << ": Key '" << key << "' already exists!\n";
			return false;
		}

		skipWhitespaces(file);

		if(*file.str != '=')
		{
			std::cout << "Line " << file.current_line_num << ": Expected '=' not found!\n";
			return false;
		}

		file.str++; //consume '='

		skipWhitespaces(file);

		Value value;

		if(!parseValue(file, value))
			return false;

		out_obj.insert(std::move(key), std::move(value), key_line_num);

		skipWhitespaces(file);
	}

	return true;
}