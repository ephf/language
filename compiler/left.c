#include "compiler.c"

void comp_NumericLiteral(NumericLiteral* self, str* line,
		Compiler* compiler) {
	strf(line, "%ld", self->number);
}

void comp_Auto(Auto* self, str* line, Compiler* compiler) {
	if(self->ref) {
		self->ref->compiler((void*) self->ref, line, compiler);
		return;
	}

	strf(line, "/* auto */ int");
}

void comp_Identifier(Identifier* self, str* line, Compiler* compiler) {
	strf(line, "%.*s", (int) self->base.size, self->base.data);
}

void comp_Scope(Scope* self, str* line, Compiler* compiler) {
	CompilerSection* const section = compiler->sections.data
		+ compiler->open_section;
	strf(&section->indent, "    ");

	for(size_t i = 0; i < self->declarations.size; i++) {
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
