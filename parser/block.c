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
	}

	if(try(parser->tokenizer, '{', 0)) {
		Scope* block = new_scope(last(parser->stack)->parent);
		push(&parser->stack, block);
		block->children = collect_until(parser, &statement, 0, '}');
		parser->stack.size--;
		return (void*) block;
	}

	Node* expr = expression(parser);
	if(!(expr->flags & fStatementTerminated))
		expect(parser->tokenizer, ';');

	if(expr->flags & fIgnoreStatment) return new_node((Node) {
			.compiler = (void*) &comp_Ignore,
	});

	return new_node((Node) { .Statement = {
			.compiler = (void*) &comp_Statement,
			.expression = expr,
	}});
}
