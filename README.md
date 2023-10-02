# SQL optimizer

## What is this project?
This tool is a C++ implementation of an SQL optimizer. It parses SQL (with [PostgreSQL parser](https://github.com/pganalyze/libpg_query)), builds a relational algebra tree, performs optimizations, and deparses the optimized relational algebra tree back to SQL. This tool aims to bring advanced SQL optimation techniques to all database systems, which may have not implemented these optimizations internally. The initial focus is on unnesting correlated subqueries, using the algorithm described in the paper ["Unnesting Arbitrary Queries"](https://www.semanticscholar.org/paper/Unnesting-Arbitrary-Queries-Neumann-Kemper/3112928019f64d8c388e8cfbae34b9887c789213). See "SQL Unnesting.pptx" for the slides of the final presentation of the project.

## Preliminary Results
Following TPCH queries contain correlated subqueries, which lead to extremely long execution times, if a database's optimizer cannot decorrelate them. The benchmarks were run on PostgreSQL, whose optimizer is not able to decorrelate these queries. This tool has generated optimized forms for these queries, which result in significant performance improvements. 

| TPCH Query  | Original Query    | Optimized Query   |
| ------------|-------------------| ------------------|
| Q2          | 197.706 ms        | 140.944ms         |
| Q17         | >10min (timeout)  | 2438.320ms        |
| Q20         | >10min (timeout)  | 1546.563ms        |

## Example
The original TPCH Q20:
```
-- TPC-H Query 20
select
        s_name,
        s_address
from
        supplier,
        nation
where
        s_suppkey in (
                select
                        ps_suppkey
                from
                        partsupp
                where
                        ps_partkey in (
                                select
                                        p_partkey
                                from
                                        part
                                where
                                        p_name like 'forest%'
                        )
                        and ps_availqty > (
                                select
                                        0.5 * sum(l_quantity)
                                from
                                        lineitem
                                where
                                        l_partkey = ps_partkey
                                        and l_suppkey = ps_suppkey
                                        and l_shipdate >= date '1994-01-01'
                                        and l_shipdate < date '1994-01-01' + interval '1' year
                        )
        )
        and s_nationkey = n_nationkey
        and n_name = 'CANADA'
order by
        s_name;
```

is transformed by this tool into the following optimized form:
```
-- Optimized TPC-H Query 20
select 
        s_name, 
        s_address
from 
        nation, 
        supplier
where 
        s_suppkey in (
                select 
                        ps_suppkey
                from 
                        partsupp, 
                        (
                                select 
                                        (0.5*sum(l_quantity)), 
                                        l_partkey, 
                                        l_suppkey
                                from 
                                        lineitem
                                where 
                                        l_shipdate>=date '1994-01-01' 
                                        and l_shipdate<(date '1994-01-01'+interval '1' year)
                                group by l_partkey, l_suppkey
                        ) as t3(m, t3_ps_partkey, t3_ps_suppkey)
                where 
                        ps_partkey in (
                                select
                                        p_partkey
                                from 
                                        part
                                where 
                                        p_name like 'forest%'
                        ) 
                        and ps_availqty>t3.m 
                        and ps_partkey=t3.t3_ps_partkey 
                        and ps_suppkey=t3.t3_ps_suppkey
                        and s_nationkey=n_nationkey 
                        and n_name='CANADA'
        )
order by s_name;
``` 
