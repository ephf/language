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

Type* open_type_interm(Type* type, TypeStateActions* actions, typeof(type->compiler) stop_at) {
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
	StringifyAlphaNum = 1 << 0,
};

void stringify_type(Type* type, str* string, unsigned flags) {
	const OpenedType opened_type = open_type(type);
	Type* const open = opened_type.open_type;

	if(open->compiler == (void*) &comp_Auto) {
		if(open->flags & tfNumeric) {
			if(flags & StringifyAlphaNum) strf(string, "number");
			else strf(string, "~number");
		} else strf(string, "auto");
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

	else if(open->compiler == (void*) &comp_StructType) {
		strf(string, flags & StringifyAlphaNum
				? "struct_%.*s" : "struct %.*s",
				(int) open->StructType.identifier->base.size,
				open->StructType.identifier->base.data);
	}

	else {
		strf(string, "~unknown");
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

enum {
	SiftGenerics = 1 << 0,
};

void sift_type(Type* type, void (*acceptor)(Type*, void*), void* accumulator,
		unsigned flags);

static inline void sift_generics(Declaration* declaration, void (*acceptor)(Type*, void*),
		void* accumulator, unsigned flags) {
	if(declaration->generics.stack.size > 1) {
		for(size_t i = 0; i < last(declaration->generics.stack).size; i++) {
			sift_type(last(declaration->generics.stack).data[i], acceptor, accumulator, 
					flags);
		}
	}
}

void sift_type(Type* type, void (*acceptor)(Type*, void*), void* accumulator,
		unsigned flags) {
	const OpenedType opened = open_type(type);
	Type* const open = opened.open_type;
	acceptor(open, accumulator);

	if(open->compiler == (void*) &comp_PointerType) {
		sift_type(open->PointerType.base, acceptor, accumulator, flags);
	}

	else if(open->compiler == (void*) &comp_FunctionType) {
		sift_generics((void*) open->FunctionType.declaration, acceptor, accumulator, flags);
	}

	else if(open->compiler == (void*) &comp_StructType) {
		sift_generics((void*) open, acceptor, accumulator, flags);
		for(size_t i = 0; i < open->StructType.fields.size; i++) {
			sift_type(open->StructType.fields.data[i]->type, acceptor, accumulator, flags);
		}
	}

	close_type(opened.actions);
}

typedef struct {
	TypeStateActions* actions;
	Type** wrapper;
} StandaloneAccumulator;

void standalone_acceptor(Type* type, StandaloneAccumulator* accumulator) {
	while((type = open_type_interm(type, accumulator->actions, (void*) &comp_GenericType))
			->compiler == (void*) &comp_GenericType) {
		Declaration* const declaration = type->GenericType.declaration;

		for(size_t i = 2; i < declaration->generics.stack.size; i++) {
			*accumulator->wrapper = wrap_applied_generics(*accumulator->wrapper,
					declaration->generics.stack.data[i], declaration);
		}

		type = last(type->GenericType.declaration->generics.stack)
			.data[type->GenericType.index];
	}
}

Type* make_type_standalone(Type* type) {
	TypeStateActions actions = { 0 };
	StandaloneAccumulator accumulator = { &actions, &type };
	sift_type(type, (void*) &standalone_acceptor, &accumulator, 0);
	return type;
}

typedef struct {
	int circular;
	Type* compare;
} CircularAccumulator;

void circular_acceptor(Type* type, CircularAccumulator* accumulator) {
	if(type == accumulator->compare) accumulator->circular = 1;
}

int try_assign_auto_type(Type* open_a, Type* open_b) {
	if(open_a->compiler == (void*) &comp_Auto) {
		if(open_a == open_b) return 1;

		CircularAccumulator accumulator = { 0, open_b };
		sift_type(open_a, (void*) &circular_acceptor, &accumulator, 0);
		accumulator.compare = open_a;
		sift_type(open_b, (void*) &circular_acceptor, &accumulator, 0);
		if(accumulator.circular) return -1;

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
				open_a->PointerType.base, open_b->PointerType.base,
				trace, messages);
		close_type(opened_a.actions);
		close_type(opened_b.actions);

		if(test_result) return test_result;
		goto partial_checks;
	}

	int result;
	if((result = try_assign_auto_type(open_b, open_a))
			|| (result = try_assign_auto_type(open_a, open_b))) {
		close_type(opened_a.actions);
		close_type(opened_b.actions);
		return result;
	}

	close_type(opened_a.actions);
	close_type(opened_b.actions);

	if(open_a->type == (void*) &comp_External) {
		if(open_b->type != (void*) &comp_External
				|| !streq(open_a->External.data,
					open_b->External.data))
			goto partial_checks;
		return 1;
	}

	if(open_a->compiler == (void*) &comp_StructType
			&& open_b->compiler == (void*) &comp_StructType) {
		return streq(open_a->StructType.identifier->base,
				open_b->StructType.identifier->base);
	}

partial_checks:
	if(open_a->flags & open_b->flags & tfNumeric) return 2;
	return 0;
}

void clash_types(Type* a, Type* b, Trace trace, Messages* messages) {
	int result = test_types(a, b, trace, messages);
	if(result <= 0) {
		str string_a = { 0 }, string_b = { 0 };
		stringify_type(a, &string_a, 0);
		stringify_type(b, &string_b, 0);

		str message = strf(0, "type mismatch between "
				"'\33[35m%.*s\33[0m' and '\33[35m%.*s\33[0m'",
				(int) string_a.size, string_a.data,
				(int) string_b.size, string_b.data);
		if(result == -1) {
			strf(&message, " (types are circularly referencing "
					"eachother)");
		}

		push(messages, Err(trace, message));
	}
}
