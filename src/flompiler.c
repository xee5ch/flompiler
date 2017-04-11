#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define MAXVALS 8 //number of inputs/outputs per function max
#define WORDLEN 100 //largest word size
#define LINELEN 5000 //largest line size
#define MAXLINES 5000 //largest line size
#define MAXSCOPES 100 //largest line size
#define MAXWORDS 17 //most words per line

#define eprint(expr) fprintf(stderr, expr);
#define dprint(expr, f) printf(#expr " = '%" f "'\n", expr); //for debugging

struct func {
	char ins[MAXVALS][WORDLEN], outs[MAXVALS][WORDLEN], name[WORDLEN]; //inputs and outputs, name of function
	//terminators are if (ins[i][0]) (outs[i][0]) (name[0]) then end
	char satisfied; //mask for if the inputs are satisfied
};

struct scope {
	char name[WORDLEN];
	struct func f[MAXLINES];
	int subscopes;
};

//TODO: support multiple return values

//will run a function scope.f[i] and put code into p
void runfunc(char *program, struct scope scope, int i);

//will divide the chars of s into a new group every char in sep, and put the result in r.
void split(char **r, char *s, char sep) {
	char *next; //next found sep
	int i = 0, dist; //index of result
	while (*s && (next = strchr(s, sep))) {
		dist = next - s;
		if (dist) { //if you aren't on a sep
			strncpy(r[i], s, dist); //slice it and move up
			r[i++][dist] = '\0'; //put on terminator
		}
		s = 1 + next;
	}
	strcpy(r[i++], s); //put on the last string.
	strcpy(r[i], ""); //null terminator (points to the memory address of a baked-in "" string.
}

//will transfer code to a structure
void parse(struct scope *scopes, char *escaped, char *s) {
	//i = line index, j = word index, k = output index, f = normal line index, si = scope index
	int i, j, k, f = 0, si = 0;
	char **lines = malloc(sizeof(char *) * MAXLINES); //array of lines
	for (i = 0; i < MAXLINES; i++)
		lines[i] = malloc(LINELEN);
	char **words = malloc(sizeof(char *) * MAXWORDS); //array of words in current line
	for (i = 0; i < MAXWORDS; i++) {
		words[i] = malloc(WORDLEN);
	}

	split(lines, s, '\n');
	//iterate through lines
	for (i = 0; lines[i][0]; i++) {
		if (lines[i][0] == '#') {
			strcat(escaped, lines[i]);
			strcat(escaped, "\n");
			continue;
		}
		//split words[i] into lines
		split(words, lines[i], ' ');

		char currentlineislambda = 0;
		//find a lambda
		for (j = 0; j < MAXWORDS && words[j][0]; j++)
			if (words[j][0] == ';') {
				//switch scopes
				if (f) {
					//final stuff
					scopes[si++].f[f].name[0] = 0; //null terminator
					f = 0;
				}
				currentlineislambda = 1;
				break;
			}
		if (!f && !currentlineislambda) {
			fprintf(stderr, "the first line must be a lambda.\n");
			exit(1);
		}
		//put the inputs into scopes[si].f[i]
		for (j = 0; j < MAXVALS && words[j][0] >= 'a' && words[j][0] <= 'z'; j++)
			strcpy(scopes[si].f[f].ins[j], words[j]);
		//put the function name into scopes[si].f[f]
		if (!words[j]) {
			fprintf(stderr, "No function specified, line %i.\n", i + 1);
			exit(1);
		}
		strcpy(scopes[si].f[f].name, words[j++]);
		//put the outputs into scopes[si].f[f]
		for (k = 0; k < MAXVALS && words[j][0] >= 'a' && words[j][0] <= 'z'; k++)
			strcpy(scopes[si].f[f].outs[k], words[j++]);
		scopes[si].f[f++].satisfied = 0; //inputs are not yet satisfied. I don't know if it needs set.
	}
	//final stuff
	scopes[si].f[f].name[0] = 0; //null terminator
}

void printfunc(struct func f) {
	int i;
	dprint(f.name, "s");
	for (i = 0; f.ins[i][0]; i++)
		dprint(f.ins[i], "s");
	for (i = 0; f.outs[i][0]; i++)
		dprint(f.outs[i], "s");
}

//returns true if a function is fully satisfied (only used in makemain)
int issatisfied(struct func *f) {
	char i, sat = 0; //iterator, holds full potential satisfaction
	for (i = 0; i < MAXVALS && f->ins[i][0]; i++) //iterate inputs
		sat |= 1 << i; //calculate potential satisfaction
	if (sat == f->satisfied) //f is fully satisfied
		return 1;
	return 0;
}

//get the name before the "<" if there is a "<" and put it in *r
void namefrompipe(char *r, char *s) {
	strcpy(r, s);
	char *pos = strchr(r, '<');
	if (pos)
		r[pos - r] = '\0';
}

//update every func.satisfied flag for a pipe
void satisfy(char *program, struct scope scope, char *pipe) {
	char *pipename = malloc(WORDLEN); //name of pipe
	char *inname  = malloc(WORDLEN); //name of current in
	namefrompipe(pipename, pipe);
	int i, j;
	for (i = 1; scope.f[i].name[0]; i++) { //loop through scope.f
		for (j = 0; scope.f[i].ins[j][0] && j < MAXVALS; j++) { //loop through ins
			namefrompipe(inname, scope.f[i].ins[j]);
			if (!strcmp(inname, pipename)) { //matches
				//do other functions if they are ready
				scope.f[i].satisfied |= 1 << j;
				if (issatisfied(scope.f + i)) {
					scope.f[i].satisfied = 0;
					runfunc(program, scope, i);
				}
			}
		}
	}
	free(inname);
	free(pipename);
}

//returns a copy.
struct scope branchscope(struct scope old) {
	struct scope new;
	int i;
	for (i = 0; old.f[i].name[0]; i++) {
		strcpy(new.f[i].name, old.f[i].name);
		memcpy(new.f[i].ins, old.f[i].ins, sizeof(char [MAXVALS][WORDLEN]));
		memcpy(new.f[i].outs, old.f[i].outs, sizeof(char [MAXVALS][WORDLEN]));
		new.f[i].satisfied = old.f[i].satisfied;
	}
	sprintf(new.name, "%sn%i", old.name, ++old.subscopes);
	new.subscopes = 0;
}

//will run a function ucope->f[i] and put code into p
void runfunc(char *program, struct scope scope, int i) {
	printf("began runfunc\n");
	int j, k;
	char *line = malloc(LINELEN); //stores current line
	strcpy(line, "");
	if (scope.f[i].name[0] == '#' || scope.f[i].name[0] == '\'') { //is constant
		for (j = 0; scope.f[i].outs[j][0]; j++) { //iterate through outputs
			//do the "var = var = ..."
			namefrompipe(line + strlen(line), scope.f[i].outs[j]);
			strcat(line, " = ");
		}
		if (scope.f[i].name[0] == '#') {
			strcat(line, scope.f[i].name + 1);
		} else {
			strcat(line, "'");
			strcat(line, scope.f[i].name + 1);
			strcat(line, "'");
		}
		strcat(line, ";\n");
		strcat(program, line); //put the line in the program
		for (j = 0; scope.f[i].outs[j][0]; j++) //iterate through outputs
			satisfy(program, scope, scope.f[i].outs[j]); //satisfy the output
	} else if (scope.f[i].name[0] == '+'
			|| scope.f[i].name[0] == '-'
			|| scope.f[i].name[0] == '*'
			|| scope.f[i].name[0] == '/'
			|| scope.f[i].name[0] == '%') { //is a mathmatical operand.
		for (j = 0; scope.f[i].outs[j][0]; j++) { //iterate through outputs
			//do the "var = var = ..."
			namefrompipe(line + strlen(line), scope.f[i].outs[j]);
			strcat(line, " = ");
		}
		char op[4] = " x ";
		op[1] = scope.f[i].name[0]; //make " + " (as an example)
		for (j = 0; scope.f[i].ins[j][0]; j++) { //iterate through inputs
			//do the "val + val + ..."
			strcat(line, scope.f[i].ins[j]);
			if (scope.f[i].ins[j+1][0]) //if there is a next one
				strcat(line, op);
		}
		strcat(line, ";\n");
		strcat(program, line); //put the line in the program
		for (j = 0; scope.f[i].outs[j][0]; j++) //iterate through outputs
			satisfy(program, scope, scope.f[i].outs[j]); //satisfy the output
	} else if (scope.f[i].name[0] == '=' || scope.f[i].name[0] == '>') {
		strcat(line, "if (");
		strcat(line, scope.f[i].ins[0]);
		if (scope.f[i].name[0] == '=')
			strcat(line, " == ");
		else
			strcat(line, " > ");
		strcat(line, scope.f[i].ins[2]);
		strcat(line, ") {\n");
		struct scope same = branchscope(scope);
		satisfy(program, same, scope.f[i].outs[0]);
		strcat(line, "} else {\n");
		struct scope different = branchscope(scope);
		satisfy(program, different, scope.f[i].outs[1]);
		strcat(line, "}\n");
	} else if (scope.f[i].outs[1][0]) { //multiple outputs
		fprintf(stderr, "Multiple outputs are not yet supported.\n");
		exit(1);
		//TODO
	} else { //one output
		if (scope.f[i].outs[0][0]) {
			namefrompipe(line, scope.f[i].outs[0]);
			strcat(line, " = ");
		}
		//write out something to the effect of "type val = func(ins[0], ins[1], ...);\n"
		//or "val = func(ins[0], ins[1], ...);\n"
		//"func("
		if (scope.f[i].name[0] == '@')
			strcat(line, scope.f[i].name + 1);
		else
			strcat(line, scope.f[i].name);
		strcat(line, "(");
		for (j = 0; scope.f[i].ins[j][0]; j++) { //iterate through inputs
			strcat(line, scope.f[i].ins[j]); //put input
			if (scope.f[i].ins[j+1][0]) //if not the last input
				strcat(line, ", ");
		}
		strcat(line, ");\n");
		strcat(program, line); //put the line in the program
		if (scope.f[i].outs[0][0]) //check if there is an output
			satisfy(program, scope, scope.f[i].outs[0]); //satisfy the output
	}
	free(line);
}

//get type from a pipe, format: "pipe<type>", puts it in *r, unless it returns 1, in which case the type is not specified
int typefrompipe(char *r, char *s) {
	//get whatever's between <>'s
	char *posgt = strchr(s, '<');
	//if not found, return 1.
	if (!posgt)
		return 1;
	posgt++;
	size_t len = strchr(posgt, '>') - posgt;
	strncpy(r, posgt, len);
	r[len] = '\0';
	return 0;
}

//autotyping: give the type to *r. Takes scopes, scope #, function #, output #
void gettype(char *r, struct scope *scopes, int s, int f, int o) {
	if (typefrompipe(r, scopes[s].f[f].outs[o])) { //if type is specified, typefrompipe will handle it
		char *funcname = scopes[s].f[f].name;
		//automatically type:
		int i, j, k; //index of scopes, scopes[s].f, and scopes[s].f[f].ins/outs
		//look through outputs of functions to see if it's defined in the declaration
		//remember: lambda
		switch (funcname[0]) {
			case '\'':
				strcpy(r, "char");
				return;
			case '+':
			case '-':
			case '*':
			case '/':
			case '%':
			case '#':
				strcpy(r, "double");
				return;
		}
		for (i = 0; scopes[i].f[0].name[0]; i++) //iterate through scopes
			if (!strcmp(scopes[i].f[0].name, funcname)) { //if it is a matching lambda
				if (scopes[i].f[0].ins[1][0]) {
					eprint("multiple outputs not yet supported.\n");
					exit(0);
				}
				if (!typefrompipe(r, scopes[i].f[0].ins[o])) //if a type is found, we are done.
					return;
			}
		printf("No type specified for %s. Assuming double.\n", scopes[s].f[f].outs[o]);
		strcpy(r, "double /*No type found*/");
	}
}

//will give the "type var" or "var" depending on which is appropriate, and put it in *r.
//Takes scopes, scope #, function #, output #
void decl(char *r, struct scope *scopes, int s, int f, int o) {
	//put "type " if needed
	gettype(r, scopes, s, f, o);
	strcat(r, " ");
	namefrompipe(r + strlen(r), scopes[s].f[f].outs[o]);
}

//declare a function e.g. type func(type var), and put it in *r.
//Information is taken from scopes[s].f[0], mostly.
void declfunc(char *r, struct scope *scopes, int s) {
	int i, j;
	char ismain = !strcmp(";main", scopes[s].f[0].name);
	char *tempname = malloc(WORDLEN); //to store pipe names
	if (ismain)
		strcpy(r, "\nint "); //main always returns an int.
	else {
		strcpy(r, "\n");
		//put in type. typefrompipe will do the magic for us for the most part.
		if (typefrompipe(r + strlen(r), scopes[s].f[0].outs[0]))
			//typefrompipe is useless. It can't determine the type!
			//iterate through local functions, searching for a return value
			for (i = 1; scopes[s].f[i].name[0]; i++)
				for (j = 0; scopes[s].f[i].outs[j][0]; j++) {
					namefrompipe(tempname, scopes[s].f[i].outs[j]);
					if (!strcmp(tempname, scopes[s].f[0].outs[0])) //matches
						gettype(r + strlen(r), scopes, s, i, j);
				}
		strcat(r, " ");
	}
	strcat(r, scopes[s].f[0].name + 1); //don't copy the ; at the beginning
	strcat(r, "(");
	if (!ismain) {
		for (j = 0; scopes[s].f[0].ins[j][0]; j++) {
			//like decl, but for ins
			gettype(r + strlen(r), scopes, s, 0, j);
			strcat(r, " ");
			namefrompipe(r + strlen(r), scopes[s].f[0].ins[j]);
			if (scopes[s].f[0].ins[j+1][0])
				strcat(r, ", "); //last argument gets no comma.
		}
	}
	strcat(r, ")");
	free(tempname);
}

void allfuncs(char *program, struct scope *scopes) {
	int s, i, j;
	//initial function decl's
	for (s = 0; scopes[s].f[0].name[0]; s++) {
		declfunc(program + strlen(program), scopes, s);
		strcat(program, ";\n");
	}
	//actual function definitions
	for (s = 0; scopes[s].f[0].name[0]; s++) {
		declfunc(program + strlen(program), scopes, s);
		strcat(program, " {\n");
		//do declarations
		for (i = 1; scopes[s].f[i].name[0]; i++)
			for (j = 0; scopes[s].f[i].outs[j][0]; j++) {
				decl(program + strlen(program), scopes, s, i, j);
				strcat(program, ";\n");
			}
		//do functions without inputs
		for (i = 1; scopes[s].f[i].name[0]; i++)
			if (!scopes[s].f[i].ins[0][0]) {
				printfunc(scopes[s].f[i]);
				printf("beginning runfunc\n");
				runfunc(program, scopes[s], i);
				printf("ending runfunc\n");
			}
		//satisfy the lambda's arguments
		for (i = 0; scopes[s].f[0].ins[i][0]; i++)
			satisfy(program, scopes[s], scopes[s].f[0].ins[i]);
		//return value;
		if (scopes[s].f[0].outs[0][0]) { //if there are outputs
			strcat(program, "return ");
			strcat(program, scopes[s].f[0].outs[0]);
			strcat(program, ";\n");
		}
		strcat(program, "}\n");
	}
}

int main() {
	printf("main: started\n");
	char *flangprogram = malloc(MAXLINES * LINELEN);
	fread(flangprogram, sizeof(char), MAXLINES * LINELEN, stdin);
	printf("Inputted program is \"%s\"\n", flangprogram);
	char *program = malloc(MAXLINES * LINELEN);
	struct scope *scopes = calloc(sizeof(struct scope), MAXSCOPES);
	strcpy(program, "");
	parse(scopes, program, flangprogram);
	printf("main: finished reading\n");
	allfuncs(program, scopes);
	printf("main: output:\n%s", program);
	return 0;
}
