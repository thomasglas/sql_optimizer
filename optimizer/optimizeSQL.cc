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
#include "optimizer/ra_tree.h"

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
  "select name from students group by name having min(id)>2",
  "select name from students where name like '%glas'",
  "select name from students where name not like '%glas'",
  "select * from students, profs, exams, lectures",
  "select * from students where year = date '1999'",
  "select * from students where year < date '1999' + interval '3' day",
  "select substring(name from 1 for 2) from students",
  "select (1-id)*5 as foo from students",
  "select id from students where id between min(id) and 5",
  "select case when id='Thomas' then 'cool' else 'awesome' end as foo from students",
  "select distinct id from students",
  "select count(distinct id) from students",
  "select extract(year from _date) from students",
  "select s2.sid from (select s.id, s.name from students s) as s2(sid,firstname)",
  "select * from (students s left outer join professors p on s.id=p.id) as s2(col1, col2)",
  "with foo(a,b) as (select a,b from students) select * from foo",
};

std::vector<const char*> tests_correlated = {
  "select s.name from students s where s.id = (select min(id) from students where semester=s.semester)", 
  "select s.name from students s where exists (select * from exams e where s.id=e.sid)",
  "select s.name from students s where not exists (select * from exams e where s.id=e.sid)",
  // "select s.name from students s where s.id in (select e.sid from exams e where grade=1.0)",
  // "select s.name from students s where s.id not in (select e.sid from exams e where grade=1.0)",
};
  
std::vector<const char*> Q1Q2 = {
  /*Q1*/ "select s.name, e.course from students s,exams e where s.id=e.sid and e.grade=(select min(e2.grade) from exams e2 where s.id=e2.sid)",
  /*Q2*/ "select s.name, e.course from students s, exams e where s.id=e.sid and (s.major = 'CS' or s.major = 'Games Eng') and e.grade>=(select avg(e2.grade)+1 from exams e2 where s.id=e2.sid or (e2.curriculum=s.major and s.year>e2.date))"
};


std::vector<const char*> tpch_correlated = {
  /*Q2*/ "select s_acctbal, s_name, n_name, p_partkey, p_mfgr, s_address, s_phone, s_comment from part, supplier, partsupp, nation, region where p_partkey=ps_partkey and s_suppkey=ps_suppkey and p_size=42 and p_type like '%42' and s_nationkey=n_nationkey and n_regionkey=r_regionkey and r_name='42' and ps_supplycost=(select min(ps_supplycost) from partsupp, supplier, nation, region where p_partkey=ps_partkey and s_suppkey=ps_suppkey and s_nationkey=n_nationkey and n_regionkey=r_regionkey and r_name='42') order by s_acctbal desc, n_name, s_name, p_partkey",
  /*Q4*/ "select o_orderpriority, count(*) as order_count from orders where o_orderdate>=date '42' and o_orderdate < date '42'+interval '3' month and exists (select * from lineitem where l_orderkey=o_orderkey and l_commitdate<l_receiptdate ) group by o_orderpriority order by o_orderpriority",
  /*Q17*/ "select sum(l_extendedprice)/7.0 as avg_yearly from lineitem, part where p_partkey=l_partkey and p_brand='42' and p_container='42' and l_quantity<(select 0.2 * avg(l_quantity) from lineitem where l_partkey=p_partkey)",
  // /*Q20*/ "select s_name, s_address from supplier, nation where s_suppkey in (select ps_suppkey from partsupp where ps_partkey in (select p_partkey from part where p_name like '42%') and ps_availqty>(select 0.5*sum(l_quantity) from lineitem where l_partkey=ps_partkey and l_suppkey=ps_suppkey and l_shipdate>=date('42') and l_shipdate<date('42')+interval '1' year)) and s_nationkey=n_nationkey and n_name='42' order by s_name",
  /*Q21*/ "select s_name, count(*) as numwait from supplier, lineitem l1, orders, nation where s_suppkey=l1.l_suppkey and o_orderkey=l1.l_orderkey and o_orderstatus='F' and l1.l_receiptdate>l1.l_commitdate and exists (select * from lineitem l2 where l2.l_orderkey=l1.l_orderkey and l2.l_suppkey<>l1.l_suppkey) and not exists (select * from lineitem l3 where l3.l_orderkey=l1.l_orderkey and l3.l_suppkey<>l1.l_suppkey and l3.l_receiptdate>l3.l_commitdate) and s_nationkey=n_nationkey and n_name='42' group by s_name order by numwait desc, s_name",
  // /*Q22*/ "select cntrycode, count(*) as numcust, sum(c_acctbal) as totacctbal from (select substring(c_phone from 1 for 2) as cntrycode, c_acctbal from customer where substring(c_phone from 1 for 2) in ('[I1]','[I2]','[I3]','[I4]','[I5]','[I6]','[I7]') and c_acctbal>(select avg(c_acctbal) from customer where c_acctbal>0.00 and substring (c_phone from 1 for 2) in ('[I1]','[I2]','[I3]','[I4]','[I5]','[I6]','[I7]')) and not exists (select * from orders where o_custkey=c_custkey)) as custsale group by cntrycode order by cntrycode",
};


std::vector<const char*> tpch_uncorrelated = {
  // /*Q1*/ "select l_returnflag, l_linestatus, sum(l_quantity) as sum_qty, sum(l_extendedprice) as sum_base_price, sum(l_extendedprice * (1 - l_discount)) as sum_disc_price, sum(l_extendedprice * (1 - l_discount) * (1 + l_tax)) as sum_charge, avg(l_quantity) as avg_qty, avg(l_extendedprice) as avg_price, avg(l_discount) as avg_disc, count(*) as count_order from lineitem where l_shipdate <= date '1998-12-01' - interval '90' day group by l_returnflag, l_linestatus order by l_returnflag, l_linestatus",
  // /*Q3*/ "select l_orderkey, sum(l_extendedprice * (1 - l_discount)) as revenue, o_orderdate, o_shippriority from customer, orders, lineitem where c_mktsegment = 'BUILDING' and c_custkey = o_custkey and l_orderkey = o_orderkey and o_orderdate < date '1995-03-15' and l_shipdate > date '1995-03-15' group by l_orderkey, o_orderdate, o_shippriority order by revenue desc, o_orderdate",
  // /*Q5*/ "select n_name, sum(l_extendedprice * (1 - l_discount)) as revenue from customer, orders, lineitem, supplier, nation, region where c_custkey = o_custkey and l_orderkey = o_orderkey and l_suppkey = s_suppkey and c_nationkey = s_nationkey and s_nationkey = n_nationkey and n_regionkey = r_regionkey and r_name = 'ASIA' and o_orderdate >= date '1994-01-01' and o_orderdate < date '1994-01-01' + interval '1' year group by n_name order by revenue desc",
  // /*Q6*/ "select sum(l_extendedprice * l_discount) as revenue from lineitem where l_shipdate >= date '1994-01-01' and l_shipdate < date '1994-01-01' + interval '1' year and l_discount between 0.06 - 0.01 and 0.06 + 0.01 and l_quantity < 24",
  // /*Q7*/ "select supp_nation, cust_nation, l_year, sum(volume) as revenue from ( select n1.n_name as supp_nation, n2.n_name as cust_nation, extract(year from l_shipdate) as l_year, l_extendedprice * (1 - l_discount) as volume from supplier, lineitem, orders, customer, nation n1, nation n2 where s_suppkey = l_suppkey and o_orderkey = l_orderkey and c_custkey = o_custkey and s_nationkey = n1.n_nationkey and c_nationkey = n2.n_nationkey and ( (n1.n_name = 'FRANCE' and n2.n_name = 'GERMANY') or (n1.n_name = 'GERMANY' and n2.n_name = 'FRANCE') ) and l_shipdate between date '1995-01-01' and date '1996-12-31' ) as shipping group by supp_nation, cust_nation, l_year order by supp_nation, cust_nation, l_year",
  // /*Q8*/ "select o_year, sum(case when nation = 'BRAZIL' then volume else 0 end) / sum(volume) as mkt_share from ( select extract(year from o_orderdate) as o_year, l_extendedprice * (1 - l_discount) as volume, n2.n_name as nation from part, supplier, lineitem, orders, customer, nation n1, nation n2, region where p_partkey = l_partkey and s_suppkey = l_suppkey and l_orderkey = o_orderkey and o_custkey = c_custkey and c_nationkey = n1.n_nationkey and n1.n_regionkey = r_regionkey and r_name = 'AMERICA' and s_nationkey = n2.n_nationkey and o_orderdate between date '1995-01-01' and date '1996-12-31' and p_type = 'ECONOMY ANODIZED STEEL' ) as all_nations group by o_year order by o_year",
  // /*Q9*/ "select nation, o_year, sum(amount) as sum_profit from ( select n_name as nation, extract(year from o_orderdate) as o_year, l_extendedprice * (1 - l_discount) - ps_supplycost * l_quantity as amount from part, supplier, lineitem, partsupp, orders, nation where s_suppkey = l_suppkey and ps_suppkey = l_suppkey and ps_partkey = l_partkey and p_partkey = l_partkey and o_orderkey = l_orderkey and s_nationkey = n_nationkey and p_name like '%green%' ) as profit group by nation, o_year order by nation, o_year desc",
  // /*Q10*/ "select c_custkey, c_name, sum(l_extendedprice * (1 - l_discount)) as revenue, c_acctbal, n_name, c_address, c_phone, c_comment from customer, orders, lineitem, nation where c_custkey = o_custkey and l_orderkey = o_orderkey and o_orderdate >= date '1993-10-01' and o_orderdate < date '1993-10-01' + interval '3' month and l_returnflag = 'R' and c_nationkey = n_nationkey group by c_custkey, c_name, c_acctbal, c_phone, n_name, c_address, c_comment order by revenue desc",
  // Q11 is denested
  // /*Q11*/ "select ps_partkey, sum(ps_supplycost * ps_availqty) as value from partsupp, supplier, nation where ps_suppkey = s_suppkey and s_nationkey = n_nationkey and n_name = 'GERMANY' group by ps_partkey having sum(ps_supplycost * ps_availqty) > ( select sum(ps_supplycost * ps_availqty) * 0.0001 from partsupp, supplier, nation where ps_suppkey = s_suppkey and s_nationkey = n_nationkey and n_name = 'GERMANY' ) order by value desc",
  // Q12 has in
  // // /*Q12*/ "select l_shipmode, sum(case when o_orderpriority = '1-URGENT' or o_orderpriority = '2-HIGH' then 1 else 0 end) as high_line_count, sum(case when o_orderpriority <> '1-URGENT' and o_orderpriority <> '2-HIGH' then 1 else 0 end) as low_line_count from orders, lineitem where o_orderkey = l_orderkey and l_shipmode in ('MAIL', 'SHIP') and l_commitdate < l_receiptdate and l_shipdate < l_commitdate and l_receiptdate >= date '1994-01-01' and l_receiptdate < date '1994-01-01' + interval '1' year group by l_shipmode order by l_shipmode",
  // /*Q13*/ "select c_count, count(*) as custdist from ( select c_custkey, count(o_orderkey) from customer left outer join orders on c_custkey = o_custkey and o_comment not like '%special%requests%' group by c_custkey ) as c_orders (c_custkey, c_count) group by c_count order by custdist desc, c_count desc",
  // /*Q14*/ "select 100.00 * sum(case when p_type like 'PROMO%' then l_extendedprice * (1 - l_discount) else 0 end) / sum(l_extendedprice * (1 - l_discount)) as promo_revenue from lineitem, part where l_partkey = p_partkey and l_shipdate >= date '1995-09-01' and l_shipdate < date '1995-09-01' + interval '1' month",
  // Q15 cte code replacement
  /*Q15*/ "with revenue (supplier_no, total_revenue) as ( select l_suppkey, sum(l_extendedprice * (1 - l_discount)) from lineitem where l_shipdate >= date '1996-01-01' and l_shipdate < date '1996-01-01' + interval '3' month group by l_suppkey) select s_suppkey, s_name, s_address, s_phone, total_revenue from supplier, revenue where s_suppkey = supplier_no and total_revenue = ( select max(total_revenue) from revenue ) order by s_suppkey",
  // /*Q16*/ "",
  // /*Q18*/ "",
  // /*Q19*/ "",
};

void parse_json(){

  PgQueryParseResult result;

  for (size_t i = 0; i < tests.size(); i++) {
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
    SQLtoRA* sql_to_ra = new SQLtoRA();
    RaTree* raTree = sql_to_ra->parse(test);
    std::cout << raTree->root->to_string() <<std::endl;
    RAtoSQL* ra_to_sql = new RAtoSQL(raTree);
    std::string sql = ra_to_sql->deparse();
    std::cout << sql << std::endl;
  }
}

void run_tests_correlated(){
  std::cout << "\n===== correlated tests =====" << std::endl;
  for(auto test: tests_correlated){
    SQLtoRA* sql_to_ra = new SQLtoRA();
    RaTree* raTree = sql_to_ra->parse(test);
    std::cout << raTree->root->to_string() << "\n" << std::endl;
  }
}

void run_q1q2(){
  std::cout << "\n===== Q1Q2 tests =====" << std::endl;
  for(auto test: Q1Q2){
    SQLtoRA* sql_to_ra = new SQLtoRA();
    RaTree* raTree = sql_to_ra->parse(test);
    std::cout << raTree->root->to_string() << "\n" << std::endl;
  }
}

void run_tpch(){
  std::cout << "\n===== TPCH tests =====" << std::endl;
  for(auto test: tpch_correlated){
    SQLtoRA* sql_to_ra = new SQLtoRA();
    RaTree* raTree = sql_to_ra->parse(test);
    std::cout << raTree->root->to_string() << "\n" << std::endl;
  }
}

void run_tpch_uncorrelated(){
  std::cout << "\n===== TPCH tests =====" << std::endl;
  for(auto test: tpch_uncorrelated){
    SQLtoRA* sql_to_ra = new SQLtoRA();
    RaTree* raTree = sql_to_ra->parse(test);
    std::cout << raTree->root->to_string() << "\n" << std::endl;
    RAtoSQL* ra_to_sql = new RAtoSQL(raTree);
    std::string sql = ra_to_sql->deparse();
    std::cout << sql << std::endl;
  }
}

void deparse_protobuf(const char* test){
  PgQueryProtobufParseResult result = pg_query_parse_protobuf(test);
  PgQueryDeparseResult deparsed_result = pg_query_deparse_protobuf(result.parse_tree);
  printf("%s\n", deparsed_result.query);

  pg_query_free_deparse_result(deparsed_result);
  pg_query_free_protobuf_parse_result(result);
}

int main() {
  run_tests();
  // run_tests_correlated();
  // run_q1q2();
  // run_tpch();
  run_tpch_uncorrelated();
  // parse_json();
  // Optional, this ensures all memory is freed upon program exit (useful when running Valgrind)
  pg_query_exit();

  return 0;
}
