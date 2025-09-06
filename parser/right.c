#include "left.c"

NodeList collect_until(Parser* parser, Node* (*supplier)(Parser*),
		unsigned char divider, unsigned char terminator);
Node* right(Node* lefthand, Parser* parser, unsigned char precedence);
Node* statement(Parser* parser);

enum {
	RightBinary,
	RightAltBinary,
	RightDeclaration,
	RightAssignment,
	RightCall,
};

typedef struct {
	unsigned char precedence : 4,
				  type : 4;
} RightOperator;

RightOperator right_operator_table[128] = {
	['('] = { 1, RightCall },
	['*'] = { 3, RightAltBinary }, ['/'] = { 3 }, ['%'] = { 3 },
	['+'] = { 4 }, ['-'] = { 4 },
	[TokenIdentifier] = { 14, RightDeclaration },
	['='] = { 14, RightAssignment },
};

Node* expression(Parser* parser) {
	return right(left(parser), parser, 15);
}

Variable* declaration(Node* type, Token identifier, Parser* parser) {
	if(!(type->flags & fType)) {
		push(parser->tokenizer->messages, Err(type->trace,
					str("expected a type before "
						"declaration identifier")));
		push(parser->tokenizer->messages, Hint(
					str("also try '\33[35mtypeof(expr)\33[0m'")));
		type = (void*) type->type;
	}

	IdentifierInfo info = new_identifier(identifier, parser);

	if(try(parser->tokenizer, '(', 0)) {
		Trace trace_start = stretch(type->trace, info.trace);
		
		FunctionType* function_type = (void*) new_type((Type) {
				.FunctionType = {
					.compiler = (void*) &comp_FunctionType,
					.trace = trace_start,
				}
		});
		push(&function_type->signature, (void*) type);

		FunctionDeclaration* declaration = (void*) new_node((Node) {
				.FunctionDeclaration = {
					.compiler = (void*) &comp_FunctionDeclaration,
					.flags = info.identifier->flags & fExternal,
					.trace = trace_start,
					.type = (void*) function_type,
					.identifier = info.identifier,
				}
		});
		declaration->body = new_scope((void*) declaration);
		function_type->declaration = declaration;

		push(&parser->stack, declaration->body);
		push(&info.scope->declarations, (void*) declaration);
		put(info.scope, info.identifier->base, (void*) declaration);

		NodeList argument_declarations = collect_until(
				parser, &expression, ',', ')');

		for(size_t i = 0; i < argument_declarations.size; i++) {
			if(argument_declarations.data[i]->compiler
					!= (void*) &comp_Variable
					&& argument_declarations.data[i]->flags
						& fIgnoreStatment) {
				push(parser->tokenizer->messages, Err(
							argument_declarations.data[i]->trace,
							str("expected an argument declaration")));
			} else {
				push(&function_type->signature,
						argument_declarations.data[i]->type);
				VariableDeclaration* argument =
					(void*) argument_declarations.data[i]->Variable
						.declaration;
				argument->is_inline = 1;
				push(&declaration->arguments, argument);
			}
		}
		free(argument_declarations.data);

		if(!(declaration->flags & fExternal)) {
			expect(parser->tokenizer, '{');
			declaration->body->children = collect_until(parser,
					&statement, 0, '}');
		}

		parser->stack.size--;
		return variable_of((void*) declaration, declaration->trace,
				fIgnoreStatment |
				fStatementTerminated
					* !(declaration->flags & fExternal));
	}

	VariableDeclaration* declaration =
		(void*) new_node((Node) { .VariableDeclaration = {
				.compiler = (void*) &comp_VariableDeclaration,
				.trace = stretch(type->trace, info.trace),
				.type = (void*) type,
				.identifier = info.identifier,
		}});

	push(&info.scope->declarations, (void*) declaration);
	put(info.scope, info.identifier->base, (void*) declaration);

	if(type->flags & fConst && try(parser->tokenizer, '=', 0)) {
		declaration->const_value = right(left(parser), parser, 14);
		clash_types(declaration->type, declaration->const_value->type,
				declaration->trace, parser->tokenizer->messages);
	}

	return variable_of((void*) declaration,
			declaration->trace, fIgnoreStatment);
}

Message see_declaration(Declaration* declaration, Node* node) {
	return See(declaration->trace,
				strf(0, "declaration of '\33[35m%.*s\33[0m'",
					(int) node->trace.slice.size,
					node->trace.slice.data));
}

Node* right(Node* lefthand, Parser* parser, unsigned char precedence) {
	RightOperator operator;
outer_while:
	while((operator = right_operator_table
				[parser->tokenizer->current.type]).precedence) {
		if(operator.precedence >= precedence
				+ (operator.type == RightAssignment)) break;

		switch(operator.type) {
			case RightAltBinary: {
				switch(parser->tokenizer->current.type) {
					case '*':
						if(!(lefthand->flags & fType)) break;
						lefthand = reference(lefthand, stretch(
									lefthand->trace,
									next(parser->tokenizer).trace));
						goto outer_while;
				}
				goto binary;
			}

			case RightAssignment:
				if(!(lefthand->flags & fMutable) ||
						lefthand->type->flags & fConst) {
					push(parser->tokenizer->messages, Err(
								lefthand->trace,
								str("left hand of assignment "
									"is not a mutable value")));
				}
			case RightBinary: binary: {
				Token operator_token = next(parser->tokenizer);
				Node* righthand = right(left(parser), parser,
						operator.precedence);
				clash_types(lefthand->type, righthand->type,
						stretch(lefthand->trace, righthand->trace),
						parser->tokenizer->messages);

				lefthand = new_node((Node) { .BinaryOperation = {
						.compiler = (void*) &comp_BinaryOperation,
						.flags = lefthand->flags & righthand->flags,
						.trace = stretch(lefthand->trace,
								righthand->trace),
						.type = lefthand->type,
						.left = lefthand,
						.operator = operator_token.trace.slice,
						.right = righthand,
				}});
				break;
			}

			case RightDeclaration: {
				if(!(lefthand->flags & fType))
					return lefthand;
				lefthand = (void*) declaration(lefthand,
						next(parser->tokenizer), parser);
				break;
			}

			case RightCall: {
				next(parser->tokenizer);

				TypeList signature = { 0 };
				Type* return_type;

				if(lefthand->type->compiler !=
						(void*) &comp_FunctionType) {
					push(parser->tokenizer->messages, Err(
								lefthand->trace,
								str("calling a non-function value")));

					return_type = new_type((Type) { .Auto = {
							.compiler = (void*) &comp_Auto,
							.trace = lefthand->trace,
					}});
				} else {
					signature = lefthand->type->FunctionType
						.signature;
					return_type = signature.data[0];
				}

				NodeList arguments = collect_until(parser,
						&expression, ',', ')');
				for(size_t i = 0; i < arguments.size; i++) {
					if(i + 1 >= signature.size) {
						push(parser->tokenizer->messages, Err(
									stretch(
										arguments.data[i]->trace,
										last(arguments)->trace),
									str("too many arguments in "
										"function call")));
						push(parser->tokenizer->messages,
								see_declaration((void*) lefthand->type
										->FunctionType.declaration,
									lefthand));
						break;
					}

					clash_types(signature.data[i + 1],
							arguments.data[i]->type,
							arguments.data[i]->trace,
							parser->tokenizer->messages);
				}

				if(arguments.size + 1 < signature.size) {
					push(parser->tokenizer->messages, Err(
								lefthand->trace,
								str("not enough arguments in "
									"function call")));
					push(parser->tokenizer->messages,
							see_declaration((void*) lefthand->type
									->FunctionType.declaration,
								lefthand));
				}
				
				return new_node((Node) { .FunctionCall = {
						.compiler = (void*) &comp_FunctionCall,
						.type = return_type,
						.trace = lefthand->trace,
						.function = lefthand,
						.arguments = arguments,
				}});
			}
		}
	}

	return lefthand;
}
