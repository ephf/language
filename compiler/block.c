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
