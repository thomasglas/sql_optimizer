with cte_2(l_orderkey,l_suppkey) as (
select l3.l_orderkey, l3.l_suppkey
from lineitem l3
where l3.l_receiptdate>l3.l_commitdate
),cte_3(l_orderkey,l_suppkey) as (
select l2.l_orderkey, l2.l_suppkey
from lineitem l2
)
select s_name, count(*) as numwait
from nation, orders, lineitem l1, supplier
where exists (select *
from cte_3
where cte_3.l_orderkey=l1.l_orderkey and cte_3.l_suppkey<>l1.l_suppkey
) and (not exists (select *
from cte_2
where cte_2.l_orderkey=l1.l_orderkey and cte_2.l_suppkey<>l1.l_suppkey
)) and (s_suppkey=l1.l_suppkey and s_nationkey=n_nationkey) and o_orderkey=l1.l_orderkey and n_name='SAUDI ARABIA' and o_orderstatus='F' and l1.l_receiptdate>l1.l_commitdate
group by s_name
order by numwait desc, s_name;
