/*!	\file	RPNEvaluator.cpp
	\brief	RPN Evaluator class implementation.
	\author	Garth Santor
	\date	2021-10-29
	\copyright	Garth Santor, Trinh Han

=============================================================
Revision History
-------------------------------------------------------------

Version 2021.11.01
	C++ 20 validated
	Changed to GATS_TEST

Version 2012.11.13
	C++ 11 cleanup

Version 2009.12.10
	Alpha release.

=============================================================

Copyright Garth Santor/Trinh Han

The copyright to the computer program(s) herein
is the property of Garth Santor/Trinh Han, Canada.
The program(s) may be used and/or copied only with
the written permission of Garth Santor/Trinh Han
or in accordance with the terms and conditions
stipulated in the agreement/contract under which
the program(s) have been supplied.
=============================================================*/

#include <ee/RPNEvaluator.hpp>
#include <ee/integer.hpp>
#include <ee/operation.hpp>
#include <ee/function.hpp>
#include <ee/real.hpp>
#include <ee/boolean.hpp>
#include <ee/variable.hpp>
#include <cassert>
#include <stack>
#include <variant>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <functional>





[[nodiscard]] Operand::pointer_type RPNEvaluator::evaluate( TokenList const& rpnExpression ) {
        using Value = std::variant<Integer::value_type, Real::value_type, bool>;

        std::function<Value(Operand::pointer_type const&)> to_value = [&](Operand::pointer_type const& operand) -> Value {
                if (is<Integer>(operand)) return value_of<Integer>(operand);
                if (is<Real>(operand)) return value_of<Real>(operand);
                if (is<Boolean>(operand)) return value_of<Boolean>(operand);
                if (is<Variable>(operand)) {
                        auto var = convert<Variable>(operand);
                        if (!var->value())
                                throw std::runtime_error("Error: variable not initialized");
                        return to_value(var->value());
                }
                throw std::runtime_error("Error: unsupported operand");
        };

        auto make_operand_from_value = [](Value const& value) -> Operand::pointer_type {
                if (std::holds_alternative<Integer::value_type>(value))
                        return make_operand<Integer>(std::get<Integer::value_type>(value));
                if (std::holds_alternative<Real::value_type>(value))
                        return make_operand<Real>(std::get<Real::value_type>(value));
                return make_operand<Boolean>(std::get<bool>(value));
        };

        auto promote = [](Value const& lhs, Value const& rhs) -> std::pair<Value, Value> {
                if (std::holds_alternative<Real::value_type>(lhs) || std::holds_alternative<Real::value_type>(rhs)) {
                        auto to_real = [](Value const& v) {
                                if (std::holds_alternative<Real::value_type>(v)) return std::get<Real::value_type>(v);
                                if (std::holds_alternative<Integer::value_type>(v)) return Real::value_type(std::get<Integer::value_type>(v));
                                return Real::value_type(std::get<bool>(v));
                        };
                        return { to_real(lhs), to_real(rhs) };
                }
                return { lhs, rhs };
        };

        OperandList stack;

        for (auto const& token : rpnExpression) {
                if (is<Operand>(token)) {
                        stack.push_back(convert<Operand>(token));
                        continue;
                }

                if (!is<Operation>(token))
                        continue;

                if (is<PostfixOperator>(token)) {
                        if (stack.size() < 1)
                                throw std::runtime_error("Error: insufficient operands");
                        auto operand = stack.back();
                        stack.pop_back();
                        Value val = to_value(operand);
                        if (!std::holds_alternative<Integer::value_type>(val))
                                throw std::runtime_error("Error: unsupported operand");
                        auto ival = std::get<Integer::value_type>(val);
                        if (ival < 0)
                                throw std::runtime_error("Error: unsupported operand");
                        Integer::value_type result = 1;
                        for (Integer::value_type i = 1; i <= ival; ++i)
                                result *= i;
                        stack.push_back(make_operand<Integer>(result));
                        continue;
                }

                if (is<UnaryOperator>(token)) {
                        if (stack.size() < 1)
                                throw std::runtime_error("Error: insufficient operands");
                        auto operand = stack.back();
                        stack.pop_back();
                        Value val = to_value(operand);

                        if (is<Identity>(token)) {
                                stack.push_back(operand);
                                continue;
                        }
                        if (is<Negation>(token)) {
                                if (std::holds_alternative<Real::value_type>(val))
                                        stack.push_back(make_operand<Real>(-std::get<Real::value_type>(val)));
                                else if (std::holds_alternative<Integer::value_type>(val))
                                        stack.push_back(make_operand<Integer>(-std::get<Integer::value_type>(val)));
                                else
                                        throw std::runtime_error("Error: unsupported operand");
                                continue;
                        }
                        if (is<Not>(token)) {
                                bool b{};
                                if (std::holds_alternative<bool>(val))
                                        b = std::get<bool>(val);
                                else
                                        throw std::runtime_error("Error: unsupported operand");
                                stack.push_back(make_operand<Boolean>(!b));
                                continue;
                        }
                }

                // binary operators and functions
                if (is<BinaryOperator>(token)) {
                                if (stack.size() < 2)
                                        throw std::runtime_error("Error: insufficient operands");
                                auto rhsOp = stack.back(); stack.pop_back();
                                auto lhsOp = stack.back(); stack.pop_back();
                                Value rhs = to_value(rhsOp);
                                Value lhs = to_value(lhsOp);

                        if (is<Assignment>(token)) {
                                if (!is<Variable>(lhsOp))
                                        throw std::runtime_error("Error: assignment to a non-variable.");
                                auto var = convert<Variable>(lhsOp);
                                var->set(make_operand_from_value(rhs));
                                stack.push_back(var);
                                continue;
                        }

                        auto [lProm, rProm] = promote(lhs, rhs);

                        auto make_bool = [&](bool value) { stack.push_back(make_operand<Boolean>(value)); };
                        auto make_int = [&](Integer::value_type value) { stack.push_back(make_operand<Integer>(value)); };
                        auto make_real = [&](Real::value_type value) { stack.push_back(make_operand<Real>(value)); };

                        if (is<Addition>(token)) {
                                if (std::holds_alternative<Real::value_type>(lProm))
                                        make_real(std::get<Real::value_type>(lProm) + std::get<Real::value_type>(rProm));
                                else
                                        make_int(std::get<Integer::value_type>(lProm) + std::get<Integer::value_type>(rProm));
                                continue;
                        }
                        if (is<Subtraction>(token)) {
                                if (std::holds_alternative<Real::value_type>(lProm))
                                        make_real(std::get<Real::value_type>(lProm) - std::get<Real::value_type>(rProm));
                                else
                                        make_int(std::get<Integer::value_type>(lProm) - std::get<Integer::value_type>(rProm));
                                continue;
                        }
                        if (is<Multiplication>(token)) {
                                if (std::holds_alternative<Real::value_type>(lProm))
                                        make_real(std::get<Real::value_type>(lProm) * std::get<Real::value_type>(rProm));
                                else
                                        make_int(std::get<Integer::value_type>(lProm) * std::get<Integer::value_type>(rProm));
                                continue;
                        }
                        if (is<Division>(token)) {
                                if (std::holds_alternative<Real::value_type>(lProm))
                                        make_real(std::get<Real::value_type>(lProm) / std::get<Real::value_type>(rProm));
                                else
                                        make_int(std::get<Integer::value_type>(lProm) / std::get<Integer::value_type>(rProm));
                                continue;
                        }
                        if (is<Modulus>(token)) {
                                make_int(std::get<Integer::value_type>(lProm) % std::get<Integer::value_type>(rProm));
                                continue;
                        }
                        if (is<Power>(token)) {
                                if (std::holds_alternative<Real::value_type>(lProm))
                                        make_real(pow(std::get<Real::value_type>(lProm), std::get<Real::value_type>(rProm)));
                                else {
                                        auto base = std::get<Integer::value_type>(lProm);
                                        auto exp = std::get<Integer::value_type>(rProm);
                                        Integer::value_type result = 1;
                                        for (Integer::value_type i = 0; i < exp; ++i)
                                                result *= base;
                                        make_int(result);
                                }
                                continue;
                        }
                        if (is<Equality>(token)) { make_bool(lProm == rProm); continue; }
                        if (is<Inequality>(token)) { make_bool(lProm != rProm); continue; }
                        if (is<Less>(token)) {
                                if (std::holds_alternative<Real::value_type>(lProm))
                                        make_bool(std::get<Real::value_type>(lProm) < std::get<Real::value_type>(rProm));
                                else
                                        make_bool(std::get<Integer::value_type>(lProm) < std::get<Integer::value_type>(rProm));
                                continue;
                        }
                        if (is<LessEqual>(token)) {
                                if (std::holds_alternative<Real::value_type>(lProm))
                                        make_bool(std::get<Real::value_type>(lProm) <= std::get<Real::value_type>(rProm));
                                else
                                        make_bool(std::get<Integer::value_type>(lProm) <= std::get<Integer::value_type>(rProm));
                                continue;
                        }
                        if (is<Greater>(token)) {
                                if (std::holds_alternative<Real::value_type>(lProm))
                                        make_bool(std::get<Real::value_type>(lProm) > std::get<Real::value_type>(rProm));
                                else
                                        make_bool(std::get<Integer::value_type>(lProm) > std::get<Integer::value_type>(rProm));
                                continue;
                        }
                        if (is<GreaterEqual>(token)) {
                                if (std::holds_alternative<Real::value_type>(lProm))
                                        make_bool(std::get<Real::value_type>(lProm) >= std::get<Real::value_type>(rProm));
                                else
                                        make_bool(std::get<Integer::value_type>(lProm) >= std::get<Integer::value_type>(rProm));
                                continue;
                        }
                        if (is<And>(token)) { make_bool(std::get<bool>(lhs) && std::get<bool>(rhs)); continue; }
                        if (is<Or>(token)) { make_bool(std::get<bool>(lhs) || std::get<bool>(rhs)); continue; }
                        if (is<Xor>(token)) { make_bool(std::get<bool>(lhs) ^ std::get<bool>(rhs)); continue; }
                        if (is<Nand>(token)) { make_bool(!(std::get<bool>(lhs) && std::get<bool>(rhs))); continue; }
                        if (is<Nor>(token)) { make_bool(!(std::get<bool>(lhs) || std::get<bool>(rhs))); continue; }
                        if (is<Xnor>(token)) { make_bool(std::get<bool>(lhs) == std::get<bool>(rhs)); continue; }
                }

                if (is<Function>(token)) {
                        if (is<OneArgFunction>(token)) {
                                if (stack.size() < 1)
                                        throw std::runtime_error("Error: insufficient operands");
                                auto arg = stack.back(); stack.pop_back();
                                Value v = to_value(arg);
                                auto get_real = [&](Value const& val) { return std::holds_alternative<Real::value_type>(val) ? std::get<Real::value_type>(val) : Real::value_type(std::get<Integer::value_type>(val)); };
                                if (is<Abs>(token)) {
                                        if (std::holds_alternative<Real::value_type>(v))
                                                stack.push_back(make_operand<Real>(abs(std::get<Real::value_type>(v))));
                                        else
                                                stack.push_back(make_operand<Integer>(abs(std::get<Integer::value_type>(v))));
                                } else if (is<Sin>(token)) { stack.push_back(make_operand<Real>(sin(get_real(v)))); }
                                else if (is<Cos>(token)) { stack.push_back(make_operand<Real>(cos(get_real(v)))); }
                                else if (is<Tan>(token)) { stack.push_back(make_operand<Real>(tan(get_real(v)))); }
                                else if (is<Sqrt>(token)) { stack.push_back(make_operand<Real>(sqrt(get_real(v)))); }
                                else if (is<Ln>(token)) { stack.push_back(make_operand<Real>(log(get_real(v)))); }
                                else if (is<Lb>(token)) { stack.push_back(make_operand<Real>(log2(get_real(v)))); }
                                else if (is<Log>(token)) { stack.push_back(make_operand<Real>(log10(get_real(v)))); }
                                else if (is<Exp>(token)) { stack.push_back(make_operand<Real>(exp(get_real(v)))); }
                                else if (is<Floor>(token)) { stack.push_back(make_operand<Real>(floor(get_real(v)))); }
                                else if (is<Ceil>(token)) { stack.push_back(make_operand<Real>(ceil(get_real(v)))); }
                                else if (is<Arccos>(token)) { stack.push_back(make_operand<Real>(acos(get_real(v)))); }
                                else if (is<Arcsin>(token)) { stack.push_back(make_operand<Real>(asin(get_real(v)))); }
                                else if (is<Arctan>(token)) { stack.push_back(make_operand<Real>(atan(get_real(v)))); }
                                else if (is<Result>(token)) {
                                        throw std::runtime_error("Error: unsupported operand");
                                }
                                continue;
                        }

                        if (is<TwoArgFunction>(token)) {
                                if (stack.size() < 2)
                                        throw std::runtime_error("Error: insufficient operands");
                                auto rhsOp = stack.back(); stack.pop_back();
                                auto lhsOp = stack.back(); stack.pop_back();
                                Value rhs = to_value(rhsOp);
                                Value lhs = to_value(lhsOp);
                                auto get_real = [&](Value const& val) { return std::holds_alternative<Real::value_type>(val) ? std::get<Real::value_type>(val) : Real::value_type(std::get<Integer::value_type>(val)); };
                                if (is<Arctan2>(token)) { stack.push_back(make_operand<Real>(atan2(get_real(lhs), get_real(rhs)))); }
                                else if (is<Max>(token)) { stack.push_back(make_operand<Real>(std::max(get_real(lhs), get_real(rhs)))); }
                                else if (is<Min>(token)) { stack.push_back(make_operand<Real>(std::min(get_real(lhs), get_real(rhs)))); }
                                else if (is<Pow>(token)) { stack.push_back(make_operand<Real>(pow(get_real(lhs), get_real(rhs)))); }
                                continue;
                        }
                }
        }

        if (stack.empty())
                throw std::runtime_error("Error: insufficient operands");
        if (stack.size() != 1)
                throw std::runtime_error("Error: too many operands");
        return stack.back();
}

