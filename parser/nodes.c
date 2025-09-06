#include "../tokenizer.c"

typedef struct Compiler Compiler;

typedef union Node Node;
typedef Vector(Node*) NodeList;
typedef Vector(Node) Nodes;

typedef union Type Type;
typedef Vector(Type*) TypeList;
typedef Vector(TypeList) TypeLists;

typedef union Declaration Declaration;
typedef Vector(Declaration*) DeclarationList;

#define extends_Node \
	void (*compiler)(Node*, str*, Compiler*); \
	Trace trace; \
	Type* type; \
	unsigned long flags

#define extends_Type \
	extends_Node
typedef struct {
	str identifier;
	Declaration* declaration;
} ScopeEntry;
typedef Vector(ScopeEntry) ScopeEntries;

typedef struct {
	extends_Node;
	Vector(ScopeEntries);
	DeclarationList declarations;
	NodeList children;
	Declaration* parent;
} Scope;
typedef Vector(Scope*) Stack;
void comp_Scope(Scope*, str*, Compiler*);

typedef struct {
	Scope* variant_set;
	TypeLists variants;
	TypeLists stack;
} Generics;

#define extends_Declaration \
	extends_Node; \
	Identifier* identifier; \
	Node* const_value; \
	Generics generics;

typedef struct {
	extends_Node;
	long number;
} NumericLiteral;
void comp_NumericLiteral(NumericLiteral*, str*, Compiler*);

typedef struct {
	extends_Type;
	Type* ref;
	TypeList generics;
	Declaration* generics_declaration;
} Auto;
void comp_Auto(Auto*, str*, Compiler*);

typedef struct {
	extends_Node;
	str base;
	Declaration* parent;
	unsigned external : 1;
} Identifier;
void comp_Identifier(Identifier*, str*, Compiler*);

typedef struct {
	extends_Declaration;
	unsigned is_inline : 1,
			 inline_compiled : 1;
} VariableDeclaration;
typedef Vector(VariableDeclaration*) VariableDeclarationList;
void comp_VariableDeclaration(VariableDeclaration*, str*, Compiler*);


typedef struct {
	extends_Node;
} Missing;
void comp_Missing(Missing*, str*, Compiler*);

typedef struct {
	extends_Node;
	str data;
} External;
void comp_External(External*, str*, Compiler*);

typedef struct {
	extends_Node;
	Node* expression;
} Statement;
void comp_Statement(Statement*, str*, Compiler*);

typedef struct {
	extends_Node;
	Declaration* declaration;
	TypeList generics;
	unsigned observed : 1;
} Variable;
void comp_Variable(Variable*, str*, Compiler*);

typedef struct {
	extends_Node;
	Node* left;
	str operator;
	Node* right;
} BinaryOperation;
void comp_BinaryOperation(BinaryOperation*, str*, Compiler*);

typedef struct {
	extends_Declaration;
	VariableDeclarationList arguments;
	Scope* body;
} FunctionDeclaration;
void comp_FunctionDeclaration(FunctionDeclaration*, str*, Compiler*);

typedef struct {
	extends_Type;
	TypeList signature;
	FunctionDeclaration* declaration;
	ssize_t typedef_id;
} FunctionType;
void comp_FunctionType(FunctionType*, str*, Compiler*);

typedef struct {
	extends_Node;
	Node* value;
} ReturnStatement;
void comp_ReturnStatement(ReturnStatement*, str*, Compiler*);

typedef struct {
	extends_Node;
	Node* function;
	NodeList arguments;
} FunctionCall;
void comp_FunctionCall(FunctionCall*, str*, Compiler*);

typedef struct {
	extends_Type;
	Type* base;
} PointerType;
void comp_PointerType(PointerType*, str*, Compiler*);

typedef struct {
	extends_Type;
	Declaration* declaration;
	size_t index;
} GenericType;
void comp_GenericType(GenericType*, str*, Compiler*);

union Type {
	struct { extends_Type; };

	Auto Auto;
	External External;
	Variable Variable;
	GenericType GenericType;

	FunctionType FunctionType;
	PointerType PointerType;
};

union Declaration {
	struct { extends_Declaration; };

	VariableDeclaration VariableDeclaration;
	FunctionDeclaration FunctionDeclaration;
};

union Node {
	struct { extends_Node; };

	NumericLiteral NumericLiteral;
	Identifier Identifier;
	Scope Scope;
	Missing Missing;
	External External;
	Variable Variable;

	Type Type;
	Auto Auto;

	Declaration Declaration;
	VariableDeclaration VariableDeclaration;
	BinaryOperation BinaryOperation;
	FunctionDeclaration FunctionDeclaration;
	FunctionCall FunctionCall;

	Statement Statement;
	ReturnStatement ReturnStatement;
};

void comp_Ignore(Node* self, str* line, Compiler* compiler) {}

enum {
	fType = 1 << 0,
	fConst = 1 << 1,
	fConstExpr = 1 << 2,
	fMutable = 1 << 3,
	fIgnoreStatment = 1 << 4,
	fStatementTerminated = 1 << 5,
	fExternal = 1 << 6,

	fSize,
	afStart = fSize - 1,

	tfNumeric = afStart << 1,
};

typedef Vector(Nodes) NodeArena;

NodeArena global_node_arena = { 0 };
NodeList global_node_unused = { 0 };
__attribute__ ((constructor)) void init_node_arena() {
	push(&global_node_arena, (Nodes) { 0 });
	resv(global_node_arena.data, 1024);
}

Node* new_node(Node node) {
	if(global_node_unused.size) {
		Node* box = global_node_unused.data[--global_node_unused.size];
		*box = node;
		return box;
	}

	Nodes* vector = &last(global_node_arena);

	if(vector->cap == vector->size) {
		push(&global_node_arena, (Nodes) { 0 });
		resv(&last(global_node_arena), vector->cap * 2);
		vector = &last(global_node_arena);
	}

	Node* box = vector->data + vector->size;
	push(vector, node);
	return box;
}

void unbox(Node* box) {
	push(&global_node_unused, box);
}

Type* new_type(Type type) {
	type.flags |= fType;
	Type* box = (void*) new_node((Node) { .Type = type });
	box->type = box;
	return box;
}

