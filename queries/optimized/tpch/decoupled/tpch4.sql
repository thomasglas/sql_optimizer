with cte_1(l_orderkey) as (
select l_orderkey
from lineitem
where l_commitdate<l_receiptdate
)
select o_orderpriority, count(*) as order_count
from orders
where exists (select *
from cte_1
where cte_1.l_orderkey=o_orderkey
) and (o_orderdate>=date '1993-07-01' and o_orderdate<(date '1993-07-01'+interval '3' month))
group by o_orderpriority
order by o_orderpriority;
