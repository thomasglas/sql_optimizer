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
  "SELECT max(s.id, foo), min(s.id+4), max(s.id)+4 from students s",
  "select s.name from students s where s.id = (select min(s2.id) from students s2 where s2.id>1)", 
  "select * from students where a=1 and (b=2 or c=3)",
  "select grade from exams group by sid, name having sid=1",
  "select sid from exams order by grade asc, semester desc",
  "select -id from students",
  "select name from students having sid=2",
  "select name from students where name like '%glas'",
  "select * from students, profs, exams, lectures",
  "select * from students where year = date '1999'",
  "select * from students where year < date '1999' + interval '3' day",
};

std::vector<const char*> tests_correlated = {
  "select s.name from students s where s.id = (select min(id) from students where semester=s.semester)", 
  "select s.name from students s where exists (select * from exams e where s.id=e.sid)",
  "select s.name from students s where not exists (select * from exams e where s.id=e.sid)",
  // "select s.name from students s where s.id in (select e.sid from exams e where grade=1.0)",
};
  
std::vector<const char*> Q1Q2 = {
  /*Q1*/ "select s.name, e.course from students s,exams e where s.id=e.sid and e.grade=(select min(e2.grade) from exams e2 where s.id=e2.sid)",
  /*Q2*/ "select s.name, e.course from students s, exams e where s.id=e.sid and (s.major = 'CS' or s.major = 'Games Eng') and e.grade>=(select avg(e2.grade)+1 from exams e2 where s.id=e2.sid or (e2.curriculum=s.major and s.year>e2.date))"
};


std::vector<const char*> tpch = {
  /*Q2*/ "select s_acctbal, s_name, n_name, p_partkey, p_mfgr, s_address, s_phone, s_comment from part, supplier, partsupp, nation, region where p_partkey=ps_partkey and s_suppkey=ps_suppkey and p_size=42 and p_type like '%42' and s_nationkey=n_nationkey and n_regionkey=r_regionkey and r_name='42' and ps_supplycost=(select min(ps_supplycost) from partsupp, supplier, nation, region where p_partkey=ps_partkey and s_suppkey=ps_suppkey and s_nationkey=n_nationkey and n_regionkey=r_regionkey and r_name='42') order by s_acctbal desc, n_name, s_name, p_partkey",
  /*Q4*/ "select o_orderpriority, count(*) as order_count from orders where o_orderdate>=date '42' and o_orderdate < date '42'+interval '3' month and exists (select * from lineitem where l_orderkey=o_orderkey and l_commitdate<l_receiptdate ) group by o_orderpriority order by o_orderpriority",
  /*Q17*/ "select sum(l_extendedprice)/7.0 as avg_yearly from lineitem, part where p_partkey=l_partkey and p_brand='42' and p_container='42' and l_quantity<(select 0.2 * avg(l_quantity) from lineitem where l_partkey=p_partkey)",
  // /*Q20*/ "select s_name, s_address from supplier, nation where s_suppkey in (select ps_suppkey from partsupp where ps_partkey in (select p_partkey from part where p_name like '42%') and ps_availqty>(select 0.5*sum(l_quantity) from lineitem where l_partkey=ps_partkey and l_suppkey=ps_suppkey and l_shipdate>=date('42') and l_shipdate<date('42')+interval '1' year)) and s_nationkey=n_nationkey and n_name='42' order by s_name",
  /*Q21*/ "select s_name, count(*) as numwait from supplier, lineitem l1, orders, nation where s_suppkey=l1.l_suppkey and o_orderkey=l1.l_orderkey and o_orderstatus='F' and l1.l_receiptdate>l1.l_commitdate and exists (select * from lineitem l2 where l2.l_orderkey=l1.l_orderkey and l2.l_suppkey<>l1.l_suppkey) and not exists (select * from lineitem l3 where l3.l_orderkey=l1.l_orderkey and l3.l_suppkey<>l1.l_suppkey and l3.l_receiptdate>l3.l_commitdate) and s_nationkey=n_nationkey and n_name='42' group by s_name order by numwait desc, s_name",
  // /*Q22*/ "select cntrycode, count(*) as numcust, sum(c_acctbal) as totacctbal from (select substring(c_phone from 1 for 2) as cntrycode, c_acctbal from customer where substring(c_phone from 1 for 2) in ('[I1]','[I2]','[I3]','[I4]','[I5]','[I6]','[I7]') and c_acctbal>(select avg(c_acctbal) from customer where c_acctbal>0.00 and substring (c_phone from 1 for 2) in ('[I1]','[I2]','[I3]','[I4]','[I5]','[I6]','[I7]')) and not exists (select * from orders where o_custkey=c_custkey)) as custsale group by cntrycode order by cntrycode",
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

void run_tests(){
  std::cout << "\n===== tests =====" << std::endl;
  for(auto test: tests){
    Ra__Node* ra_root = parse_sql_query(test);
    std::cout << ra_root->to_string() <<std::endl;
    std::string sql = deparse_ra(ra_root);
    std::cout << sql << std::endl;
  }
}

void run_tests_correlated(){
  std::cout << "\n===== correlated tests =====" << std::endl;
  for(auto test: tests_correlated){
    Ra__Node* ra_root = parse_sql_query(test);
    std::cout << ra_root->to_string() << "\n" << std::endl;
  }
}

void run_q1q2(){
  std::cout << "\n===== Q1Q2 tests =====" << std::endl;
  for(auto test: Q1Q2){
    Ra__Node* ra_root = parse_sql_query(test);
    std::cout << ra_root->to_string() << "\n" << std::endl;
  }
}

void run_tpch(){
  std::cout << "\n===== TPCH tests =====" << std::endl;
  for(auto test: tpch){
    Ra__Node* ra_root = parse_sql_query(test);
    std::cout << ra_root->to_string() << "\n" << std::endl;
  }
}

int main() {
  // run_tests();
  // run_tests_correlated();
  // run_q1q2();
  run_tpch();
  // parse_json();
  // Optional, this ensures all memory is freed upon program exit (useful when running Valgrind)
  pg_query_exit();

  return 0;
}
