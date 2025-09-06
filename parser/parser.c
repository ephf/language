#include "nodes.c"
#include <stdint.h>

typedef struct {
	Tokenizer* tokenizer;
	Stack stack;
} Parser;

uint32_t fnv1a_u32_hash(str string) {
	uint32_t hash = 2166136261u;
	while(string.size--) hash = (hash ^ *string.data++) * 16777619;
	return hash;
}

Scope* new_scope(Declaration* parent) {
	Scope scope = {
		.compiler = (void*) &comp_Scope,
		.parent = parent,
	};
	
	resv(&scope, 16);
	memset(scope.data, 0, sizeof(ScopeEntries) * 16);

	Scope* boxed_scope = (void*) new_node((Node) { .Scope = scope });
	if(!boxed_scope->parent) boxed_scope->parent = (void*) boxed_scope;
	return boxed_scope;
}

void put(Scope* scope, str identifier, Declaration* declaration) {
	ScopeEntries* entries = scope->data
		+ fnv1a_u32_hash(identifier) % scope->cap;
	push(entries, ((ScopeEntry) { identifier, declaration }));
}

Variable* variable_of(Declaration* declaration, Trace trace,
		unsigned long flags) {
	flags |= fConstExpr | fMutable | declaration->flags & fType;

	return (void*) new_node((Node) { .Variable = {
			.compiler = (void*) &comp_Variable,
			.flags = flags,
			.trace = trace,
			.type = declaration->type,
			.declaration = declaration,
	}});
}

Variable* get(Scope scope, Trace identifier) {
	if(!scope.data) return 0;
	ScopeEntries entries = scope.data[
		fnv1a_u32_hash(identifier.slice) % scope.cap];
	for(size_t i = 0; i < entries.size; i++) {
		if(streq(identifier.slice, entries.data[i].identifier)) {
			return (void*) variable_of(entries.data[i].declaration,
					identifier, 0);
		}
	}
	return 0;
}

Variable* find(Stack stack, Trace identifier) {
	for(size_t i = stack.size; i > 0; i--) {
		Variable* possible_found = get(*stack.data[i - 1], identifier);
		if(possible_found) return possible_found;
	}
	return 0;
}
