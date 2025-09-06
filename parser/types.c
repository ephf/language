#include "identifier.c"

enum {
	StateActionGenerics,
};

typedef struct {
	unsigned action;
	Node* target;
} TypeStateAction;
typedef Vector(TypeStateAction) TypeStateActions;

typedef struct {
	TypeStateActions actions;
	Type* open_type;
} OpenedType;

Type* open_type_interm(Type* type, TypeStateActions* actions,
		typeof(type->compiler) stop_at) {
	if(type->compiler == stop_at) return type;

	if(type->compiler == (void*) &comp_Auto && type->Auto.ref) {
		if(type->Auto.generics_declaration) {
			push(&type->Auto.generics_declaration->generics.stack,
					type->Auto.generics);
			push(actions, ((TypeStateAction) {
					.action = StateActionGenerics,
					.target = (void*) type->Auto.generics_declaration,
			}));
		}

		return open_type_interm(type->Auto.ref, actions, stop_at);
	}

	if(type->compiler == (void*) &comp_Variable
			&& type->Variable.declaration->const_value) {
		if(type->Variable.generics.size) {
			push(&type->Variable.declaration->generics.stack,
					type->Variable.generics);
			push(actions, ((TypeStateAction) {
					.action = StateActionGenerics,
					.target = (void*) type->Auto.generics_declaration,
			}));
		}

		return open_type_interm(
				(void*) type->Variable.declaration->const_value,
				actions, stop_at);
	}

	if(type->compiler == (void*) &comp_GenericType) {
		return open_type_interm(
				last(type->GenericType.declaration->generics.stack)
					.data[type->GenericType.index], actions, stop_at);
	}

	return type;
}

OpenedType open_type(Type* type) {
	OpenedType opened_type = { 0 };
	opened_type.open_type =
		open_type_interm(type, &opened_type.actions, 0);
	return opened_type;
}

void close_type(TypeStateActions actions) {
	for(size_t i = actions.size; i > 0; i--) {
		switch(actions.data[i - 1].action) {
			case StateActionGenerics:
				actions.data[i - 1].target->Declaration.generics
					.stack.size--;
		}
	}

	free(actions.data);
}

enum {
	StringifyAlphaNum,
};

void stringify_type(Type* type, str* string, unsigned flags) {
	const OpenedType opened_type = open_type(type);
	Type* const open = opened_type.open_type;

	if(open->compiler == (void*) &comp_Auto) {
		strf(string, "auto");
	}

	else if(open->compiler == (void*) &comp_External) {
		strf(string, "%.*s",
				(int) open->External.data.size,
				open->External.data.data);
	}

	else if(open->compiler == (void*) &comp_PointerType) {
		stringify_type(open->PointerType.base, string, flags);
		if(flags & StringifyAlphaNum) strf(string, "_ptr");
		else strf(string, "*");
	}

	else {
		strf(string, "[unknown]");
	}

	close_type(opened_type.actions);
}

Type* clone_base_type(Type* type) {
	const OpenedType opened = open_type(type);
	Type* const open = opened.open_type;

	Type* cloned = (void*) new_node(*(Node*)(void*) open);
	close_type(opened.actions);
	return cloned;
}

Type* make_type_standalone(Type* type) {
	// TODO: traverse_type() function that traverses a types children
	// (e.g. traversing pointers, function arguments, and generics)
	TypeStateActions actions = { 0 };
	Type* open;

	while((open = open_type_interm(type, &actions,
					(void*) &comp_GenericType))
			->compiler == (void*) &comp_GenericType) {
		Declaration* declaration = open->GenericType.declaration;

		for(size_t i = declaration->generics.stack.size; i > 1; i--) {
			type = new_type((Type) { .Auto = {
					.compiler = (void*) &comp_Auto,
					.trace = type->trace,
					.flags = type->flags,
					.generics = declaration->generics
						.stack.data[i - 1],
					.generics_declaration = declaration,
					.ref = type,
			}});
		}
	}

	close_type(actions);
	return type;
}

int try_assign_auto_type(Type* open_a, Type* open_b) {
	if(open_a->compiler == (void*) &comp_Auto) {
		if(open_a == open_b) return 1;
		if(open_a->flags & tfNumeric && !(open_b->flags & tfNumeric))
			return 0;
		open_a->Auto.ref = make_type_standalone(open_b);
		return 1;
	}

	return 0;
}

int test_types(Type* a, Type* b, Trace trace, Messages* messages) {
	const OpenedType opened_a = open_type(a);
	const OpenedType opened_b = open_type(b);
	Type* const open_a = opened_a.open_type;
	Type* const open_b = opened_b.open_type;

	if(open_a->compiler == (void*) &comp_PointerType
			&& open_b->compiler == (void*) &comp_PointerType) {
		if((open_a->PointerType.base->flags
					^ open_b->PointerType.base->flags) & fConst) {
			push(messages, Warn(trace, str("comparing pointer to const "
							"with pointer to non-const")));
		}

		const int test_result = test_types(
				a->PointerType.base, b->PointerType.base,
				trace, messages);
		close_type(opened_a.actions);
		close_type(opened_b.actions);

		if(test_result) return test_result;
		goto partial_checks;
	}

	close_type(opened_a.actions);
	close_type(opened_b.actions);

	if(try_assign_auto_type(open_b, open_a)
			|| try_assign_auto_type(open_a, open_b)) return 1;

	else if(open_a->type == (void*) &comp_External) {
		if(open_b->type != (void*) &comp_External
				|| !streq(open_a->External.data,
					open_b->External.data))
			goto partial_checks;
		return 1;
	}

partial_checks:
	if(open_a->flags & open_b->flags & tfNumeric) return 2;
	return 0;
}

void clash_types(Type* a, Type* b, Trace trace, Messages* messages) {
	if(!test_types(a, b, trace, messages)) {
		str string_a = { 0 }, string_b = { 0 };
		stringify_type(a, &string_a, 0);
		stringify_type(b, &string_b, 0);

		push(messages, Err(trace,
					strf(0, "type mismatch between "
						"'\33[35m%.*s\33[0m' and '\33[35m%.*s\33[0m'",
						(int) string_a.size, string_a.data,
						(int) string_b.size, string_b.data)));
	}
}
