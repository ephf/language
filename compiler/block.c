#include "right.c"

void comp_Statement(Statement* self, str* line, Compiler* compiler) {
	str expression_line = new_line(compiler);
	self->expression->compiler(self->expression, &expression_line,
			compiler);
	strf(&expression_line, ";");
	push(&compiler->sections.data[compiler->open_section].lines,
			expression_line);
}

void comp_ReturnStatement(ReturnStatement* self, str* line,
		Compiler* compiler) {
	str statement_line = new_line(compiler);
	strf(&statement_line, "return ");
	self->value->compiler((void*) self->value, &statement_line,
			compiler);
	push(&compiler->sections.data[compiler->open_section].lines,
			strf(&statement_line, ";"));
}

void comp_StructType(StructType* self, str* line, Compiler* compiler) {
	strf(line, "struct ");
	self->identifier->compiler((void*) self->identifier, line,
			compiler);
}

void comp_structLiteral(StructLiteral* self, str* line,
		Compiler* compiler) {
	strf(line, "(");
	self->type->compiler((void*) self->type, line, compiler);
	strf(line, ") {");

	for(size_t i = 0; i < self->fields.size; i++) {
		strf(line, i ? ", " : " ");
		
		if(self->field_names.data[i].size) {
			strf(line, ".%.*s = ",
					(int) self->field_names.data[i].size,
					self->field_names.data[i].data);
		}

		self->fields.data[i]->compiler(self->fields.data[i], line,
				compiler);
	}
	strf(line, " }");
}
