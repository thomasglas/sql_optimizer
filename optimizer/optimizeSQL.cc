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
  "select s.name from students s where s.id = (select min(s2.id) from students s2 where s2.id>1)", 
  "select s.name from students s where s.id = (select min(s2.id) from students s2 where s2.semester=s.semester)", 
  "select * from students where a=1 and (b=2 or c=3)",
  "select grade from exams group by sid, name",
  "select sid from exams order by grade asc, semester desc",
  "select -id from students",

  // "select s.name from students s where exists (select * from exams e where s.id=e.sid)",
  /* = select s.name from students s, (select distinct sid from exams) e where s.id = e.sid*/
  // "select s.name from students s where not exists (select * from exams e where s.id=e.sid)",
  /* = select s.name from students s left join (select distinct sid from exams) e on s.id = e.sid where e.sid is null*/
  // "select s.name from students s where s.id in (select e.sid from exams e where grade=1.0)",
  /* select s.name from students s where exists (select e.sid from exams e where grade=1.0 and s.id=e.sid)*/
  /* select s.name from students s, (select distinct sid from exams where grade=1.0) e where s.id=e.sid*/
};
  
std::vector<const char*> tests2 = {
  // /*Q1*/ "select s.name,e.course from students s,exams e where s.id=e.sid and e.grade=(select min(e2.grade) from exams e2 where s.id=e2.sid)",
  /*Q2*/ "select s.name, e.course from students s, exams e where s.id=e.sid and (s.major = 'CS' or s.major = 'Games Eng') and e.grade>=(select avg(e2.grade)+1 from exams e2 where s.id=e2.sid or (e2.curriculum=s.major and s.year>e2.date))"
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
  for(auto test: tests2){
    Ra__Node* ra_root = parse_sql_query(test);
    std::string sql = deparse_ra(ra_root);
    std::cout << sql << std::endl;
  }
  // parse_json();
  // Optional, this ensures all memory is freed upon program exit (useful when running Valgrind)
  pg_query_exit();

  return 0;
}
