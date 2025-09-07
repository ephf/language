#include "compiler.c"

void comp_NumericLiteral(NumericLiteral* self, str* line,
		Compiler* compiler) {
	strf(line, "%ld", self->number);
}

void comp_Auto(Auto* self, str* line, Compiler* compiler) {
	const OpenedType opened = open_type((void*) self);
	
	if(opened.open_type->compiler == (void*) &comp_Auto) {
		strf(line, "/* auto */ int");

		if(!self->warned) {
			self->warned = 1;
			push(compiler->messages, Warn(self->trace,
						str("auto was never assigned a type (defaults "
							"to '\33[35mint\33[0m')")));
		}
	} else {
		opened.open_type->compiler((void*) opened.open_type, line,
				compiler);
	}

	close_type(opened.actions);
}

void comp_Identifier(Identifier* self, str* line, Compiler* compiler) {
	strf(line, "%.*s", (int) self->base.size, self->base.data);
}

void comp_Scope(Scope* self, str* line, Compiler* compiler) {
	CompilerSection* const section = compiler->sections.data
		+ compiler->open_section;
	strf(&section->indent, "    ");

	for(size_t i = 0; i < self->declarations.size; i++) {
		if(self->declarations.data[i]->is_inline) continue;
		self->declarations.data[i]->compiler(
				(void*) self->declarations.data[i], line, compiler);
	}
	for(size_t i = 0; i < self->children.size; i++) {
		self->children.data[i]->compiler(self->children.data[i],
				line, compiler);
	}

	compiler->sections.data[compiler->open_section].indent.size -= 4;
}

void comp_Missing(Missing* self, str* line, Compiler* compiler) {
	push(compiler->messages, Err(self->trace,
				strf(0, "cannot find '\33[35m%.*s\33[0m' in scope",
					(int) self->trace.slice.size,
					self->trace.slice.data)));
}

void comp_External(External* self, str* line, Compiler* compiler) {
	strf(line, "%.*s", (int) self->data.size, self->data.data);
}

void comp_Variable(Variable* self, str* line, Compiler* compiler) {
	if(self->declaration->const_value
			&& self->declaration->const_value->flags & fConstExpr) {
		self->declaration->const_value->compiler(
				self->declaration->const_value, line, compiler);
		return;
	}

	self->declaration->identifier->compiler(
			(void*) self->declaration->identifier, line, compiler);
}

void comp_GenericType(GenericType* self, str* line,
		Compiler* compiler) {
	Type* const base = last(self->declaration->generics.stack)
		.data[self->index];
	base->compiler((void*) base, line, compiler);
}
