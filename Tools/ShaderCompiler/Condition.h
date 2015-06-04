#pragma once

#include "Common.h"

#include <cassert>

class OptionValue : public Condition
{
public:
	OptionValue(Permutation option, Permutation option_group_mask)
		: _option(option), _option_group_mask(option_group_mask)
	{}

	bool evaluate(Permutation permutation) override
	{
		return (permutation & _option_group_mask) == _option;
	}
private:
	Permutation _option;
	Permutation _option_group_mask;
};

enum class OperationType : u8
{
	INVALID,
	NOT,
	AND,
	OR
};

class Operation : public Condition
{
public:
	Operation(OperationType type, std::shared_ptr<Condition> left, std::shared_ptr<Condition> right) 
		: _left(left), _right(right), _type(type)
	{}

	bool evaluate(Permutation permutation) override
	{
		switch(_type)
		{
		case OperationType::INVALID:
			assert(false);
			return false;
		case OperationType::NOT:
			return !_left->evaluate(permutation);
		case OperationType::AND:
			return _left->evaluate(permutation) && _right->evaluate(permutation);
		case OperationType::OR:
			return _left->evaluate(permutation) || _right->evaluate(permutation);
		default:
			assert(false);
			return false;
		}
	}
private:
	std::shared_ptr<Condition> _left;
	std::shared_ptr<Condition> _right;
	OperationType              _type;
};