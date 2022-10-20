// Welcome to the easiest way to parse an SQL query :-)
// Compile the file like this:
//
// g++ -g -o optimizeSQL -I../ -I../src/postgres/include/ -I../vendor/ -I../src/ -L../ optimizeSQL.cc  ../src/optimizer/*.cc -lpg_query -pthread

#include <pg_query.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "protobuf/pg_query.pb-c.h"
#include "optimizer/relational_algebra.h"
#include "optimizer/parse_sql_to_ra.h"
#include "optimizer/deparse_ra_to_sql.h"

size_t testCount = 1;
std::vector<const char*> tests = {
  "SELECT s.name, s.id from students s, exams e where s.id=1 and s.name='Thomas' or not s.avg>2.0",
  "SELECT s.id from students s where 1=1",
  "SELECT s.id as foo from students s",
  "SELECT * from students",
  "SELECT e.grade from exams e, students s where e.sid=s.id",
  "SELECT 5, 5.0, 'Thomas'" ,
  "SELECT s.id+5-2 from students s where s.id-4<>1",
  "SELECT max(s.id), min(s.id+4), max(s.id)+4 from students s",
  "select s.name from students s where s.id = (select min(s2.id) from students s2)", // or/and s.id = (select max(s3.id) from students s3)
  "select * from students where a=1 and (b=2 or c=3)"
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

int main() {
  for(auto test: tests){
    Ra__Node* ra_root = parse_query(test);
    std::string sql = deparse_ra(ra_root);
    std::cout << sql << std::endl;
  }
  // parse_json();
  // Optional, this ensures all memory is freed upon program exit (useful when running Valgrind)
  pg_query_exit();

  return 0;
}
