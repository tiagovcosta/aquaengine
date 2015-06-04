#pragma once

#include <cstdint>
#include <map>
#include <vector>

// DONE: CHECK LOCALE ON parseNumber(), ',' vs '.'

// TODO: (NOT SURE YET) Make sure all values inside array are the same type

enum class ValueType : uint8_t
{
	OBJECT,
	ARRAY,
	STRING,
	NUMBER,
	BOOL,
	NULL_VALUE
};

class Value;

typedef std::vector<Value> Array;

class Object;

class Value
{
	//DONE (RValue constructors for Object, Array and std::string)
	//Object, Array and std::string are now passed as value and std::move'd
	//This should handle both l and rvalues
public:
	Value(int line_num = 0); //NULL_VALUE
	Value(Object obj, int line_num);
	Value(Array arr, int line_num);
	Value(std::string str, int line_num);
	Value(double number, int line_num);
	Value(bool boolean, int line_num);

	Value(const Value& other);
	Value(Value&& other);

	Value& operator=(Value other);

	friend void swap(Value& first, Value& second);

	~Value();

	ValueType getType() const;
	uint32_t  getLineNum() const;

	const Object&      asObject() const;
	const Array&       asArray() const;
	const std::string& asString() const;
	double             asNumber() const;
	bool               asBool() const;

private:
	void*     _data;
	ValueType _type;
	uint32_t  _line_num;
};

class Object
{
private:

	struct ObjVal
	{
		Value value;
		uint32_t key_line;
	};

	std::map<std::string, ObjVal>   _values;
	std::vector<std::string>        _insertion_order;

public:
	
	struct Entry
	{
		const std::string& key;
		const Value&       value;
		uint32_t           key_line;
	};

	void insert(std::string key, Value value, uint32_t key_line)
	{
		_values[key] = { std::move(value), key_line };

		_insertion_order.push_back(std::move(key));
	}

	size_t size() const
	{
		return _insertion_order.size();
	}

	Entry operator[](size_t index) const
	{
		const std::string& key = _insertion_order[index];
		const ObjVal& val = _values.at(key);

		return{ key, val.value, val.key_line };
	}

	const Value* operator[](const std::string& key) const
	{
		auto it = _values.find(key);

		if(it == _values.end())
		{
			return nullptr;
		}

		return &it->second.value;
	}
};

bool parseSJSON(const std::string& filename, const char* data, Object& out_obj);