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
	RightFieldAccess,
};

typedef struct {
	unsigned char precedence : 4,
				  type : 4;
} RightOperator;

RightOperator right_operator_table[128] = {
	['.'] = { 1, RightFieldAccess },
	['('] = { 1, RightCall },
	['*'] = { 3, RightAltBinary }, ['/'] = { 3 }, ['%'] = { 3 },
	['+'] = { 4 }, ['-'] = { 4 },
	[TokenIdentifier] = { 13, RightDeclaration },
	['='] = { 14, RightAssignment },
};

Node* expression(Parser* parser) {
	return right(left(parser), parser, 15);
}

int filter_missing(Type* type, void* ignore) {
	return type->compiler == (void*) &comp_Missing;
}

void recycle_missing(Type* missing, Parser* parser) {
	Variable* possible_found = find(parser->stack, missing->trace);

	if(possible_found && possible_found->flags & fType) {
		*missing = *(Type*)(void*) possible_found;
	}

	unbox((void*) possible_found);
}

Variable* declaration(Node* type, Token identifier, Parser* parser) {
	// TODO: insert scope before calling info to catch generic
	// type declarations
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
		info.identifier->declaration = (void*) declaration;

		apply_generics((void*) declaration, info.generics_collection);
		sift_type((void*) type, (void*) &recycle_missing, parser, 0);

		push(&parser->stack, declaration->body);
		push(&info.scope->declarations, (void*) declaration);
		put(info.scope, info.identifier->base, (void*) declaration);

		NodeList argument_declarations = collect_until(parser, &expression, ',', ')');

		for(size_t i = 0; i < argument_declarations.size; i++) {
			if(argument_declarations.data[i]->compiler
					== (void*) &comp_Variable
					&& argument_declarations.data[i]->flags
						& fIgnoreStatment) {
				push(&function_type->signature, argument_declarations.data[i]->type);
				VariableDeclaration* argument =
					(void*) argument_declarations.data[i]->Variable.declaration;
				argument->is_inline = 1;
				push(&declaration->arguments, argument);
			} else {
				push(parser->tokenizer->messages, Err(
							argument_declarations.data[i]->trace,
							str("expected an argument declaration")));
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
				fIgnoreStatment | fStatementTerminated * !(declaration->flags & fExternal));
	}

	VariableDeclaration* declaration =
		(void*) new_node((Node) { .VariableDeclaration = {
				.compiler = (void*) &comp_VariableDeclaration,
				.trace = stretch(type->trace, info.trace),
				.type = (void*) type,
				.identifier = info.identifier,
		}});
	info.identifier->declaration = (void*) declaration;

	push(&info.scope->declarations, (void*) declaration);
	put(info.scope, info.identifier->base, (void*) declaration);

	if(type->flags & fConst && try(parser->tokenizer, '=', 0)) {
		declaration->const_value = right(left(parser), parser, 14);
		clash_types(declaration->type, declaration->const_value->type,
				declaration->trace, parser->tokenizer->messages);
	}

	return variable_of((void*) declaration, declaration->trace, fIgnoreStatment);
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

				const OpenedType opened_function_type = open_type(lefthand->type);
				Type* const open_function_type = opened_function_type.open_type;

				if(open_function_type->compiler != (void*) &comp_FunctionType) {
					push(parser->tokenizer->messages, Err(lefthand->trace,
								str("calling a non-function value")));

					return_type = new_type((Type) { .Auto = {
							.compiler = (void*) &comp_Auto,
							.trace = lefthand->trace,
					}});
				} else {
					signature = open_function_type->FunctionType.signature;
					return_type = signature.data[0];
				}

				NodeList arguments = collect_until(parser, &expression, ',', ')');
				for(size_t i = 0; i < arguments.size; i++) {
					if(i + 1 >= signature.size) {
						push(parser->tokenizer->messages, Err(
									stretch(arguments.data[i]->trace,
										last(arguments)->trace),
									str("too many arguments in function call")));
						push(parser->tokenizer->messages,
								see_declaration((void*) open_function_type
									->FunctionType.declaration, lefthand));
						break;
					}

					clash_types(signature.data[i + 1], arguments.data[i]->type,
							arguments.data[i]->trace, parser->tokenizer->messages);
				}

				if(arguments.size + 1 < signature.size) {
					push(parser->tokenizer->messages, Err(lefthand->trace,
								str("not enough arguments in function call")));
					push(parser->tokenizer->messages,
							see_declaration((void*) lefthand->type->FunctionType.declaration,
								lefthand));
				}
				
				return_type = make_type_standalone(return_type);
				close_type(opened_function_type.actions);

				return new_node((Node) { .FunctionCall = {
						.compiler = (void*) &comp_FunctionCall,
						.type = return_type,
						.trace = lefthand->trace,
						.function = lefthand,
						.arguments = arguments,
				}});
			}

			case RightFieldAccess: {
				const OpenedType opened = open_type(
						(void*) lefthand->type);
				StructType* const struct_type = 
					(void*) opened.open_type;

				str operator_token =
					next(parser->tokenizer).trace.slice;
				Token field_token = expect(parser->tokenizer,
						TokenIdentifier);

				// TODO: error message if not struct
				if(struct_type->compiler != (void*) &comp_StructType) {
					return lefthand;
				}

				ssize_t found_index = -1;
				for(size_t i = 0; i < struct_type->fields.size; i++) {
					if(streq(field_token.trace.slice,
								struct_type->fields.data[i]
								->identifier->base)) {
						found_index = i;
					}
				}

				if(found_index < 0) {
					push(parser->tokenizer->messages, Err(
								field_token.trace, strf(0,
									"no field named "
									"'\33[35m%.*s\33[0m' on struct "
									"'\33[35m%.*s\33[0m'",
									(int) field_token.trace.slice.size,
									field_token.trace.slice.data,
									(int) lefthand->trace.slice.size,
									lefthand->trace.slice.data)));
					push(parser->tokenizer->messages,
							see_declaration((void*) struct_type,
								lefthand));
					break;
				}

				Type* field_type = make_type_standalone(
						struct_type->fields.data[found_index]->type);
				close_type(opened.actions);

				lefthand = new_node((Node) { .BinaryOperation = {
						.compiler = (void*) &comp_BinaryOperation,
						.flags = fMutable | (lefthand->flags & fConstExpr),
						.trace = stretch(lefthand->trace, field_token.trace),
						.type = field_type,
						.left = lefthand,
						.operator = operator_token,
						.right = new_node((Node) { .External = {
								.compiler = (void*) &comp_External,
								.data = field_token.trace.slice,
						}}),
				}});
				break;
			}
		}
	}

	return lefthand;
}
