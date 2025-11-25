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
					.type = new_type((Type) { .Wrapper = {
							.compiler = (void*) &comp_Wrapper,
							.trace = token.trace,
							.flags = fConstExpr | tfNumeric,
					}}),
					.flags = fConstExpr,
					.number = strtol(token.trace.slice.data, 0, 0),
			}});

		case TokenIdentifier: {
			if(streq(token.trace.slice, str("auto"))) {
				return (void*) new_type((Type) { .Wrapper = {
						.compiler = (void*) &comp_Wrapper,
						.trace = token.trace,
				}});
			}

			if(streq(token.trace.slice, str("typeof"))) {
				expect(parser->tokenizer, '(');
				Node* argument = right(left(parser), parser, 15);
				expect(parser->tokenizer, ')');
				return (void*) argument->type;
			}

			// TODO: sizeof() & fix segfault on function calls on missing values

			if(streq(token.trace.slice, str("const"))) {
				Type* type = (void*) right(left(parser), parser, 13);
				
				if(!(type->flags & fType)) {
					push(parser->tokenizer->messages, Err(type->trace,
								str("expected a type after '\33[35mconst\33[0m'")));
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

				Token external_token = expect(parser->tokenizer, TokenString);
				Trace trace = stretch(token.trace, external_token.trace);
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
					.Missing = {
						.compiler = (void*) &comp_Missing,
						.trace = token.trace,
					}
			});

			if(try(parser->tokenizer, '{', 0)) {
				const OpenedType opened = open_type((void*) info.value);
				StructType* const struct_type = (void*) opened.type;

				// TODO: error message if not struct
				if(struct_type->compiler != (void*) comp_StructType) {
					close_type(opened.actions, 0);
					goto ret;
				}

				StructLiteral* struct_literal = (void*) new_node((Node) { .StructLiteral = {
						.compiler = (void*) &comp_structLiteral,
						.type = (void*) info.value,
				}});

				while(parser->tokenizer->current.type
						&& parser->tokenizer->current.type != '}') {
					Node* field_value = expression(parser);
					str field_name = { 0 };

					if(try(parser->tokenizer, ':', 0)) {
						Trace field_name_trace = field_value->trace;
						field_name = field_value->trace.slice;
						unbox(field_value);
						field_value = expression(parser);

						for(size_t i = 0; i < struct_type->fields.size; i++) {
							if(streq(field_name, struct_type->fields
										.data[i]->identifier->base))
								goto found_field;
						}

						push(parser->tokenizer->messages, Err(field_name_trace,
									strf(0, "no field named '\33[35m%.*s\33[0m' on "
										"struct '\33[35m%.*s\33[0m'",
										(int) field_name_trace.slice.size,
										field_name_trace.slice.data,
										(int) info.trace.slice.size,
										info.trace.slice.data)));
						push(parser->tokenizer->messages,
								see_declaration((void*) struct_type, (void*) info.value));

					}
found_field:
					push(&struct_literal->field_names, field_name);
					push(&struct_literal->fields, field_value);

					if(!try(parser->tokenizer, ',', 0)) break;
				}
				struct_literal->trace = stretch(info.trace,
						expect(parser->tokenizer, '}').trace);
				close_type(opened.actions, 0);
				return (void*) struct_literal;
			}

ret:
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
					.type = new_type((Type) { .Wrapper = {
							.compiler = (void*) &comp_Wrapper,
							.trace = token.trace,
					}}),
					.data = token.trace.slice,
			}});		
	}

	push(parser->tokenizer->messages, Err(token.trace,
				strf(0, "expected a \33[35mliteral\33[0m, but got '\33[35m%.*s\33[0m'",
					(int) token.trace.slice.size, token.trace.slice.data)));
	return new_node((Node) {
			.compiler = (void*) &comp_NumericLiteral,
			.trace = token.trace,
			.type = new_type((Type) { .Wrapper = {
					.compiler = (void*) &comp_Wrapper,
					.trace = token.trace,
			}}),
	});
}
