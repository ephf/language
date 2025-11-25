#include "identifier.c"

typedef struct {
	TypeStateActions actions;
	Type* type;
} OpenedType;

extern int in_compiler_step;
extern Compiler* generics_compiler_context;
TypeStateActions global_state_actions = { 0 };

enum { ActionKeepGlobalState = 1 << 0 };

void type_state_action(TypeStateAction action, unsigned flags) {
	if(!(flags & ActionKeepGlobalState)) {
		push(&global_state_actions, action);
	}

	switch(action.type) {
		case StateActionGenerics:
			push(&action.target->Declaration.generics.stack, action.TypeList);
			if(in_compiler_step) {
				action.target->compiler(action.target, NULL, generics_compiler_context);
			}
			break;

		case StateActionCollection:
			for(size_t i = 0; i < action.TypeStateActions.size; i++) {
				type_state_action(action.TypeStateActions.data[i],
						flags | ActionKeepGlobalState);
			}
			break;
	}
}

void undo_type_state_action(TypeStateAction action, unsigned flags) {
	if(!(flags & ActionKeepGlobalState)) {
		global_state_actions.size--;
	}

	switch(action.type) {
		case StateActionGenerics:
			action.target->Declaration.generics.stack.size--;
			break;

		case StateActionCollection:
			for(size_t i = action.TypeStateActions.size; i > 0; i--) {
				undo_type_state_action(action.TypeStateActions.data[i - 1],
						flags | ActionKeepGlobalState);
			}
			break;
	}
}

Type* peek_type(Type* type, TypeStateAction* action, unsigned flags) {
	if(type->compiler == (void*) &comp_Wrapper && type->Wrapper.ref) {
		if(type->Wrapper.action.type) {
			type_state_action(type->Wrapper.action, flags);
			*action = type->Wrapper.action;
		}

		return type->Wrapper.variable
			? type->Wrapper.ref->Declaration.const_value
			: (void*) type->Wrapper.ref;
	}

	if(type->compiler == (void*) &comp_GenericType) {
		return last(type->GenericType.declaration->generics.stack)
			.data[type->GenericType.index];
	}

	return NULL;
}

OpenedType open_type_wa(Type* type, Type* follower, int (*acceptor)(Type*, Type*, void*),
		void* accumulator) {
	OpenedType opened_type = { 0 };
	if(!type) return opened_type;
	TypeStateAction action = { 0 };

	while((opened_type.type = peek_type(type, &action, 0)) != type) {
		type = opened_type.type;
		if(acceptor) acceptor(type, follower, accumulator);

		if(action.type) {
			push(&opened_type.actions, action);
			action.type = 0;
		}
	}

	return opened_type;
}

#define open_type(type) open_type_wa(type, NULL, NULL, NULL);

void close_type(TypeStateActions actions, unsigned flags) {
	for(size_t i = actions.size; i > 0; i--) {
		undo_type_state_action(actions.data[i - 1], flags);
	}
	free(actions.data);
}

Type* make_type_standalone(Type* type) {
	return global_state_actions.size
		? new_type((Type) { .Wrapper = {
				.compiler = (void*) &comp_Wrapper,
				.flags = type->flags,
				.trace = type->trace,
				.action = { StateActionCollection, .TypeStateActions = global_state_actions },
				.ref = (void*) type,
		}}) : type;
}

enum {
	TraverseIntermeditate 	= 1 << 0, 
	TraverseGenerics 		= 1 << 1,
};

int traverse_type(Type*, Type*, int (*)(Type*, Type*, void*), void*, unsigned);

static inline int traverse_generics(Declaration* declaration,
		int (*acceptor)(Type*, Type*, void*), void* accumulator, unsigned flags) {
	if(!(flags & TraverseGenerics) || declaration->generics.stack.size <= 1) return 0;
	
	const TypeList generics = last(declaration->generics.stack);
	for(size_t i = 0; i < generics.size; i++) {
		const int result = traverse_type(generics.data[i], NULL, acceptor, accumulator,
				flags);
		if(result) return result;
	}

	return 0;
}

int traverse_type(Type* type, Type* follower, int (*acceptor)(Type*, Type*, void*),
		void* accumulator, unsigned flags) {
	const OpenedType ofollower = open_type(follower);
	const OpenedType otype = open_type_wa(type, follower, (flags & TraverseIntermeditate)
			? acceptor : 0, accumulator);

	if(!(flags & TraverseIntermeditate)) {
		int result = acceptor(type, follower, accumulator);
		if(result) return result - 1;
	}

	if(follower && otype.type->compiler != ofollower.type->compiler) return 1;

	int result = 0;
	if(otype.type->compiler == (void*) &comp_PointerType) {
		result = traverse_type(otype.type->PointerType.base, ofollower.type->PointerType.base,
				acceptor, accumulator, flags);
	} else if(otype.type->compiler == (void*) &comp_StructType) {
		result = traverse_generics((void*) otype.type, acceptor, accumulator, flags);
		if(ofollower.type->StructType.fields.size != otype.type->StructType.fields.size) {
			result = 1;
		}

		for(size_t i = 0; !result && i < otype.type->StructType.fields.size; i++) {
			result = traverse_type(otype.type->StructType.fields.data[i]->type,
					ofollower.type->StructType.fields.data[i]->type, acceptor,
					accumulator, flags);
		}
	} else if(otype.type->compiler == (void*) &comp_FunctionType) {
		result = traverse_generics((void*) otype.type->FunctionType.declaration,
				acceptor, accumulator, flags);
	}

	close_type(otype.actions, 0);
	close_type(ofollower.actions, 0);
	return 0;
}

enum { StringifyAlphaNum = 1 << 0 };

typedef struct {
	str* string;
	unsigned flags;
} StringifyAccumulator;

int stringify_acceptor(Type* type, Type* _, StringifyAccumulator* accumulator) {
	if(type->compiler == (void*) &comp_Wrapper) {
		strf(accumulator->string, type->flags & tfNumeric
				? accumulator->flags & StringifyAlphaNum ? "number" : "~number"
				: "auto");
	} else if(type->compiler == (void*) &comp_External) {
		strf(accumulator->string, "%.*s", (int) type->External.data.size,
				type->External.data.data);
	} else if(type->compiler == (void*) &comp_PointerType) {
		strf(accumulator->string, accumulator->flags & StringifyAlphaNum
				? "ptrto_" : "&");
	} else if(type->compiler == (void*) &comp_StructType) {
		strf(accumulator->string, accumulator->flags & StringifyAlphaNum
				? "struct_%.*s" : "struct %.*s",
				(int) type->StructType.identifier->base.size,
				type->StructType.identifier->base.size);
	} else {
		strf(accumulator->string, accumulator->flags & StringifyAlphaNum
				? "UNKNOWN" : "~unknown");
	}
	return 0;
}

void stringify_type(Type* type, str* string, unsigned flags) {
	traverse_type(type, NULL, (void*) &stringify_acceptor,
			&(StringifyAccumulator) { string, flags }, 0);
}

enum {
	TestMismatch = 2,
	TestCircular,

	ClashPassive = 1 << 0,
};

typedef struct {
	Trace trace;
	Messages* messages;
	unsigned flags;
} ClashAccumulator;

int circular_acceptor(Type* type, Type* _, Type* compare) {
	return 2 * (type == compare);
}

int assign_wrapper(Wrapper* wrapper, Type* follower, ClashAccumulator* accumulator) {
	if(wrapper->flags & tfNumeric && !(follower->flags & tfNumeric)) return TestMismatch;

	if(
			traverse_type((void*) wrapper, NULL, (void*) &circular_acceptor, follower,
				TraverseGenerics & TraverseIntermeditate) ||
			traverse_type((void*) follower, NULL, (void*) &circular_acceptor, wrapper,
				TraverseGenerics & TraverseIntermeditate)
	  ) return TestCircular;

	if(wrapper->compare) {
		int result = clash_types(wrapper->compare, follower, accumulator->trace,
				accumulator->messages, ClashPassive | accumulator->flags);
		if(result) return result + 1;
	}

	if(!(accumulator->flags & ClashPassive)) {
		Type* const standalone = make_type_standalone(follower);

		if(wrapper->assign_compare) wrapper->compare = standalone;
		else wrapper->ref = (void*) standalone;

		wrapper->flags = follower->flags;

		if(wrapper->compare && follower->compiler == (void*) &comp_Wrapper) {
			follower->Wrapper.compare = wrapper->compare;
		}
	}
	return 1;
}

int clash_acceptor(Type* type, Type* follower, ClashAccumulator* accumulator) {
	if(type->compiler == (void*) &comp_Wrapper) {
		return assign_wrapper((void*) type, follower, accumulator);
	} else if(follower->compiler == (void*) &comp_Wrapper) {
		return assign_wrapper((void*) follower, type, accumulator);
	} else if(type->compiler != follower->compiler);

	else if(type->compiler == (void*) &comp_External) {
		if(streq(type->External.data, follower->External.data)) return 1;
	} else return 0;

	if(type->flags & follower->flags & tfNumeric) return 1;
	return TestMismatch;
}

int clash_types(Type* a, Type* b, Trace trace, Messages* messages, unsigned flags) {
	ClashAccumulator accumulator = { trace, messages, flags };
	int result = traverse_type(a, b, (void*) &clash_acceptor, &accumulator, 0);

	if(result && !(flags & ClashPassive)) {
		str message = strf(0, "type mismatch between '\33[35m");
		stringify_type(a, &message, 0);

		strf(&message, "\33[0m' and '\33[35m");
		stringify_type(b, &message, 0);

		strf(&message, result + 1 == TestCircular
				? "\33[0m' (types are circularly referencing eachother)"
				: "\33[0m'");

		push(messages, Err(trace, message));
	}

	return result;
}
