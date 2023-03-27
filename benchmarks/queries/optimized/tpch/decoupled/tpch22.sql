with cte_2(o_custkey) as (
select o_custkey
from orders
)
select cntrycode, count(*) as numcust, sum(c_acctbal) as totacctbal
from (select substring(c_phone from 1 for 2) as cntrycode, c_acctbal
from customer
where c_acctbal>(select avg(c_acctbal)
from customer
where c_acctbal>0.00 and substring(c_phone from 1 for 2) in ('13','31','23','29','30','18','17')
) and (not exists (select *
from cte_2
where cte_2.o_custkey=c_custkey
)) and substring(c_phone from 1 for 2) in ('13','31','23','29','30','18','17')
) as custsale
group by cntrycode
order by cntrycode;
