/*!	\file	parser.cpp
	\brief	Parser class implementation.
	\author	Garth Santor
	\date	2021-10-29
	\copyright	Garth Santor, Trinh Han
=============================================================
Revision History
-------------------------------------------------------------

Version 2021.11.01
	C++ 20 validated
	Changed to GATS_TEST

Version 2014.10.31
	Visual C++ 2013

Version 2012.11.13
	C++ 11 cleanup

Version 2009.12.02
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

#include <ee/parser.hpp>
#include <ee/function.hpp>
#include <ee/operand.hpp>
#include <ee/operator.hpp>
#include <ee/pseudo_operation.hpp>
#include <stack>
#include <stdexcept>


[[nodiscard]] TokenList Parser::parse(TokenList const& infixTokens) {
        auto precedence = [](Token::pointer_type const& token) {
                if (is<Factorial>(token)) return 15;
                if (is<Power>(token)) return 14;
                if (is<Identity>(token) || is<Negation>(token) || is<Not>(token)) return 13;
                if (is<Multiplication>(token) || is<Division>(token) || is<Modulus>(token)) return 12;
                if (is<Addition>(token) || is<Subtraction>(token)) return 11;
                if (is<Less>(token) || is<LessEqual>(token) || is<Greater>(token) || is<GreaterEqual>(token)) return 9;
                if (is<Equality>(token) || is<Inequality>(token)) return 8;
                if (is<And>(token) || is<Nand>(token)) return 6;
                if (is<Xor>(token) || is<Xnor>(token)) return 5;
                if (is<Or>(token) || is<Nor>(token)) return 4;
                if (is<Assignment>(token)) return 1;
                return 0;
        };

        auto is_right_associative = [](Token::pointer_type const& token) {
                return is<Power>(token) || is<Assignment>(token);
        };

        TokenList output;
        std::stack<Token::pointer_type> opStack;

        for (auto const& token : infixTokens) {
                if (is<Operand>(token)) {
                        output.push_back(token);
                        continue;
                }

                if (is<Function>(token)) {
                        opStack.push(token);
                        continue;
                }

                if (is<ArgumentSeparator>(token)) {
                        while (!opStack.empty() && !is<LeftParenthesis>(opStack.top())) {
                                output.push_back(opStack.top());
                                opStack.pop();
                        }
                        continue;
                }

                if (is<LeftParenthesis>(token)) {
                        opStack.push(token);
                        continue;
                }

                if (is<RightParenthesis>(token)) {
                        while (!opStack.empty() && !is<LeftParenthesis>(opStack.top())) {
                                output.push_back(opStack.top());
                                opStack.pop();
                        }

                        if (opStack.empty())
                                throw std::runtime_error("Right parenthesis has no matching left parenthesis");

                        opStack.pop();

                        if (!opStack.empty() && is<Function>(opStack.top())) {
                                output.push_back(opStack.top());
                                opStack.pop();
                        }
                        continue;
                }

                if (is<Operator>(token)) {
                        while (!opStack.empty() && is<Operator>(opStack.top())) {
                                auto const top = opStack.top();
                                auto const topPrec = precedence(top);
                                auto const curPrec = precedence(token);
                                if (topPrec > curPrec || (topPrec == curPrec && !is_right_associative(token))) {
                                        output.push_back(top);
                                        opStack.pop();
                                }
                                else
                                        break;
                        }
                        opStack.push(token);
                        continue;
                }
        }

        while (!opStack.empty()) {
                        if (is<LeftParenthesis>(opStack.top()))
                                throw std::runtime_error("Missing right-parenthesis");
                        output.push_back(opStack.top());
                        opStack.pop();
        }

        return output;
}
