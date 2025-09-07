#include "left.c"

void comp_VariableDeclaration(VariableDeclaration* self, str* line,
		Compiler* compiler) {
	str decl_line = new_line(compiler);
	if(!self->is_inline) line = &decl_line;

	self->type->compiler((void*) self->type, line, compiler);
	strf(line, " ");
	if(self->type->flags & fConst) strf(line, "const ");
	self->identifier->compiler((void*) self->identifier, line,
			compiler);

	if(!self->is_inline) {
		if(self->const_value) {
			strf(line, " = ");
			self->const_value->compiler(self->const_value, line,
					compiler);
		} else if(self->type->flags & fConst) {
			push(compiler->messages, Err(self->trace,
						str("expected declaration with "
							"'\33[35mconst\33[0m' type to have a "
							"value")));
		}

		strf(line, ";");
		push(&compiler->sections.data[compiler->open_section].lines,
				decl_line);
	}
}

void comp_BinaryOperation(BinaryOperation* self, str* line,
		Compiler* compiler) {
	strf(line, "(");
	self->left->compiler(self->left, line, compiler);
	strf(line, " %.*s ", (int) self->operator.size, self->operator.data);
	self->right->compiler(self->right, line, compiler);
	strf(line, ")");
}

void dual_function_compiler(FunctionDeclaration* self,
		Compiler* compiler, str identifier, int hoisted) {
	const size_t previous_section = compiler->open_section;
	size_t section = compiler->open_section = compiler->sections.size;
	push(&compiler->sections, (CompilerSection) { 0 });

	str declaration_line = new_line(compiler);

	Type* const return_type =
		self->type->FunctionType.signature.data[0];
	return_type->compiler((void*) return_type, &declaration_line,
			compiler);

	strf(&declaration_line, " %.*s(",
			(int) identifier.size, identifier.data);
	for(size_t i = 0; i < self->arguments.size; i++) {
		if(i) strf(&declaration_line, ", ");
		VariableDeclaration* const argument = self->arguments.data[i];

		if(hoisted) {
			argument->type->compiler((void*) argument->type,
					&declaration_line, compiler);
		} else {
			argument->compiler((void*) argument, &declaration_line,
					compiler);
		}
	}
	strf(&declaration_line, hoisted ? ");" : ") {");
	push(&compiler->sections.data[section * !hoisted].lines,
			declaration_line);

	if(hoisted) {
		compiler->open_section = previous_section;
		return;
	}

	self->body->compiler((void*) self->body, &declaration_line,
			compiler);

	str terminator_line = new_line(compiler);
	push(&compiler->sections.data[section].lines,
			strf(&terminator_line, "}"));
	compiler->open_section = previous_section;
}

void comp_FunctionDeclaration(FunctionDeclaration* self, str* line,
		Compiler* compiler) {
	if(self->flags & fExternal) return;

	if(self->generics.stack.size == 1) {
		str base_identifier = { 0 };
		self->identifier->compiler((void*) self->identifier,
				&base_identifier, compiler);
		strs identifiers = filter_unique_generics_variants(
				self->generics.variants, base_identifier);

		for(size_t i = 0; i < self->generics.variants.size; i++) {
			if(!self->generics.variants.data[i].size) continue;

			push(&self->generics.stack,
					self->generics.variants.data[i]);
			dual_function_compiler(self, compiler, identifiers.data[i],
					1);
			dual_function_compiler(self, compiler, identifiers.data[i],
					0);
			self->generics.stack.size--;
		}

		free(base_identifier.data);
		return;
	}

	str identifier = { 0 };
	self->identifier->compiler((void*) self->identifier, &identifier,
			compiler);
	dual_function_compiler(self, compiler, identifier, 1);
	dual_function_compiler(self, compiler, identifier, 0);
	free(identifier.data);
}

void function_type_typedef(FunctionType* self, Compiler* compiler,
		str identifier) {
	str typedef_line = strf(0, "typedef ");
	Type* const return_type = self->signature.data[0];
	return_type->compiler((void*) return_type, &typedef_line,
			compiler);

	strf(&typedef_line, "(*%.*s)(", (int) identifier.size,
			identifier.data);
	for(size_t i = 1; i < self->signature.size; i++) {
		if(i > 1) strf(&typedef_line, ", ");
		self->signature.data[i]->compiler(
				(void*) self->signature.data[i], &typedef_line,
				compiler);
	}
	strf(&typedef_line, ");");

	push(&compiler->sections.data[0].lines, typedef_line);
}

ssize_t global_function_typedef_id = 0;

void comp_FunctionType(FunctionType* self, str* line,
		Compiler* compiler) {
	if(!self->typedef_id) {
		const ssize_t typedef_id = ++global_function_typedef_id;
		self->typedef_id = -1;

		str base_identifier = strf(0, "__Function%zd", typedef_id);

		if(self->declaration->generics.stack.size) {
			Generics* generics = &self->declaration->generics;
			strs identifiers = filter_unique_generics_variants(
					generics->variants, base_identifier);

			for(size_t i = 0; i < generics->variants.size; i++) {
				if(!generics->variants.data[i].size) continue;

				push(&generics->stack, generics->variants.data[i]);
				function_type_typedef(self, compiler,
						identifiers.data[i]);

				generics->stack.size--;
			}

			free(base_identifier.data);
		}

		self->typedef_id = typedef_id;
	}

	if(self->typedef_id < 0) {
		strf(line, "/* circular */ void*");
		return;
	}

	strf(line, "__Function%zd", self->typedef_id);
	if(self->declaration->generics.stack.size) {
		append_generics_identifier(line,
				last(self->declaration->generics.stack));
	}
}

void comp_FunctionCall(FunctionCall* self, str* line,
		Compiler* compiler) {
	self->function->compiler(self->function, line, compiler);
	strf(line, "(");
	for(size_t i = 0; i < self->arguments.size; i++) {
		if(i) strf(line, ", ");
		self->arguments.data[i]->compiler(self->arguments.data[i],
				line, compiler);
	}
	strf(line, ")");
}

void comp_PointerType(PointerType* self, str* line,
		Compiler* compiler) {
	self->base->compiler((void*) self->base, line, compiler);
	strf(line, "*");
}
