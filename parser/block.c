#include "right.c"

NodeList collect_until(Parser* parser, Node* (*supplier)(Parser*),
		unsigned char divider, unsigned char terminator) {
	NodeList collection = { 0 };

	while(parser->tokenizer->current.type &&
			parser->tokenizer->current.type != terminator) {
		push(&collection, supplier(parser));
		if(divider && !try(parser->tokenizer, divider, 0)) break;
	}
	if(terminator) expect(parser->tokenizer, terminator);

	return collection;
}

Node* statement(Parser* parser) {
	if(parser->tokenizer->current.type == TokenIdentifier) {
		if(streq(parser->tokenizer->current.trace.slice,
					str("return"))) {
			Trace trace_start = next(parser->tokenizer).trace;
			Node* value = expression(parser);
			expect(parser->tokenizer, ';');

			if(last(parser->stack)->parent->compiler !=
					(void*) &comp_FunctionDeclaration) {
				push(parser->tokenizer->messages, Err(trace_start,
							str("return statement needs to be "
								"inside of a function")));
			} else {
				clash_types(last(parser->stack)->parent
						->FunctionDeclaration.type
						->FunctionType.signature.data[0],
						value->type, value->trace,
						parser->tokenizer->messages);
			}

			return new_node((Node) { .ReturnStatement = {
					.compiler = (void*) &comp_ReturnStatement,
					.value = value,
			}});
		}

		if(streq(parser->tokenizer->current.trace.slice,
					str("struct"))) {
			Trace trace_start = next(parser->tokenizer).trace;
			IdentifierInfo info = new_identifier(
					expect(parser->tokenizer, TokenIdentifier),
					parser);

			StructType* type = (void*) new_type((Type) {
					.StructType = {
						.compiler = (void*) &comp_StructType,
						.flags = fConstExpr,
						.trace = stretch(trace_start, info.trace),
						.identifier = info.identifier,
					}
			});
			type->body = new_scope((void*) type);
			// TODO: create a flag that only allows type to compile
			// if it is pointed to (in reference())
			// this will prevent circular types and allow structs
			// to reference themselves within themselves

			Declaration* declaration = (void*) new_node((Node) {
					.VariableDeclaration = {
						.compiler = (void*) &comp_VariableDeclaration,
						.flags = fConst | fType,
						.trace = type->trace,
						.type = (void*) type,
						.const_value = (void*) type,
						.identifier = info.identifier,
					}
			});
			put(info.scope, info.identifier->base, declaration);

			expect(parser->tokenizer, '{');

			Node* possible_field = 0;
			while(parser->tokenizer->current.type &&
					parser->tokenizer->current.type != '}' &&
					(possible_field = expression(parser))
						->compiler == (void*) &comp_Variable &&
					possible_field->flags & fIgnoreStatment) {
				possible_field->Variable.declaration->is_inline = 1;
				push(&type->fields,
						(void*) possible_field->Variable.declaration);
				expect(parser->tokenizer, ';');
			}

			if(!try(parser->tokenizer, '}', 0)) {
				NodeList declarations = collect_until(parser,
						&statement, 0, '}');
				type->body->children = declarations;
			}

			return new_node((Node) { .compiler = &comp_Ignore });
		}
	}

	Node* expr = expression(parser);
	if(!(expr->flags & fStatementTerminated))
		expect(parser->tokenizer, ';');

	if(expr->flags & fIgnoreStatment) return new_node((Node) {
			.compiler = &comp_Ignore,
	});

	return new_node((Node) { .Statement = {
			.compiler = (void*) &comp_Statement,
			.expression = expr,
	}});
}
