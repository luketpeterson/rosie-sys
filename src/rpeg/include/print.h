/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  print.h                                                                  */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */


void walk_instructions (Instruction *p,
			int codesize,
			void *(*operation)(const Instruction *, Instruction *, void *),
			void *context);

void print_ktable (Ktable *kt);
void *print_instruction (const Instruction *op, Instruction *p, void *context);
void print_instructions (Instruction *p, int codesize);

