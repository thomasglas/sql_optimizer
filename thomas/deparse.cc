// Welcome to the easiest way to parse an SQL query :-)
// Compile the file like this:
//
// cc -I../ -I../src/postgres/include/ -I../vendor/ -L../ deparse.c -lpg_query -o deparse

#include <pg_query.h>
#include <stdio.h>
#include <stdlib.h>
#include "src/pg_query_readfuncs.h"
#include "protobuf/pg_query.pb-c.h"
#include "protobuf/pg_query.pb.h"

size_t testCount = 1;
const char* tests[] = {
  "SELECT s.name, s.id from students s"
};

void parse_json(){

  PgQueryParseResult result;
  size_t i;

  for (i = 0; i < testCount; i++) {
    result = pg_query_parse(tests[i]);

    if (result.error) {
      printf("error: %s at %d\n", result.error->message, result.error->cursorpos);
    } else {
      printf("%s\n", result.parse_tree);
    }

    pg_query_free_parse_result(result);
  }
}

void parse_deparse_query(){

  PgQueryProtobufParseResult result;
  size_t i;

  for (i = 0; i < testCount; i++) {
    result = pg_query_parse_protobuf(tests[i]);

    PgQueryDeparseResult deparsed_result = pg_query_deparse_protobuf(result.parse_tree);
    printf("%s\n", deparsed_result.query);

    pg_query_free_deparse_result(deparsed_result);
    pg_query_free_protobuf_parse_result(result);
  }
}

void print_relation_name(){


  printf("Print relation name:\n");

  PgQueryProtobufParseResult result;  

  result = pg_query_parse_protobuf(tests[0]);
  PgQuery__ParseResult* parse_result;
  parse_result = pg_query__parse_result__unpack(NULL, result.parse_tree.len, (void *) result.parse_tree.data);
  PgQuery__RawStmt* raw_stmt = parse_result->stmts[0];

  PgQuery__Node* stmt = raw_stmt->stmt;
  PgQuery__Node__NodeCase node_case = stmt->node_case;
  PgQuery__SelectStmt* select_stmt = stmt->select_stmt;
  PgQuery__Node* from_clause = *(select_stmt->from_clause);
  PgQuery__Node__NodeCase from_node_case = from_clause->node_case;
  PgQuery__RangeVar* from_range_var = from_clause->range_var;
  char* relname = from_range_var->relname;

  printf("%s\n", relname);
}


void generate_protobuf(){
  // create a new protobuf and deparse it
  // final product: PgQueryProtobuf
  // PgQueryProtobuf = packed PgQuery__ParseResult 

  // PgQueryProtobuf* result;
  // PgQuery__ParseResult* parse_result;
  // pg_query__parse_result__init(parse_result);
  // parse_result->stmts

  pg_query::ParseResult parseResult;

}

int main() {
  generate_protobuf();
  // Optional, this ensures all memory is freed upon program exit (useful when running Valgrind)
  pg_query_exit();

  return 0;
}
