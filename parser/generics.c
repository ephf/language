#include "parser.c"

Type* clone_base_type(Type* type);
NodeList collect_until(Parser* parser, Node* (*supplier)(Parser*),
		unsigned char divider, unsigned char terminator);
Node* expression(Parser* parser);
Message see_declaration(Declaration* declaration, Node* node);
void clash_types(Type* a, Type* b, Trace trace, Messages* messages);
void stringify_type(Type* type, str* string, unsigned flags);

Type* wrap_applied_generics(Type* type, TypeList generics,
		Declaration* declaration) {
	return new_type((Type) { .Auto = {
			.compiler = (void*) &comp_Auto,
			.trace = type->trace,
			.flags = type->flags,
			.generics = generics,
			.generics_declaration = declaration,
			.ref = type,
	}});
}

void assign_generics(Variable* variable, Parser* parser) {
	Declaration* const declaration = variable->declaration;
	if(!declaration->generics.stack.size) return;
	const TypeList base_generics = declaration->generics.stack.data[0];

	TypeList input_generics = { 0 };
	for(size_t i = 0; i < base_generics.size; i++) {
		push(&input_generics, clone_base_type(base_generics.data[i]));
	}

	if(try(parser->tokenizer, '<', 0)) {
		NodeList type_arguments = collect_until(parser, &expression,
				',', '>');
		for(size_t i = 0; i < type_arguments.size; i++) {
			if(!(type_arguments.data[i]->flags & fType)) {
				push(parser->tokenizer->messages, Err(
							type_arguments.data[i]->trace,
							str("expected a type in type arguments")));
			}

			if(i >= base_generics.size) {
				push(parser->tokenizer->messages, Err(
							stretch(type_arguments.data[i]->trace,
								last(type_arguments)->trace),
							str("too many type arguments")));
				push(parser->tokenizer->messages,
						see_declaration(declaration,
							type_arguments.data[i]));
				break;
			}

			clash_types(input_generics.data[i],
					(void*) type_arguments.data[i],
					type_arguments.data[i]->trace,
					parser->tokenizer->messages);
		}
	}

	variable->generics = input_generics;
	variable->type = wrap_applied_generics(variable->type,
			input_generics, declaration);
	push(&declaration->generics.variants, input_generics);
}

typedef Vector(Declaration**) DeclarationSetters;

typedef struct {
	TypeList base_generics;
	DeclarationSetters declaration_setters;
} GenericsCollection;

GenericsCollection collect_generics(Parser* parser) {
	if(!try(parser->tokenizer, '<', 0))
		return (GenericsCollection) { 0 };

	TypeList base_generics = { 0 };
	DeclarationSetters declaration_setters = { 0 };

	while(parser->tokenizer->current.type
			&& parser->tokenizer->current.type != '>') {
		Token identifier = expect(parser->tokenizer, TokenIdentifier);

		GenericType* generic_type = (void*) new_type((Type) {
				.GenericType = {
					.compiler = (void*) &comp_GenericType,
					.flags = fConstExpr,
					.trace = identifier.trace,
					.index = base_generics.size,
				}
		});
		push(&declaration_setters, &generic_type->declaration);

		push(&base_generics, new_type((Type) { .Auto = {
					.compiler = (void*) &comp_Auto,
					.flags = fConstExpr,
					.trace = identifier.trace,
		}}));

		put(last(parser->stack), identifier.trace.slice,
				(void*) new_node((Node) { .VariableDeclaration = {
					.compiler = (void*) &comp_VariableDeclaration,
					.trace = identifier.trace,
					.flags = fConst | fType,
					.type = (void*) generic_type,
					.const_value = (void*) generic_type,
				}}));

		if(!try(parser->tokenizer, ',', 0)) break;
	}
	expect(parser->tokenizer, '>');

	return (GenericsCollection) {
		.base_generics = base_generics,
		.declaration_setters = declaration_setters,
	};
}

void apply_generics(Declaration* declaration,
		GenericsCollection collection) {
	if(!collection.base_generics.size) return;

	push(&declaration->generics.stack, collection.base_generics);
	for(size_t i = 0; i < collection.declaration_setters.size; i++) {
		*collection.declaration_setters.data[i] = declaration;
	}
}

void append_generics_identifier(str* string, TypeList generics) {
	if(!generics.size) return;
	
	for(size_t i = 0; i < generics.size; i++) {
		strf(string, "__");
		stringify_type(generics.data[i], string, 1 << 0);
	}
}

strs filter_unique_generics_variants(TypeLists variants, str base) {
	strs identifiers = { 0 };
	Scope variants_set = { 0 };
	init_scope(&variants_set);

	for(size_t i = 0; i < variants.size; i++) {
		str identifier = strf(0, "%.*s", (int) base.size, base.data);
		append_generics_identifier(&identifier, variants.data[i]);

		if(get(variants_set, (Trace) { .slice = identifier })) {
			variants.data[i].size = 0;
		} else {
			put(&variants_set, identifier, 0);
		}

		push(&identifiers, identifier);
	}

	for(size_t i = 0; i < variants_set.cap; i++) {
		free(variants_set.data[i].data);
	}
	free(variants_set.data);

	return identifiers;
}
