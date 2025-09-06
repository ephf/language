#include "parser.c"

typedef struct {
	Identifier* identifier;
	Variable* value;
	Scope* scope;
	Trace trace;
} IdentifierInfo;

IdentifierInfo new_identifier(Token base_identifier, Parser* parser) {
	unsigned long identifier_flags = 0;
	if(streq(base_identifier.trace.slice, str("extern"))) {
		identifier_flags |= fExternal;
		base_identifier = expect(parser->tokenizer, TokenIdentifier);
	}

	IdentifierInfo info = {
		.identifier = (void*) new_node((Node) { .Identifier = {
				.compiler = (void*) &comp_Identifier,
				.flags = identifier_flags,
				.base = base_identifier.trace.slice,
		}}),
		.value = find(parser->stack, base_identifier.trace),
		.scope = last(parser->stack),
		.trace = base_identifier.trace,
	};

	return info;
}
