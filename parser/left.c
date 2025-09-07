#include "types.c"

Node* right(Node* lefthand, Parser* parser, unsigned char precedence);

Node* reference(Node* node, Trace trace) {
	if(node->flags & fType) {
		return (void*) new_type((Type) { .PointerType = {
				.compiler = (void*) &comp_PointerType,
				.trace = trace,
				.base = (void*) node,
		}});
	}

	return node;
}

Node* left(Parser* parser) {
	Token token = next(parser->tokenizer);
	
	switch(token.type) {
		case TokenNumber:
			return new_node((Node) { .NumericLiteral = {
					.compiler = (void*) &comp_NumericLiteral,
					.trace = token.trace,
					.type = new_type((Type) { .Auto = {
							.compiler = (void*) &comp_Auto,
							.trace = token.trace,
							.flags = fConstExpr | tfNumeric,
					}}),
					.flags = fConstExpr,
					.number = strtol(token.trace.slice.data, 0, 0),
			}});

		case TokenIdentifier: {
			if(streq(token.trace.slice, str("auto"))) {
				return (void*) new_type((Type) { .Auto = {
						.compiler = (void*) &comp_Auto,
						.trace = token.trace,
				}});
			}

			if(streq(token.trace.slice, str("typeof"))) {
				expect(parser->tokenizer, '(');
				Node* argument = right(left(parser), parser, 15);
				expect(parser->tokenizer, ')');
				return (void*) argument->type;
			}

			if(streq(token.trace.slice, str("const"))) {
				Type* type = (void*) right(left(parser), parser, 13);
				
				if(!(type->flags & fType)) {
					push(parser->tokenizer->messages, Err(
								type->trace,
								str("expected a type after "
									"'\33[35mconst\33[0m'")));
					type = type->type;
				}

				type->flags |= fConst;
				return (void*) type;
			}

			if(streq(token.trace.slice, str("extern"))) {
				expect(parser->tokenizer, '<');
				Type* type = (void*) expression(parser);
				if(!(type->flags & fType)) type = type->type;
				expect(parser->tokenizer, '>');

				Token external_token =
					expect(parser->tokenizer, TokenString);
				Trace trace = stretch(token.trace,
						external_token.trace);
				str data = external_token.trace.slice;
				data.data++;
				data.size -= 2;

				return new_node((Node) { .External = {
						.compiler = (void*) &comp_External,
						.flags = fConstExpr,
						.trace = trace,
						.type = type,
						.data = data,
				}});
			}

			IdentifierInfo info = new_identifier(token, parser);
			unbox((void*) info.identifier);

			if(!info.value) return (void*) new_type((Type) {
					.compiler = (void*) &comp_Missing,
					.trace = token.trace,
			});

			return (void*) info.value;
		}

		case '(': {
			Node* expression = right(left(parser), parser, 15);
			expect(parser->tokenizer, ')');
			return expression;
		}

		case TokenString:
			return new_node((Node) { .External = {
					.compiler = (void*) &comp_External,
					.trace = token.trace,
					.type = new_type((Type) { .Auto = {
							.compiler = (void*) &comp_Auto,
							.trace = token.trace,
					}}),
					.data = token.trace.slice,
			}});		
	}

	push(parser->tokenizer->messages, Err(token.trace,
				strf(0, "expected a \33[35mliteral\33[0m, "
					"but got '\33[35m%.*s\33[0m'",
					(int) token.trace.slice.size,
					token.trace.slice.data)));
	return new_node((Node) {
			.compiler = (void*) &comp_NumericLiteral,
			.trace = token.trace,
			.type = new_type((Type) { .Auto = {
					.compiler = (void*) &comp_Auto,
					.trace = token.trace,
			}}),
	});
}
