#include "identifier.c"

typedef struct {
	unsigned action;
	Node* target;
} TypeStateAction;
typedef Vector(TypeStateAction) TypeStateActions;

typedef struct {
	TypeStateActions actions;
	Type* open_type;
} OpenedType;

Type* open_type_interm(Type* type, TypeStateActions* actions) {
	if(type->compiler == (void*) &comp_Auto && type->Auto.ref) {
		return open_type_interm(type->Auto.ref, actions);
	}

	if(type->compiler == (void*) &comp_Variable
			&& type->Variable.declaration->const_value) {
		return open_type_interm(
				(void*) type->Variable.declaration->const_value,
				actions);
	}

	return type;
}

OpenedType open_type(Type* type) {
	OpenedType opened_type = { 0 };
	opened_type.open_type =
		open_type_interm(type, &opened_type.actions);
	return opened_type;
}

void close_type(TypeStateActions actions) {
	for(size_t i = actions.size; i > 0; i--) {
		switch(actions.data[i - 1].action) {
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

int try_assign_auto_type(Type* open_a, Type* open_b, Type* b) {
	if(open_a->compiler == (void*) &comp_Auto) {
		if(open_a == open_b) return 1;
		if(open_a->flags & tfNumeric && !(open_b->flags & tfNumeric))
			return 0;
		open_a->Auto.ref = b;
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

	if(try_assign_auto_type(open_b, open_a, a)
			|| try_assign_auto_type(open_a, open_b, b)) return 1;

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
