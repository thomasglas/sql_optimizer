select o_orderpriority, count(*) as order_count
from orders
where exists (select *
from lineitem
where l_orderkey=o_orderkey and l_commitdate<l_receiptdate
) and (o_orderdate>=date '1993-07-01' and o_orderdate<(date '1993-07-01'+interval '3' month))
group by o_orderpriority
order by o_orderpriority;
